#ifndef ENGINE_HPP
#define ENGINE_HPP

#include "const_types.h"
#include "slabs.hpp"
#include "assoc.hpp"
#include "lru.hpp"

/** previous declarations **/
class Assoc;
class LRU;
class Slabs;


struct config {
    bool use_cas;
    size_t verbose;
    rel_time_t oldest_live;
    bool evict_to_free;
    size_t maxbytes; // SLABS
    bool preallocate; // SLABS
    float factor; // SLABS
    unsigned int hashpower; // ASSOC
    size_t chunk_size;
    size_t item_size_max;
    bool ignore_vbucket;
    bool vb0;
};


class Engine {
private:
    pthread_mutex_t cache_lock;


public:
    Assoc *assoc;
    Slabs *slabs;
    LRU *lru;
    struct config config; // public member

    Engine();

    void lock_cache();
    void unlock_cache();
    rel_time_t get_current_time() { return 0; }
};
#endif
