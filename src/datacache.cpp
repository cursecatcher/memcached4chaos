#include "datacache.hpp"

DataCache::DataCache() {
    //init config
    this->config.oldest_live = 0;
    this->config.evict_to_free = true;
    this->config.maxbytes = 64 * 1024 * 1024; // 64 MB
    this->config.preallocate = false;
    this->config.factor = 1.25;
    this->config.hashpower = 16;
    this->config.chunk_size = 48;
    this->config.item_size_max = 1024 * 1024; // 1 MB

    this->assoc = new Assoc(this, this->config.hashpower);
    this->slabs = new Slabs(this,
                             this->config.maxbytes,
                             this->config.factor,
                             this->config.preallocate);
    this->lru = new LRU(this);

}

bool DataCache::get_item(const char *key, int32_t &bufflen, void **outbuffer) {
    hash_item *it = this->lru->item_get(key, strlen(key));
    bool ret = false;

    if (it) {
        bufflen = it->nbytes;
        memcpy(*outbuffer, items::item_get_data(it), it->nbytes);

        ret = true;
    }

    return ret;
}

bool DataCache::store_item(const char *key, const void *inbuffer, int32_t bufflen) {
    hash_item *it = this->lru->item_alloc(key, strlen(key), bufflen);
    bool ret = false;

    if (it) {
        memcpy(items::item_get_data(it), inbuffer, bufflen);
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

