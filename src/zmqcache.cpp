#include <zmq.hpp>
#include <iostream>

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
    datathread_t data;
    zmq::context_t context(1);

    data.cache = new DataCache();
    data.context = &context;

    zmq::socket_t clients (context, ZMQ_ROUTER);
    clients.bind("tcp://*:5555");
    zmq::socket_t workers (context, ZMQ_DEALER);
    workers.bind("inproc://workers");

    for (int i = 0; i < 2; i++) {
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

    while (true) {
        std::cout << "Thread in attesa...\n";

        zmq::message_t request;
        socket.recv(&request);

        // interpreta richiesta
        req_t message(request.data(), request.size());
        std::cout << "Received key: " << message.key() << std::endl;

        // eseguie l'operazione richiesta
        switch (message.op()) {
            case TYPE_OP_GET:
                success = data->cache->get_item(message.key(), bufflen, &buffer);
                break;
            case TYPE_OP_SET:
                success = data->cache->store_item(message.key(), message.data(), message.datalen());
                break;
            case TYPE_OP_DELETE:
                success = data->cache->delete_item(message.key());
                break;
            default:
                success = false;
                bufflen = 0;
                break;
        }

        rep_t *
        to_send = message.op() == TYPE_OP_GET ?
                  new rep_t(buffer, bufflen, success) :
                  new rep_t(success, message.op());

        zmq::message_t reply(to_send->size());
        memcpy((void *) reply.data(), to_send->binary(), to_send->size());
        socket.send(reply);

        delete(to_send);
    }

    return NULL;
}
