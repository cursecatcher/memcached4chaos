#include <iostream>
#include <string>
#include <cstring>
#include <zmq.hpp>
#include <pthread.h>
#include "reqrep.hpp"

#define VERBOSE 1

using namespace std;

void *thread_stopper(void* arg);
void *client_thread(void* arg);

class zmqClient {
    zmq::context_t *context;
    zmq::socket_t **sockets;
    pthread_t *tids;

    string address;

    int nvirtualclient;
    int ttr;

    pthread_cond_t cond;

public:
    zmqClient(int nvirtualclient, int time_to_run, string address = "tcp://localhost:5555");

    void connect();
    void work();
    void stop();

    void worker(int nsocket);
};
