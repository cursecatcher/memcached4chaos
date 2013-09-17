#include "zmqcache.hpp"
#include <iostream>

/* creare header per le risposte ai client:
 * ************************************************
 * - tipo operazione
 * - successo operazione
 * - byte usati per l'eventuale campo variabile
 * ************************************************
 * - campo variabile */

typedef struct {
    zmqServer *server_class;
    int nsocket;
} thread_param_t;


void *server_thread(void *arg) {
    thread_param_t *params = (thread_param_t *) arg;
    params->server_class->worker(params->nsocket);
    return NULL;
}

zmqServer::zmqServer(int nworkerthreads) {
    this->nworkerthreads = nworkerthreads;

    this->context = new zmq::context_t(1);
    this->sockets = new zmq::socket_t*[nworkerthreads];
    this->tids = new pthread_t[nworkerthreads];
    this->cache = new DataCache();

    pthread_cond_init(&this->cond, NULL);
    pthread_mutex_init(&this->mutex_nreq, NULL);
    this->num_req = 0;
}

void zmqServer::work() {
    thread_param_t params;
    pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

    zmq::socket_t clients(*(this->context), ZMQ_ROUTER);
    zmq::socket_t workers(*(this->context), ZMQ_DEALER);
    clients.bind("tcp://*:5555");
    workers.bind("inproc://workers");

    params.server_class = this;
    for (params.nsocket = 0; params.nsocket < this->nworkerthreads; params.nsocket++) {
        this->sockets[params.nsocket] = new zmq::socket_t(*(this->context), ZMQ_REP);
        this->sockets[params.nsocket]->connect("inproc://workers");

        if (pthread_create(&this->tids[params.nsocket], NULL, server_thread, &params)) {
            std::cerr << "Cannot create thread #" << params.nsocket << std::endl;
            abort();
        }
        pthread_cond_wait(&this->cond, &mutex);
    }

    zmq::proxy(clients, workers, NULL);
}

void zmqServer::worker(int nsocket) {
    zmq::socket_t *socket = this->sockets[nsocket];
    pthread_cond_signal(&this->cond);

    char cachebuffer[1024]; //preallocated
    char replybuffer[1024];
    void *pcachebuffer = (void*) cachebuffer;
    void *preplybuffer = (void*) replybuffer;
    int32_t byte_used = 0;
    datarequested *req = NULL;
    datareplied *rep = NULL;
    int success_op = false;
    bool closeall = false;

    while (!closeall) {
        zmq::message_t recv_message;
        socket->recv(&recv_message);
        req = new datarequested(recv_message.size(), recv_message.data());

        switch (req->op_code()) {
            case CODE_OP_GET_VALUE_BY_KEY:
                success_op = this->cache->getItem(req->key(), byte_used, &pcachebuffer);
                rep = new datareplied(preplybuffer, pcachebuffer, (size_t) byte_used, CODE_OP_GET_VALUE_BY_KEY, (success == 0));
                break;
            case CODE_OP_SET_KEY_VALUE:
                success_op = this->cache->storeItem(req->key(), req->data(), req->datasize());
                rep = new datareplied(preplybuffer, (void*) NULL, 0, CODE_OP_SET_KEY_VALUE, (success == 0));
                break;
            case CODE_OP_DELETE_BY_KEY:
                success_op = this->cache->deleteItem(req->key());
                rep = new datareplied(preplybuffer, (void*) NULL, 0, CODE_OP_DELETE_BY_KEY, (success == 0));
                break;
            case CODE_OP_SHUT_DOWN:
                closeall = true;
                rep = new datareplied(preplybuffer, (void*) NULL, 0, CODE_OP_SHUT_DOWN, true);
                break;
        }

        zmq::message_t message_to_send(rep->size());
        memcpy(message_to_send.data(), preplybuffer, rep->size());

        socket->send(message_to_send);

        pthread_mutex_lock(&this->mutex_nreq);
        this->num_req++;
        pthread_mutex_unlock(&this->mutex_nreq);

        delete(req);
        delete(rep);
    }

    usleep(10000);

    pthread_mutex_lock(&this->mutex_nreq);
    std::cout << "num_req = " << num_req << std::endl;

    for (int i = 0; i < this->nworkerthreads; i++) {
        if (i != nsocket)
            pthread_cancel(this->tids[i]);
    }

    pthread_mutex_unlock(&this->mutex_nreq);
    abort();
}

int main(int argc, char *argv[]) {
    int numworkers;

    if (argc != 2 || sscanf(argv[1], "%d", &numworkers) != 1) {
        std::cerr << "Usage: " << argv[0] << "'num_workers'" << std::endl;
        return -1;
    }
    if (numworkers < 1) {
        std::cerr << "num_workers must be greater than 0" << std::endl;
        return -2;
    }

    zmqServer *server = new zmqServer(numworkers);

    server->work();

    return 0;
}
