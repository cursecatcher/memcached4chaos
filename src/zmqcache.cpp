#include <zmq.hpp>
#include <iostream>
#include <cstdio>
#include <unistd.h> // getpid()

#include "cache/datacache.hpp"
//#include "threadpool.hpp"
#include "reqrep.hpp"

typedef struct {
    DataCache *cache;
    zmq::context_t *context;

    uint64_t nreq;
    uint64_t delta;
    pthread_mutex_t nreq_lock;
} datathread_t;

/* accetta le richieste via socket */
void *server(void *arg);


int main(int argc, char *argv[]) {
    int nworker;

    if (argc != 2 || sscanf(argv[1], "%d", &nworker) != 1 || nworker <= 0) {
        std::cerr << "Usage: " << argv[0] << "num_workert_thread" << std::endl;
        std::cerr << "num_worker_thread must be an integer greater than 0" << std::endl;
        std::cerr << "Aborted" << std::endl;
        return -1;
    }

    pid_t pid = getpid();
    datathread_t data;
    zmq::context_t context(2);

    data.cache = new DataCache();
    data.context = &context;
    data.nreq = data.delta = 0;
    pthread_mutex_init(&data.nreq_lock, NULL);

    zmq::socket_t clients (context, ZMQ_ROUTER);
    clients.bind("tcp://*:5555");
    zmq::socket_t workers (context, ZMQ_DEALER);
    workers.bind("inproc://workers");

    std::cout << "Pid process: " << pid << std::endl;

    for (int i = 0; i < nworker; i++) {
        pthread_t tid;
        if (pthread_create(&tid, NULL, server, (void*) &data)) {
            std::cout << "Cannot create thread #" << i+1 << ". Aborted." << std::endl;
            return -2;
        }
    }

    //  Connect work threads to client threads via a queue
    zmq::proxy(clients, workers, NULL);

    return 0;
}

void *server(void *arg) {
    datathread_t *data = (datathread_t*) arg;

    zmq::context_t *context = data->context;
    zmq::socket_t socket(*context, ZMQ_REP);
    socket.connect ("inproc://workers");

    void *buffer = malloc(1024);  //valore un po' a caso, infatti è provvisorio
    int32_t bufflen = 0;
    bool success = false;
    void *tempbuffer = malloc(1024); //un altro valore random, non farci caso

    datarequested *req = NULL;
    datareplied *rep = NULL;

    while (true) {
//        std::cout << "Thread in attesa...\n";

        zmq::message_t request;
        socket.recv(&request);

        // interpreta richiesta
        req = new datarequested( request.size(), request.data());

        // eseguie l'operazione richiesta
        switch (req->op_code()) {
            case TYPE_OP_GET:
//                std::cout << "richiesta get(" << req->key() << ")\n";
                success = data->cache->get_item(req->key(), bufflen, &buffer);
                rep = new datareplied(tempbuffer, buffer, (size_t) bufflen, TYPE_OP_GET, success);
                break;
            case TYPE_OP_SET:
//                std::cout << "richiesta store(" << req->key() << ", " << req->datasize() << ")\n";
                success = data->cache->store_item(req->key(), req->data(), req->datasize());
                rep = new datareplied(tempbuffer, (void*) NULL, 0, TYPE_OP_SET, success);
                break;
            case TYPE_OP_DELETE:
//                std::cout << "richiesta delete(" << req->key() << ")\n";
                success = data->cache->delete_item(req->key());
                rep = new datareplied(tempbuffer, (void*) NULL, 0, TYPE_OP_DELETE, success);
                break;
            case TYPE_OP_SHUT_DOWN:
                uint64_t totreq, deltareq;

                pthread_mutex_lock(&data->nreq_lock);
                deltareq = data->delta;
                totreq = data->delta = data->nreq;
                pthread_mutex_unlock(&data->nreq_lock);

                deltareq = totreq - deltareq;

                std::cout << "Tot reqs:" << totreq << " -- Incr: " << deltareq << std::endl;
                rep = new datareplied(tempbuffer, (void*) NULL, 0, TYPE_OP_SHUT_DOWN, success);
                break;
        }

        zmq::message_t reply(rep->size());
        memcpy((void *) reply.data(), tempbuffer, rep->size());
        socket.send(reply);

        pthread_mutex_lock(&data->nreq_lock);
        (data->nreq)++;
        pthread_mutex_unlock(&data->nreq_lock);

        delete(req);
        delete(rep);
    }

    return NULL;
}
