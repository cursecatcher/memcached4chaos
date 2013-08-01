#include "datacache.hpp"

DataCache::DataCache() {
    //init config
    this->config.oldest_live = 0;
    this->config.maxbytes = 64 * 1024 * 1024; // 64 MB
    this->config.preallocate = false;
    this->config.factor = 1.25;
    this->config.chunk_size = 48;
    this->config.hashpower = 16;
    this->config.item_size_max = 1024 * 1024; // 1 MB

    this->lru = new LRU(this->config);
}

bool DataCache::get_item(const char *key, int32_t &bufflen, void **outbuffer) {
    hash_item *it = this->lru->item_get(key, strlen(key));
    bool ret = false;

    if (it) {
        bufflen = it->nbytes;
        memcpy(*outbuffer, this->lru->item_get_data(it), it->nbytes);

        ret = true;
    }

    return ret;
}

bool DataCache::store_item(const char *key, const void *inbuffer, int32_t bufflen) {
    hash_item *it = this->lru->item_alloc(key, strlen(key), bufflen);
    bool ret = false;

    if (it) {
        memcpy(this->lru->item_get_data(it), inbuffer, bufflen);
        this->lru->store_item(it);

        ret = true;
    }

    return ret;
}

bool DataCache::delete_item(const char *key) {
    hash_item *it = this->lru->item_get(key, strlen(key));
    bool ret = false;

    if (it) {
        this->lru->item_unlink(it);
        this->lru->item_release(it);

        ret = true;
    }

    return ret;
}

