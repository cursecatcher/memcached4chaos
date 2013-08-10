#include <zmq.hpp>

#include "cache/datacache.hpp"
#include "reqrep.hpp"


class zmqServer {
private:
    DataCache *cache;
    zmq::context_t *context;
    zmq::socket_t *socket;
    unsigned nworkerthreads;

    bool stopped;
    bool runned;

public:
    zmqServer(unsigned nworkerthreads);
    ~zmqServer();

    void work();
    void stop();
};
