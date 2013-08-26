#include <pthread.h>
#include <zmq.hpp>

#include "../cache/datacache.hpp"
#include "reqrep.hpp"

void *server_thread(void *arg);


class zmqServer {
private:
    DataCache *cache;
    zmq::context_t *context;
    zmq::socket_t **sockets;
    pthread_t *tids;
    int nworkerthreads;

    bool stopped;
    bool runned;

    pthread_cond_t cond;
    pthread_mutex_t mutex;

public:
    zmqServer(int nworkerthreads);
    ~zmqServer();

    void work();
    void stop();

    void worker(int nsocket);
};
