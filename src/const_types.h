//#pragma once

#include <inttypes.h> //definition of uint*_t


#define POWER_SMALLEST 1
#define POWER_LARGEST  200
#define CHUNK_ALIGN_BYTES 8
#define DONT_PREALLOC_SLABS
#define MAX_NUMBER_OF_SLAB_CLASSES (POWER_LARGEST + 1)

typedef uint32_t rel_time_t;

/* You should not try to aquire any of the item locks before calling these
 * functions. */
typedef struct _hash_item {
    struct _hash_item *next; // pointer to next item in lru
    struct _hash_item *prev; // pointer to previous item in lru
    struct _hash_item *h_next; // hash chain next
    rel_time_t time;  // least recent access

    uint32_t nbytes; // < The total size of the data (in bytes)
    uint32_t flags; // Flags associated with the item (in network byte order)
    uint16_t nkey; // The total length of the key (in bytes)
    uint16_t iflag; /**< Intermal flags. lower 8 bit is reserved for the core
                     * server, the upper 8 bits is reserved for engine
                     * implementation. */
    unsigned short refcount;
    uint8_t slabs_clsid; // which slab class we're in
} hash_item;
