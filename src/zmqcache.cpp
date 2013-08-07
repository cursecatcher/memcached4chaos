#include <zmq.hpp>
#include <iostream>
#include <unistd.h>

#include "datacache.hpp"
//#include "threadpool.hpp"
#include "reqrep.hpp"

typedef struct {
    DataCache *cache;
    zmq::context_t *context;
} datathread_t;

/* accetta le richieste via socket
 * gestisce la thread pool */
void *server(void *arg);


int main() {
    pid_t pid = getpid();
    datathread_t data;
    zmq::context_t context(1);

    data.cache = new DataCache();
    data.context = &context;

    zmq::socket_t clients (context, ZMQ_ROUTER);
    clients.bind("tcp://*:5555");
    zmq::socket_t workers (context, ZMQ_DEALER);
    workers.bind("inproc://workers");

    std::cout << "Pid process: " << pid << std::endl;

    for (int i = 0; i < 4; i++) {
        pthread_t tid;
        if (pthread_create(&tid, NULL, server, (void*) &data)) {
            std::cout << "Cannot create thread #" << i+1 << ". Aborted." << std::endl;
            return -1;
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
        std::cout << "Thread in attesa...\n";

        zmq::message_t request;
        socket.recv(&request);

        // interpreta richiesta
        req = new datarequested( request.size(), request.data());

        // eseguie l'operazione richiesta
        switch (req->op_code()) {
            case TYPE_OP_GET:
                std::cout << "richiesta get(" << req->key() << ")\n";
                success = data->cache->get_item(req->key(), bufflen, &buffer);
                rep = new datareplied(tempbuffer, buffer, (size_t) bufflen, TYPE_OP_GET, success);
                break;
            case TYPE_OP_SET:
                std::cout << "richiesta store(" << req->key() << ", " << req->datasize() << ")\n";
                success = data->cache->store_item(req->key(), req->data(), req->datasize());
                rep = new datareplied(tempbuffer, (void*) NULL, 0, TYPE_OP_SET, success);
                break;
            case TYPE_OP_DELETE:
                std::cout << "richiesta delete(" << req->key() << ")\n";
                success = data->cache->delete_item(req->key());
                rep = new datareplied(tempbuffer, (void*) NULL, 0, TYPE_OP_DELETE, success);
                break;
        }

        std::cout << "byte reply: " << rep->size() << std::endl;

        zmq::message_t reply(rep->size());
        memcpy((void *) reply.data(), tempbuffer, rep->size());
        socket.send(reply);

//        delete(req);
//        delete(rep);
    }

    return NULL;
}
