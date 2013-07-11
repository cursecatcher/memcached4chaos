#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <pthread.h>

#include "defines.h"
#include "hash.hpp"
#include "mutex.hpp"



void* slab_rebalance_thread(void* arg);
void* slab_maintenance_thread(void* arg);
void* assoc_maintenance_thread(void *arg);

class memslab {
public:
    memslab(
        const int hashpower_init,
        int nthreads,
        const size_t limit,
        const double factor = 1.25,
        const bool prealloc = false);

    void switch_item_lock_type(enum item_lock_types type);

    /** associative array **/
    // void assoc_init(const int hashpower_init);
    item *assoc_find(const char *key, const size_t nkey, const uint32_t hv);
    int assoc_insert(item *it, const uint32_t hv);
    void assoc_delete(const char *key, const size_t nkey, const uint32_t hv);

    item** _hashitem_before (const char *key, const size_t nkey, const uint32_t hv);
    void assoc_expand(void);
    void assoc_start_expand(void);

//      void do_assoc_move_next_bucket(void);
    int start_assoc_maintenance_thread(void);
    void stop_assoc_maintenance_thread(void);
    void* _assoc_maintenance_thread(void);

    /****** slabbing *******/
    /** Given object size, return id to use when allocating/freeing memory for object
 *  0 means error: can't store such a large object */
    unsigned int slabs_clsid(const size_t size);

/** Allocate object of given length, 0 on error */
    void *slabs_alloc(size_t size, unsigned int id);

/** Free previously allocated object */
    void slabs_free(void *ptr, size_t size, unsigned int id);

    enum reassign_result_type slabs_reassign(int src, int dst);

    void slabs_adjust_mem_requested(unsigned int id, size_t old, size_t ntotal);
    int slab_automove_decision(int *src, int *dst);

    int start_slab_maintenance_thread(void);
    void stop_slab_maintenance_thread(void);

/** Slab rebalancer thread.
 * Does not use spinlocks since it is not timing sensitive. Burn less CPU and
 * go to sleep if locks are contended */
    void _slab_maintenance_thread(void);

    void slabs_rebalancer_pause(void);
    void slabs_rebalancer_resume(void);

    int slab_rebalance_start(void);
    int slab_rebalance_move(void);
    void slab_rebalance_finish(void);

/** Slab mover thread.
 * Sits waiting for a condition to jump off and shovel some memory about */
    void _slab_rebalance_thread(void);


/*** item management ***/
    inline uint64_t get_cas_id(void) {
        static uint64_t cas_id = 0;
        return ++cas_id;
    }

    unsigned short refcount_incr(unsigned short *refcount);
    unsigned short refcount_decr(unsigned short *refcount);


    void item_lock(uint32_t hv);
    void item_unlock(uint32_t hv);
//    void *item_trylock(uint32_t hv);
    mutex *item_trylock(uint32_t hv);

    item *do_item_alloc(char *key, const size_t nkey, const int flags, const rel_time_t exptime, const int nbytes, const uint32_t cur_hv);
    void item_free(item *it);
    bool item_size_ok(const size_t nkey, const int flags, const int nbytes);

    /* thread unsafe */
    int  do_item_link(item *it, const uint32_t hv);     /** may fail if transgresses limits */
    void do_item_unlink(item *it, const uint32_t hv);
    void do_item_unlink_nolock(item *it, const uint32_t hv);
    void do_item_remove(item *it);
    void do_item_update(item *it);   /** update LRU time to current and reposition */
    int  do_item_replace(item *it, item *new_it, const uint32_t hv);
    item *do_item_get(const char *key, const size_t nkey, const uint32_t hv);
    int do_store_item(item *it, const uint32_t hv);

    /* thread safe */

    item *item_alloc(char *key, size_t nkey, int flags, int nbytes);

    item *item_get(const char *key, const size_t nkey);
    void item_remove(item *item);
    int item_replace(item *old_it, item *new_it, const uint32_t hv);
    int item_link(item *item);
    void item_unlink(item *item);
    void item_update(item *item);
    int store_item(item *item);

private:
    mutex *cache_lock;
    /****** associative array *******/
    int hash_bulk_move;
    volatile int do_run_maintenance_thread;

    pthread_t assoc_maintenance_tid;
    pthread_cond_t assoc_maintenance_cond;
/* how many powers of 2's worth of buckets we use */
    unsigned int hashpower;
/* Main hash table. This is where we look except during expansion. */
    item** primary_hashtable;
/* Previous hash table. During expansion, we look here for keys that
 * haven't been moved over to the primary yet. */
    item** old_hashtable;
/* Number of items in the hash table */
    unsigned int hash_items;
/* Flag: Are we in the middle of expanding now? */
    bool expanding;
    bool started_expanding;
/* During expansion we migrate values with bucket granularity;
 * this is how far we've gotten so far.
 * Ranges from 0 .. hashsize(hashpower - 1) - 1. */
    unsigned int expand_bucket;

    /****** slabbing *******/
    slabclass_t slabclass[MAX_NUMBER_OF_SLAB_CLASSES];
    size_t mem_limit;
    size_t mem_malloced;
    size_t mem_avail;
    int power_largest;

    void* mem_base;
    void* mem_current;

/* Access to the slab allocator is protected by this lock */
//    pthread_mutex_t slabs_lock;
//  pthread_mutex_t slabs_rebalance_lock;
    mutex *slabs_lock;
    mutex *slabs_rebalance_lock;

    pthread_t slab_rebalance_tid;
    pthread_t slab_maintenance_tid;

    int do_run_slab_thread;
    int do_run_slab_rebalance_thread;

    pthread_cond_t slab_maintenance_cond;
    pthread_cond_t slab_rebalance_cond;

    struct slab_rebalance slab_rebal;
    int slab_rebalance_signal;

    int slab_bulk_check;

    /*** items ***/
#if !defined(HAVE_GCC_ATOMICS) && !defined(__sun)
    mutex *atomics_mutex;;
//    pthread_mutex_t atomics_mutex; /// INIT
#endif
//    pthread_mutex_t init_lock;
    mutex *init_lock;
    pthread_cond_t init_cond;

//    pthread_mutex_t *item_locks;
    mutex **item_locks;
    uint32_t item_lock_count; /* size of the item lock hash table */

    /* this lock is temporarily engaged during a hash table expansion */
//    pthread_mutex_t item_global_lock;
    mutex *item_global_lock;
    /* thread-specific variable for deeply finding the item lock type */
    pthread_key_t item_lock_type_key;

    item *heads[LARGEST_ID];
    item *tails[LARGEST_ID];
    unsigned int sizes[LARGEST_ID];


    /*** metodi privati ***/
    int grow_slab_list (const unsigned int id);
    void split_slab_page_into_freelist(char *ptr, const unsigned int id);

    int do_slabs_newslab(const unsigned int id);
    void *do_slabs_alloc(const size_t size, unsigned int id);
    void do_slabs_free(void *ptr, const size_t size, unsigned int id);
    enum reassign_result_type do_slabs_reassign(int src, int dst);
    void *memory_allocate(size_t size);
    int slabs_reassign_pick_any(int dst);

    void slabs_preallocate (const unsigned int maxslabs);


    size_t item_make_header(const uint8_t nkey, const int flags,
                             const int nbytes, char *suffix, uint8_t *nsuffix);
    void item_link_q(item *it);
    void item_unlink_q(item *it);
};
