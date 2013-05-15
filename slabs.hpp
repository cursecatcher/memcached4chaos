#include <iostream>
#include <cstdlib>
#include <cstring>

#include "defines.h"

#define MAX_NUMBER_OF_SLAB_CLASSES 1
#define DEFAULT_SLAB_BULK_CHECK 1

#define POWER_LARGEST 1
#define POWER_SMALLEST 1

#define CHUNK_ALIGN_BYTES 1
#define MACRO_RANDOM 1


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

struct slab_rebalance {
    void *slab_start;
    void *slab_end;
    void *slab_pos;
    int s_clsid;
    int d_clsid;
    int busy_items;
    uint8_t done;
};

enum reassign_result_type {
    REASSIGN_OK=0, REASSIGN_RUNNING, REASSIGN_BADCLASS, REASSIGN_NOSPARE,
    REASSIGN_SRC_DST_SAME
};


class slab_allocator {
    private:
/* Access to the slab allocator is protected by this lock */
        pthread_mutex_t slabs_lock;
        pthread_mutex_t slabs_rebalance_lock;

        pthread_t rebalance_tid;
        pthread_t maintenance_tid;

        pthread_cond_t slab_rebalance_cond;

        struct slab_rebalance slab_rebal;
        int slab_rebalance_signal;

        int slab_bulk_check;

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

/** Given object size, return id to use when allocating/freeing memory for object
 *  0 means error: can't store such a large object */
        unsigned int slabs_clsid(const size_t size);

/** Allocate object of given length, 0 on error */
        void *slabs_alloc(size_t size, unsigned int id);

/** Free previously allocated object */
        void slabs_free(void *ptr, size_t size, unsigned int id);

        enum reassign_result_type slabs_reassign(int src, int dst);

        int start_slab_maintenance_thread(void);
        void stop_slab_maintenance_thread(void);

        void slabs_rebalancer_pause(void);
        void slabs_rebalancer_resume(void);

/** Slab rebalancer thread.
 * Does not use spinlocks since it is not timing sensitive. Burn less CPU and
 * go to sleep if locks are contended */
        void *slab_maintenance_thread(void *arg);

/** Slab mover thread.
 * Sits waiting for a condition to jump off and shovel some memory about */
        void *slab_rebalance_thread(void *arg);
};
