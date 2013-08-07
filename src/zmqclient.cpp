#include <zmq.hpp>
#include "reqrep.hpp"

int main() {
    zmq::context_t context(1);
    zmq::socket_t socket(context, ZMQ_REQ);

    socket.connect("tcp://localhost:5555");

    req_t req("mastrota", 8, TYPE_OP_GET);

    zmq::message_t request(req.size());
    memcpy((void*) request.data(), (void*) req.binary(), req.size());
    socket.send(request);

    zmq::message_t reply;
    socket.recv(&reply);
    rep_t rep(reply.data(), reply.size());


    return 0;
}
