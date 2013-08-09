#include <iostream>
#include <cstring>
#include <zmq.hpp>
#include "reqrep.hpp"

typedef struct {
    char *key;
    void *value;
    size_t keylen;
    size_t valuelen;
    pthread_mutex_t init_lock;
} datathread_t;

int nthr = 0;

void *client(void *arg);
void *shutdown_thr(void *arg);

int main(int argc, char *argv[]) {
    int nclient, ttr;

    if (argc != 3 || sscanf(argv[1], "%d", &nclient) != 1 ||
        sscanf(argv[2], "%d", &ttr) != 1 || nclient < 1 || ttr < 1) {
        std::cerr << "Usage: " << argv[0] << " 'num_client' 'time_to_run'" << std::endl;
        std::cerr << "Aborted." << std::endl;
        return -1;
    }

    datathread_t data;

    pthread_mutex_init(&data.init_lock, NULL);
    data.key = (char*) malloc(100);
    data.value = (char*) malloc(100);

    strcpy(data.key, "giorgio");
    strcpy((char*) data.value, "mastrota");
    data.keylen = strlen(data.key);
    data.valuelen = strlen((char*)data.value);

    pthread_t tid;
    for (int i = 0; i < nclient; i++) {

        if (pthread_create(&tid, NULL, client, &data)) {
            std::cout << "Cannot create thread #" << i+1 << ". Aborted." << std::endl;
            return -2;
        }
    }

    sleep(ttr);

    pthread_create(&tid, NULL, shutdown_thr, NULL);
    pthread_join(tid, NULL);

    return 0;
}


void *client(void *arg) {
    datathread_t *data = (datathread_t*) arg;
    zmq::context_t context(1);

    zmq::socket_t socket(context, ZMQ_REQ);
    socket.connect("tcp://localhost:5555");

    int nthread;

    pthread_mutex_lock(&data->init_lock);
    nthread = nthr++;
//    std::cout << "Client #" << nthread + 1 << " pronto all'azione!" << std::endl;
    pthread_mutex_unlock(&data->init_lock);

    void *buffer = malloc(1024);
    datarequested *req;
    datareplied *rep;

    while (true) {
        if (nthread == 0)
            req = new datarequested(buffer, data->key, data->keylen, data->value, data->valuelen, TYPE_OP_SET);
        else
            req = new datarequested(buffer, data->key, data->keylen, NULL, 0, TYPE_OP_GET);

        zmq::message_t srequest(req->size());

        memcpy(srequest.data(), req->binary(), req->size());
        socket.send(srequest);

        zmq::message_t sreply;
        socket.recv(&sreply);

        rep = new datareplied(sreply.size(), sreply.data());

//        std::cout << "Esito operazione: " << rep->valret() << std::endl;

        delete(req);
        delete(rep);
    }

    return NULL;
}

void *shutdown_thr(void *arg) {
    (void) arg;

    zmq::context_t context(1);

    zmq::socket_t socket(context, ZMQ_REQ);
    socket.connect("tcp://localhost:5555");

    void *buffer = malloc(1024);

    datarequested *req = new datarequested(buffer, NULL, 0, NULL, 0, TYPE_OP_SHUT_DOWN);
    zmq::message_t request(req->size());
    memcpy(request.data(), req->binary(), req->size());
    socket.send(request);

//    zmq::message_t reply;
//    socket.recv(&reply);

    return NULL;
}
