#ifndef REQREP_HPP
#define REQREP_HPP

#include <cstddef>
#include <cstring>
#include <cstdlib>

typedef enum {
    TYPE_OP_SET = 1,
    TYPE_OP_GET,
    TYPE_OP_DELETE
} opt_t;

struct req_header {
    opt_t _op;
    size_t _nkey;
    size_t _nbytes;
};


/* req_t::bynary()
 * +-----+------+--------+--//---+ ---+---//---+
 * | opt | nkey | nbytes |  key  |'\0'|  data  |
 * +-----+------+--------+--//---+----+---//---+ */
class datarequested {
private:
    struct req_header _header;
    size_t _totbytes;
    char *_serialized;
    char *_key;
    char *_data;
public:
    /**
     * Istanzia un tipo opaco tramite cui è possibile accedere ai campi
     * del messaggio ricevuto
     * @param totbytes La dimensione in byte del dato serializzato
     * @param serialized Il dato in formato binario ricevuto via socket */
    datarequested(size_t totbytes, void* serialized) {
        this->_serialized = (char*) serialized;
        this->_totbytes = totbytes;
        memcpy((void*) &this->_header, serialized, sizeof(struct req_header));
        this->_key = this->_serialized + sizeof(struct req_header);
        this->_data = this->_header._nbytes > 0 ?
                      this->_key + this->_header._nkey + 1 :
                      NULL;
    }

    datarequested(void *dest, char *key, size_t nkey, void *data, size_t nbytes, opt_t op) {
        this->_header._nkey = nkey;
        this->_header._nbytes = nbytes;
        this->_header._op = op;

        this->_serialized = (char*) dest;
        memcpy(dest, (void*) &this->_header, sizeof(struct req_header));

        memcpy((void*) (this->_serialized + sizeof(struct req_header)), key, nkey);
        size_t offset = sizeof(struct req_header) + nkey;
        this->_serialized[offset++] = '\0';

        memcpy((void*) (this->_serialized + offset), data, nbytes);

        this->_totbytes = offset + this->_header._nbytes;
    }

    inline void *binary()       { return this->_serialized; }
    inline char *key()          { return this->_key; }
    inline void *data()         { return (void*) this->_data; }
    inline size_t size()        { return this->_totbytes; }
    inline size_t keysize()     { return this->_header._nkey; }
    inline size_t datasize()    { return this->_header._nbytes; }
    inline opt_t op_code()      { return this->_header._op; }
};

/* rep_t::binary()
 * +-----+--------+--------+--//---+
 * | opt | valret | nbytes |  data |
 * +-----+--------+--------+--//---+ */
struct rep_header {
    opt_t _op;
    bool _valret;
    size_t _nbytes;
};

class datareplied {
private:
    rep_header _header;
    size_t _totbytes;
    char *_serialized;
    char *_data;
public:
    /**
     * Istanzia un tipo opaco tramite cui è possibile accedere ai campi
     * del messaggio da inviare
     * @param serialized Il dato in formato binario ricevuto via socket
     * @param totbytes La dimensione in byte del dato serializzato */
    datareplied(size_t totbytes, void *serialized) {
        this->_serialized = (char*) serialized;
        this->_totbytes = totbytes;
        memcpy((void*) &this->_header, serialized, sizeof(struct rep_header));
        this->_data = this->_header._nbytes > 0 ?
                      this->_serialized + sizeof(struct rep_header) : NULL;
    }

    /**
     * Serializza il dato passato nel buffer e rende accessibile le
     * componenti
     * @param outbuffer Destinazione della bufferizzazione
     * @param data Porco dio, arrivaci
     * @param nbytes
     * @param op
     * @param valret
     * */ // leggi i commenti e sistemali, dio cane; molto pratici 5 parametri
    datareplied(void *dest, void *data, size_t nbytes, opt_t op, bool valret) {
        this->_header._nbytes = nbytes;
        this->_header._op = op;
        this->_header._valret = valret;
        this->_totbytes = sizeof(struct rep_header) + this->_header._nbytes;

        this->_serialized = (char*) dest;
        memcpy(dest, (void*) &this->_header, sizeof(struct rep_header));

        if (nbytes > 0 && data) {
            this->_data = this->_serialized + sizeof(struct rep_header);
            memcpy((void*) (this->_serialized + sizeof(struct rep_header)), data, nbytes);
        }
        else {
            this->_data = NULL;
        }
    }

    inline void *binary()       { return (void*) this->_serialized; }
    inline void *data()         { return (void*) this->_data; }
    inline size_t size()        { return this->_totbytes; }
    inline size_t datasize()    { return this->_header._nbytes; }
    inline opt_t op_code()      { return this->_header._op; }
    inline bool valret()        { return this->_header._valret; }
};


#endif // REQREP_HPP
