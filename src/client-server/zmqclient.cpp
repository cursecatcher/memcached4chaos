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

zmqClient::zmqClient(unsigned nvirtualclient, unsigned time_to_run) {
    this->nvirtualclient = nvirtualclient;
    this->ttr = time_to_run;

    this->stopped = false;
    this->runned = false;

    this->context = new zmq::context_t(1);
    this->sockets = new zmq::socket_t*[nvirtualclient];

    this->tids = new pthread_t[nvirtualclient];
    pthread_cond_init(&this->cond, NULL);
    pthread_mutex_init(&this->mutex, NULL);
}

zmqClient::~zmqClient() {
    delete(this->tids);
    delete(this->sockets);
    delete(this->context);
}

void zmqClient::connect(const char *address) {
    for (int i = 0; i < (int) this->nvirtualclient; i++) {
        this->sockets[i] = new zmq::socket_t(*(this->context), ZMQ_REQ);
        this->sockets[i]->connect(address);
    }
}

void zmqClient::work() {
    thread_param_t params;

    params.client_class = this;

    for (params.nsocket = 0; params.nsocket < (int) this->nvirtualclient; params.nsocket++) {
        if (pthread_create(&this->tids[params.nsocket], NULL, client_thread, &params)) {
            std::cout << "Cannot create thread #" << params.nsocket << std::endl;
            abort();
        }
        pthread_cond_wait(&this->cond, &this->mutex);
    }

#ifdef VERBOSE
    std::cout << "Init completed" << std::endl;
#endif

/*
    for (int i = 0; i < (int) this->nvirtualclient; i++) {
        pthread_join(this->tids[i], NULL);
    #ifdef VERBOSE
        std::cout << "Thread #" << i << " terminated" << std::endl;
    #endif
    } */

    sleep(this->ttr);
    abort();
}


void zmqClient::worker(int nsocket) {
    zmq::socket_t *socket = this->sockets[nsocket];
#ifdef VERBOSE
    std::cout << "Created thread #" << nsocket << std::endl;
#endif
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


    do {
        req = new datarequested(pbuffer, key, keylen, NULL, 0, CODE_OP_GET_VALUE_BY_KEY);
        zmq::message_t request(req->size());
        memcpy(request.data(), pbuffer, req->size());
        socket->send(request);

        zmq::message_t reply;
        socket->recv(&reply);
        rep = new datareplied(reply.size(), reply.data());

        //do something with reply
        delete(req);
        delete(rep);

        std::cout << "Received by thread #" << nsocket << std::endl;
    } while (!this->stopped);
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

    client->connect("tcp://localhost:5555");
    client->work();

    return 0;
}
