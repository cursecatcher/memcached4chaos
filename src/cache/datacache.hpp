#ifndef ENGINE_HPP
#define ENGINE_HPP

#include <cstring>

#include "lru.hpp"
#include "const_types.h"

/** previous declarations **/
class LRU_Lists;


class DataCache {
private:
    struct config config;
    LRU_Lists *lru;

public:
    DataCache(size_t MB_to_allocate = 64) {
        //init config
        this->config.maxbytes = MB_to_allocate * 1024 * 1024;
        this->config.preallocate = false;
        this->config.factor = 1.25;
        this->config.chunk_size = 48;
        this->config.hashpower = 16;
        this->config.item_size_max = 1024 * 1024; // 1 MB

        this->lru = new LRU_Lists(this->config);
    }

    inline bool get_item(const char *key, int32_t &bufflen, void **outbuffer) {
        hash_item *it = this->lru->item_get(key, strlen(key));
        bool ret;

        if ((ret = (it != NULL))) {
            bufflen = it->nbytes;
            memcpy(*outbuffer, this->lru->item_get_data(it), it->nbytes);
        }

        return ret;
    }

    inline bool store_item(const char *key, const void *inbuffer, int32_t bufflen) {
        hash_item *it = this->lru->item_alloc(key, strlen(key), bufflen);
        bool ret;

        if ((ret = (it != NULL))) {
            memcpy(this->lru->item_get_data(it), inbuffer, bufflen);
            this->lru->store_item(it);
        }

        return ret;
    }

    inline bool delete_item(const char *key) {
        hash_item *it = this->lru->item_get(key, strlen(key));
        bool ret;

        if ((ret = (it != NULL))) {
            this->lru->item_unlink(it);
            this->lru->item_release(it);
        }

        return ret;
    }
};
#endif
