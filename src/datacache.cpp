#include "engine.hpp"

Engine::Engine() {
    pthread_mutex_init(&this->cache_lock, NULL);

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

    this->slabs = new Slabs(this,
                             this->config.maxbytes,
                             this->config.factor,
                             this->config.preallocate);
    this->assoc = new Assoc(this,
                              this->config.hashpower);
    this->lru = new LRU(this);

}

void Engine::lock_cache() {
    pthread_mutex_lock(&this->cache_lock);
}

void Engine::unlock_cache() {
    pthread_mutex_unlock(&this->cache_lock);
}

