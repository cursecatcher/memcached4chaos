#include <cstdio>
#include "assoc.hpp"

#define LARGEST_ID POWER_LARGEST
#define hashpower 1 /*** eia, fidati ***/

typedef struct {
    uint64_t evicted;
    uint64_t evicted_nonzero;
    rel_time_t evicted_time;
    uint64_t reclaimed;
    uint64_t outofmemory;
    uint64_t tailrepairs;
    uint64_t expired_unfetched;
    uint64_t evicted_unfetched;
} itemstats_t;

itemstats_t itemstats[LARGEST_ID];


class items_management {
    slab_allocator *slabbing;
    assoc_array *associative;

    pthread_mutex_t *item_locks;
    uint32_t item_lock_count; /* size of the item lock hash table */

    item *heads[LARGEST_ID];
    item *tails[LARGEST_ID];
    unsigned int sizes[LARGEST_ID];


    size_t item_make_header(const uint8_t nkey, const int flags,
                             const int nbytes, char *suffix, uint8_t *nsuffix);

    void item_link_q(item *it);
    void item_unlink_q(item *it);

public:
    items_management();

    inline uint64_t get_cas_id(void) {
        static uint64_t cas_id = 0;
        return ++cas_id;
    }

    unsigned short refcount_incr(unsigned short *refcount);
    unsigned short refcount_decr(unsigned short *refcount);

    void *item_trylock(uint32_t hv);
    void item_trylock_unlock(void *lock);

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

    /* thread safe */

    item *item_alloc(char *key, size_t nkey, int flags, rel_time_t exptime, int nbytes);

//    enum store_item_type store_item(item *item, int comm, conn* c);
    item *item_get(const char *key, const size_t nkey);
    void item_remove(item *item);
    int item_replace(item *old_it, item *new_it, const uint32_t hv);
    int item_link(item *item);
    void item_unlink(item *item);
    void item_update(item *item);
};
