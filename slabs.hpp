#include <iostream>
#include <cstdlib>
#include <cstring>

#include "defines.h"

#define MAX_NUMBER_OF_SLAB_CLASSES 1
#define POWER_LARGEST 1
#define POWER_SMALLEST 1


#define CHUNK_ALIGN_BYTES 1


typedef struct {
    unsigned int size;      /* sizes of items */
    unsigned int perslab;   /* how many items per slab */

    void *slots;           /* list of item ptrs */
    unsigned int sl_curr;   /* total free items in list */

    unsigned int slabs;     /* how many slabs were allocated for this class */

    void **slab_list;       /* array of slab pointers */
    unsigned int list_size; /* size of prev array */

    unsigned int killing;  /* index+1 of dying slab, or zero if none */
    size_t requested; /* The number of requested bytes */
} slabclass_t;


class slab_allocator {
    private:
/* Access to the slab allocator is protected by this lock */
        pthread_mutex_t slabs_lock;
        pthread_mutex_t slabs_rebalance_lock;

        slabclass_t slabclass[MAX_NUMBER_OF_SLAB_CLASSES];
        size_t mem_limit;
        size_t mem_malloced;
        size_t mem_avail;
        int power_largest;

        void* mem_base;
        void* mem_current;

    public:
/** Init the subsystem. 1st argument is the limit on no. of bytes to allocate,
    0 if no limit. 2nd argument is the growth factor; each slab will use a chunk
    size equal to the previous slab's chunk size times this factor.
    3rd argument specifies if the slab allocator should allocate all memory
    up front (if true), or allocate memory in chunks as it is needed (if false)
*/
        slab_allocator(const size_t limit, const double factor = 1.25, const bool prealloc = false);
};
