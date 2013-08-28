#include <pthread.h>
#include <unistd.h>
#include <zmq.hpp>

#include "../cache/datacache.hpp"
#include "reqrep.hpp"

void *server_thread(void *arg);


class zmqServer {
    DataCache *cache;

    zmq::context_t *context;
    zmq::socket_t **sockets;
    pthread_t *tids;
    int nworkerthreads;

    pthread_cond_t cond; // usata nell'inizializzazione
    uint64_t num_req; // contatore del numero di richieste
    pthread_mutex_t mutex_nreq;


public:
    zmqServer(int nworkerthreads);

    void work();
    void worker(int nsocket);
};
