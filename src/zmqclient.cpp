#include "zmqclient.hpp"

zmqClient::zmqClient(unsigned nvirtualclient) {
    this->nvirtualclient = nvirtualclient;
    this->stopped = false;
    this->runned = false;

    this->context = new zmq::context_t(1);
}

zmqClient::~zmqClient() {
    delete(this->socket);
    delete(this->context);
}

void zmqClient::connect(const char *address) {
    this->socket = new zmq::socket_t(*(this->context), ZMQ_REQ);
    this->socket->connect(address);
}

void zmqClient::work() {
    char key[250+1];
    char value[1024];
    int keylen;
    int valuelen;

    char buffer[1024];
    void *pbuffer = (void*) buffer;
    datarequested *req;
    datareplied *rep;

    strcpy(key, "giorgio");     keylen = strlen(key);
    strcpy(value, "mastrota");  valuelen = strlen(value);


    while (1) {
        req = new datarequested(pbuffer, key, keylen, NULL, 0, CODE_OP_GET_VALUE_BY_KEY);
        zmq::message_t request(req->size());
        memcpy(request.data(), pbuffer, req->size());
        this->socket->send(request);

        zmq::message_t reply;
        this->socket->recv(&reply);
        rep = new datareplied(reply.size(), reply.data());

        //do something with reply
        delete(req);
        delete(rep);

        std::cout << "Received" << std::endl;
    }
}

int main() {
    zmqClient *client = new zmqClient(1);

    client->connect("tcp://localhost:5555");
    client->work();

    return 0;
}
