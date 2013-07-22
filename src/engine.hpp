#include "const_types.h"
#include "slabs.hpp"
#include "assoc.hpp"
#include "lru.hpp"


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
    Assoc *assoc;
    Slabs *slabs;
    LRU *lru;

    pthread_mutex_t cache_lock;


public:
    struct config config; // public member

    Engine();

    void lock_cache();
    void unlock_cache();
};
