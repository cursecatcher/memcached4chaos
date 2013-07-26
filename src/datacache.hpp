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
    rel_time_t oldest_live;
    bool evict_to_free;
    size_t maxbytes; // SLABS
    bool preallocate; // SLABS
    float factor; // SLABS
    unsigned int hashpower; // ASSOC
    size_t chunk_size;
    size_t item_size_max;
};


class DataCache {
public:
    Assoc *assoc;
    Slabs *slabs;
    LRU *lru;
    struct config config; // public member

    DataCache();

    bool get_item(const char *key, int32_t &bufflen, void **outbuffer);
    bool store_item(const char *key, const void *inbuffer, int32_t bufflen);
    bool delete_item(const char *key);
};
#endif
