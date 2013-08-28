#include "zmqclient.hpp"

typedef struct {
    zmqClient *client_class;
    int nsocket;
} thread_param_t;


void *thread_stopper(void* arg) {
    ((zmqClient*) arg)->stop();
    return NULL;
}

void *client_thread(void* arg) {
    thread_param_t *p = (thread_param_t*) arg;
    p->client_class->worker(p->nsocket);
    return NULL;
}

zmqClient::zmqClient(int nvirtualclient, int time_to_run, string address) {
    this->nvirtualclient = nvirtualclient;
    this->ttr = time_to_run;

    this->context = new zmq::context_t(1);
    this->sockets = new zmq::socket_t*[nvirtualclient];
    this->address = address;

    this->tids = new pthread_t[nvirtualclient];
    pthread_cond_init(&this->cond, NULL);
}


void zmqClient::connect() {
    for (int i = 0; i < (int) this->nvirtualclient; i++) {
        this->sockets[i] = new zmq::socket_t(*(this->context), ZMQ_REQ);
        this->sockets[i]->connect(this->address.c_str());
    }
}

void zmqClient::work() {
    thread_param_t params = {this, 0};
    pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

    for (; params.nsocket < (int) this->nvirtualclient; params.nsocket++) {
        if (pthread_create(&this->tids[params.nsocket], NULL, client_thread, &params)) {
            std::cout << "Cannot create thread #" << params.nsocket << std::endl;
            abort();
        }
        pthread_cond_wait(&this->cond, &mutex);
    }

    pthread_t tid;

    if (pthread_create(&tid, NULL, thread_stopper, this)) {
        std::cout << "Cannot create control thread" << std::endl;
        abort();
    }

    pthread_join(tid, NULL);
}


void zmqClient::worker(int nsocket) {
    zmq::socket_t *socket = this->sockets[nsocket];
    pthread_cond_signal(&this->cond);

    char key[250+1];
    char value[1024];
    int keylen;
    int valuelen;

    char buffer[1024];
    void *pbuffer = (void*) buffer;
    datarequested *req;
    datareplied *rep;

    strcpy(key, "giorgio");     keylen = strlen(key);
    strcpy(value, "mastrota");  valuelen = strlen(value);

    while (true) {
        req = nsocket == 0 ?
              new datarequested(pbuffer, key, keylen, value, valuelen, CODE_OP_SET_KEY_VALUE) :
              new datarequested(pbuffer, key, keylen, NULL, 0, CODE_OP_GET_VALUE_BY_KEY);

        zmq::message_t request(req->size());
        memcpy(request.data(), pbuffer, req->size());
        socket->send(request);

        zmq::message_t reply;
        socket->recv(&reply);
        rep = new datareplied(reply.size(), reply.data());

        //do something with reply
        delete(req);
        delete(rep);
    }
}

void zmqClient::stop() {
    sleep(this->ttr);

    zmq::socket_t socket(*(this->context), ZMQ_REQ);
    socket.connect(this->address.c_str());

    char buffer[32]; //basta e avanza
    datarequested terminator(buffer, NULL, 0, NULL, 0, CODE_OP_SHUT_DOWN);

    zmq::message_t request(terminator.size());
    memcpy(request.data(), buffer, terminator.size());
    socket.send(request);

    zmq::message_t reply;
    socket.recv(&reply);

    for (int i = 0; i < this->nvirtualclient; i++)
        pthread_cancel(this->tids[i]);
}

int main(int argc, char *argv[]) {
    int numclient, ttl;

    if (argc != 3 || sscanf(argv[1], "%d", &numclient) != 1 || sscanf(argv[2], "%d", &ttl) != 1) {
        std::cerr << "Usage: " << argv[0] << "'num_client' 'time_to_run'" << std::endl;
        return -1;
    }
    if (numclient < 1 || ttl < 1) {
        std::cerr << "Parameters must be greater than zero" << std::endl;
        return -2;
    }

    zmqClient *client = new zmqClient((unsigned) numclient, ttl);

    client->connect();
    client->work();

    return 0;
}
