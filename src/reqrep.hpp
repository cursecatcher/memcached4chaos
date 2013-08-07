#ifndef REQREP_HPP
#define REQREP_HPP

#include <cstddef>
#include <cstring>
#include <cstdlib>

typedef enum {
    TYPE_OP_NONE = 0,
    TYPE_OP_SET,
    TYPE_OP_GET,
    TYPE_OP_DELETE
} opt_t;


class req_t {
private:
    char *_key;
    void *_data;
    size_t _nkey;
    size_t _nbytes;
    opt_t _req_type;

/* serialized
 * +-----+------+--------+--//---+--//--+
 * | opt | nkey | nbytes |  key  | data |
 * +-----+------+--------+--//---+--//--+ */
    char *_serialized;
    size_t _totbytes;


    inline void init(const char *key, const size_t nkey, const void *data, const size_t nbytes, const opt_t op) {
        this->_key = (char*) malloc(nkey+1);
        this->_nkey = nkey;
        this->_nbytes = nbytes;
        this->_req_type = op;

        memcpy((void*) this->_key, key, nkey);
        this->_key[nkey] = '\0';

        if (nbytes > 0) {
            this->_data = malloc(nbytes);
            memcpy((void*) this->_data, data, nbytes);
        }
        else {
            this->_data = NULL;
        }
    }

    /* alloca la memoria necessaria per il dato serializzato*/
    inline void serialize() {
        this->_totbytes = sizeof(this->_nkey) + sizeof(this->_nbytes) +
                         sizeof(this->_req_type) + this->_nkey + this->_nbytes;
        this->_serialized = (char*) malloc(this->_totbytes);

        int i = sizeof(this->_req_type);
        memcpy((void*) this->_serialized, (void*) &this->_req_type, sizeof(this->_req_type));
        memcpy((void*) (this->_serialized + i), (void*) &this->_nkey, sizeof(this->_nkey));
        i += sizeof(this->_nkey);
        memcpy((void*) (this->_serialized + i), (void*) &this->_nbytes, sizeof(this->_nbytes));
        i += sizeof(this->_nbytes);
        memcpy((void*) (this->_serialized + i), (void*) this->_key, this->_nkey);

        if (this->_nbytes > 0) {
            i += this->_nkey;
            memcpy(this->_serialized + i, this->_data, this->_nbytes);
        }
    }

    inline void extract() {
        int i = sizeof(this->_req_type);
        memcpy((void*) &this->_req_type, (void*) this->_serialized, sizeof(this->_req_type));
        memcpy((void*) &this->_nkey, (void*) (this->_serialized + i), sizeof(this->_nkey));
        i += sizeof(this->_nkey);
        memcpy((void*) &this->_nbytes, (void*) (this->_serialized + i), sizeof(this->_nbytes));
        i += sizeof(this->_nbytes);
        this->_key = (char*) malloc(this->_nkey);
        memcpy((void*) this->_key, (void*) (this->_serialized + i), this->_nkey);

        if (this->_nbytes > 0) {
            this->_data = malloc(this->_nbytes);
            i += this->_nkey;
            memcpy(this->_data, (void*) (this->_serialized + i), this->_nbytes);
        }
        this->_key[i] = '\0';
    }

public:
    inline req_t(const void *data_serialized, const size_t totbytes) {
        this->_totbytes = totbytes;
        this->_serialized = (char*) malloc(totbytes);
        memcpy(this->_serialized, data_serialized, totbytes);

        this->extract();
    }

    inline req_t(const char *key, const size_t nkey, opt_t op_to_do) {
        this->init(key, nkey, NULL, 0, op_to_do);
        this->serialize();
    }

    // store
    inline req_t(const char *key, const size_t nkey, const void *data, const size_t nbytes) {
        this->init(key, nkey, data, nbytes, TYPE_OP_SET);
        this->serialize();
    }

    ~req_t() {
        if (this->_key)         free(this->_key);
        if (this->_data)        free(this->_data);
        if (this->_serialized)  free(this->_serialized);
    }

    inline char* key()      { return this->_key; }
    inline size_t keylen()  { return this->_nkey; }
    inline void* data()     { return this->_data; }
    inline size_t datalen() { return this->_nbytes; }
    inline opt_t op()       { return this->_req_type; }
    inline void* binary()   { return this->_serialized; }
    inline size_t size()    { return this->_totbytes; }
};


class rep_t {
private:
    void *_data;    // riservato per le GETs
    int32_t _nbytes;

    bool _valret;
    opt_t _rep_type;
    /* _serialized
 * +-----+--------+--------+--//---+
 * | opt | valret | nbytes |  data |
 * +-----+--------+--------+--//---+ */
    char *_serialized;
    size_t _totbytes;

    inline void init(const void *data, const int32_t nbytes, const opt_t op, const bool valret) {
        this->_rep_type = op;
        this->_valret = valret;
        this->_nbytes = nbytes;

        if (nbytes > 0) {
            this->_data = malloc((size_t) nbytes);
            memcpy(this->_data, data, (size_t) nbytes);
        }
        else {
            this->_data = NULL;
        }
    }

    inline void extract() {
        int i = sizeof(this->_rep_type);
        memcpy((void*) &this->_rep_type, (void*) this->_serialized, sizeof(this->_rep_type));
        memcpy((void*) &this->_valret, (void*) (this->_serialized + i), sizeof(this->_valret));
        i += sizeof(this->_valret);
        memcpy((void*) &this->_nbytes, (void*) (this->_serialized + i), sizeof(this->_nbytes));

        if (this->_nbytes > 0) {
            this->_data = malloc(this->_nbytes);
            i += sizeof(this->_nbytes);
            memcpy(this->_data, (void*) (this->_serialized + i), this->_nbytes);
        }
    }

    inline void serialize() {
        this->_totbytes = sizeof(this->_rep_type) + sizeof(this->_valret) +
                          sizeof(this->_nbytes) + ((size_t) this->_nbytes);
        this->_serialized = (char*) malloc(this->_totbytes);

        int i = sizeof(this->_rep_type);
        memcpy((void*) this->_serialized, (void*) &this->_rep_type, sizeof(this->_rep_type));
        memcpy((void*) (this->_serialized + i), (void*) &this->_valret, sizeof(this->_valret));
        i += sizeof(this->_valret);
        memcpy((void*) (this->_serialized + i), (void*) &this->_nbytes, sizeof(this->_nbytes));
        i += sizeof(this->_nbytes);

        if (this->_nbytes > 0)
            memcpy((void*) (this->_serialized + i), this->_data, this->_nbytes);
    }

public:
    inline rep_t(const void *data_serialized, const size_t totbytes) {
        this->_totbytes = totbytes;
        this->_serialized = (char*) malloc(totbytes);
        memcpy(this->_serialized, data_serialized, totbytes);

        this->extract();
    }

    inline rep_t(const void *data, const int32_t nbytes, const bool valret) {
        this->init(data, nbytes, TYPE_OP_GET, valret);
        this->serialize();
    }

    inline rep_t(const bool valret, const opt_t op_done) {
        this->init(NULL, 0, op_done, valret);
        this->serialize();
    }

    ~rep_t() {
        if (this->_data)        free(this->_data);
        if (this->_serialized)  free(this->_serialized);
    }

    inline void* data()         { return this->_data; }
    inline int32_t datalen()    { return this->_nbytes; }
    inline bool valret()        { return this->_valret; }
    inline size_t size()        { return this->_totbytes; }
    inline void* binary()       { return (void*) this->_serialized; }
};

#endif // REQREP_HPP
