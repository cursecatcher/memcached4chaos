#ifndef CONST_TYPES_H
#define CONST_TYPES_H

#include <cstddef> //definition of size_t
#include <inttypes.h> //definition of uint*_t
#include <iostream>

/* slabs defines */
#define POWER_SMALLEST 1
#define POWER_LARGEST  200
#define CHUNK_ALIGN_BYTES 8
#define DONT_PREALLOC_SLABS
#define MAX_NUMBER_OF_SLAB_CLASSES (POWER_LARGEST + 1)

/** How long an object can reasonably be assumed to be locked before
 *  harvesting it on a low memory condition. */
#define TAIL_REPAIR_TIME (3 * 3600)

/** We only reposition items in the LRU queue if they haven't been repositioned
 * in this many seconds. That saves us from churning on frequently-accessed items. */
#define ITEM_UPDATE_INTERVAL 60

/** To avoid scanning through the complete cache in some circumstances we'll
 * just give up and return an error after inspecting a fixed number of objects. */
#define SEARCH_ITEMS 50

/* Flags */
#define NO_FLAGS 0
#define ITEM_LINKED 1
#define ITEM_SLABBED 2


typedef uint32_t rel_time_t;

typedef struct _hash_item {
    struct _hash_item *next; // pointer to next item in lru
    struct _hash_item *prev; // pointer to previous item in lru
    struct _hash_item *h_next; // hash chain next
    rel_time_t time;  // least recent access

    uint32_t nbytes; // < The total size of the data (in bytes)
    uint16_t nkey; // The total length of the key (in bytes)
    uint16_t iflag; // Flags associated with the item
    unsigned short refcount;
    uint8_t slabs_clsid; // which slab class we're in
} hash_item;

struct config {
    size_t maxbytes; // SLABS
    bool preallocate; // SLABS
    float factor; // SLABS
    size_t chunk_size; // SLABS
    unsigned int hashpower; // ASSOC
    size_t item_size_max; // LRU - SLABS
};
#endif
