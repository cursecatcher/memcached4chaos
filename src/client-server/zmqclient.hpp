#include <iostream>
#include <cstring>
#include <zmq.hpp>
#include <pthread.h>
#include "reqrep.hpp"

#define VERBOSE 1

void *thread_stopper(void* arg);
void *client_thread(void* arg);

class zmqClient {
private:
    zmq::context_t *context;
    zmq::socket_t **sockets;
    pthread_t *tids;

    unsigned nvirtualclient;
    unsigned ttr;
    bool stopped;
    bool runned;

    pthread_cond_t cond;
    pthread_mutex_t mutex;

public:
    zmqClient(unsigned nvirtualclient, unsigned time_to_run);
    ~zmqClient();

    void connect(const char *address);
    void work();
    void stop() { this->stopped = true; }

    void worker(int nsocket);
//    void disconnect() {} // thread "captcha"
};
