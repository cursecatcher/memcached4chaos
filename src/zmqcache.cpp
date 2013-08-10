#include "zmqcache.hpp"
#include <iostream>

/* creare header per le risposte ai client:
 * ************************************************
 * - tipo operazione
 * - successo operazione
 * - byte usati per l'eventuale campo variabile
 * ************************************************
 * - campo variabile */

zmqServer::zmqServer(unsigned nworkerthreads) {
    this->nworkerthreads = nworkerthreads;
    this->stopped = false;
    this->runned = false;

    this->context = new zmq::context_t(1);
    this->cache = new DataCache();
}

zmqServer::~zmqServer() {
    this->stop();

    delete(this->context);
}

void zmqServer::work() {
    char cachebuffer[1024]; //preallocated
    char replybuffer[1024];
    void *pcachebuffer = (void*) cachebuffer;
    void *preplybuffer = (void*) replybuffer;
    int32_t byte_used = 0;
    datarequested *req = NULL;
    datareplied *rep = NULL;
    bool success_op = false;

    this->socket = new zmq::socket_t(*(this->context), ZMQ_REP);
    this->socket->bind("tcp://*:5555");

    this->runned = true;

    while (!this->stopped) {
        zmq::message_t recv_message;
        socket->recv(&recv_message);
        req = new datarequested(recv_message.size(), recv_message.data());

        //process recv_message
        switch (req->op_code()) {
            case CODE_OP_GET_VALUE_BY_KEY:
                success_op = this->cache->get_item(req->key(), byte_used, &pcachebuffer);
                rep = new datareplied(preplybuffer, pcachebuffer, (size_t) byte_used, CODE_OP_GET_VALUE_BY_KEY, success_op);
                break;
            case CODE_OP_SET_KEY_VALUE:
                success_op = this->cache->store_item(req->key(), req->data(), req->datasize());
                rep = new datareplied(preplybuffer, (void*) NULL, 0, CODE_OP_SET_KEY_VALUE, success_op);
                break;
            case CODE_OP_DELETE_BY_KEY:
                success_op = this->cache->delete_item(req->key());
                rep = new datareplied(preplybuffer, (void*) NULL, 0, CODE_OP_DELETE_BY_KEY, success_op);
                break;
            case CODE_OP_SHUT_DOWN:
                break;
        }

        zmq::message_t message_to_send(rep->size());
        memcpy(message_to_send.data(), preplybuffer, rep->size());

        socket->send(message_to_send);

        delete(req);
        delete(rep);
    }
}

void zmqServer::stop() {
    if (this->runned)
        this->stopped = true;
}


int main() {
    zmqServer *server = new zmqServer(2);

    server->work();

    return 0;
}
