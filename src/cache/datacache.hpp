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
    DataCache(size_t cacheSize = 64) {
        //init config
        this->config.maxbytes = cacheSize * 1024 * 1024;
        this->config.preallocate = false;
        this->config.factor = 1.25;
        this->config.chunk_size = 48;
        this->config.hashpower = 16;
        this->config.item_size_max = 1024 * 1024; // 1 MB

        this->lru = new LRU_Lists(this->config);
    }

    inline int getItem(const char *key, int32_t &bufflen, void **outbuffer) {
        hash_item *it = this->lru->item_get(key, strlen(key));
        int ret = -1; //

        if (it) {
            bufflen = it->nbytes;
            memcpy(*outbuffer, this->lru->item_get_data(it), it->nbytes);
            ret = 0;
        }

        return ret;
    }

    inline int storeItem(const char *key, const void *inbuffer, int32_t bufflen) {
        hash_item *it = this->lru->item_alloc(key, strlen(key), bufflen);
        int ret = -1;

        if ((ret = (it != NULL))) {
            memcpy(this->lru->item_get_data(it), inbuffer, bufflen);
            this->lru->store_item(it);
            ret = 0;
        }

        return ret;
    }

    inline int deleteItem(const char *key) {
        hash_item *it = this->lru->item_get(key, strlen(key));
        int ret = -1;

        if ((ret = (it != NULL))) {
            this->lru->item_unlink(it);
            this->lru->item_release(it);
            ret = 0;
        }

        return ret;
    }
};
#endif
