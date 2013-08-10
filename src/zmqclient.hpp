#include <iostream>
#include <cstring>
#include <zmq.hpp>
#include "reqrep.hpp"

class zmqClient {
private:
    zmq::context_t *context;
    zmq::socket_t *socket;

    unsigned nvirtualclient;
    bool stopped;
    bool runned;

public:
    zmqClient(unsigned nvirtualclient);
    ~zmqClient();

    void connect(const char *address);
    void work();
    void disconnect() {} // thread "captcha"
};
