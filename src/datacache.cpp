#include "datacache.hpp"

DataCache::DataCache() {
//    pthread_mutex_init(&this->cache_lock, NULL);

    //init config
    this->config.use_cas = true;
    this->config.verbose = 0; /** levare **/
    this->config.oldest_live = 0;
    this->config.evict_to_free = true;
    this->config.maxbytes = 64 * 1024 * 1024;
    this->config.preallocate = false;
    this->config.factor = 1.25;
    this->config.hashpower = 16;
    this->config.chunk_size = 48;
    this->config.item_size_max = 1024;

    this->assoc = new Assoc(this, this->config.hashpower);
    this->slabs = new Slabs(this,
                             this->config.maxbytes,
                             this->config.factor,
                             this->config.preallocate);
    this->lru = new LRU(this);

}

int DataCache::get_item(const char *key, int32_t &bufflen, void **outbuffer) {
    hash_item *it = this->lru->item_get(key, strlen(key));

    cout << "get(" << key << "): " << (it ? "YEP" : "NOPE") << endl;

    if (it) {
        bufflen = it->nbytes;
    }

    return it ? 1 : 0;
}

int DataCache::store_item(const char *key, const void *inbuffer, int32_t bufflen) {
    hash_item *it = this->lru->item_alloc(key, strlen(key), /*??*/ 0, bufflen);

    cout << "store(" << key << ")" << endl;

    if (it) {
        this->lru->store_item(it);
        return 1;
    }

    return 0;
}

int DataCache::delete_item(const char *key) {
    hash_item *it = this->lru->item_get(key, strlen(key));

    cout << "delete(" << key << "): " << (it ? "YEP" : "NOPE") << endl;

    if (it) {
        this->lru->item_unlink(it);
        this->lru->item_release(it);
    }

    return 0;
}

