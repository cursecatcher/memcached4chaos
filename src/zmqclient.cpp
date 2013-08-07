#include <iostream>
#include <cstring>
#include <zmq.hpp>
#include "reqrep.hpp"

int main(int argc, char *argv[]) {
    if (argc != 3) {
        std::cerr << "Usage: " << argv[0] << " 'key to send' 'value to send'" << std::endl;
        std::cerr << "Aborted." << std::endl;
        return -1;
    }

    zmq::context_t context(1);
    zmq::socket_t socket(context, ZMQ_REQ);

    socket.connect("tcp://localhost:5555");

    datarequested *req = NULL;
    datareplied *rep = NULL;
    void *buffer = malloc(1024); // un bel buffer random, che sei bellino!

    for (int i = 0; i < 4; i++) {
        req = new datarequested(buffer, argv[1], strlen(argv[1]), NULL, 0, TYPE_OP_GET);
        zmq::message_t srequest(req->size());

        memcpy(srequest.data(), req->binary(), req->size());
        socket.send(srequest);

        zmq::message_t sreply;
        socket.recv(&sreply);

        rep = new datareplied(sreply.size(), sreply.data());

        std::cout << "Esito operazione: " << rep->valret() << std::endl;

        delete(req);
        delete(rep);
    }

    return 0;
}
