#include <cassert>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <pthread.h>
#include <iostream>

#include "defines.h"

extern pthread_mutex_t cache_lock;

class assoc_array {
    private:
        int hash_bulk_move;
        volatile int do_run_maintenance_thread;

        pthread_t maintenance_tid;
        pthread_cond_t maintenance_cond;
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

    public:
        assoc_array();

        void assoc_init(const int hashpower_init);
        item *assoc_find(const char *key, const size_t nkey, const uint32_t hv);
        int assoc_insert(item *it, const uint32_t hv);
        void assoc_delete(const char *key, const size_t nkey, const uint32_t hv);

        item** _hashitem_before (const char *key, const size_t nkey, const uint32_t hv);
        void assoc_expand(void);
        void assoc_start_expand(void);

//      void do_assoc_move_next_bucket(void);
        int start_assoc_maintenance_thread(void);
        void stop_assoc_maintenance_thread(void);

        void* assoc_maintenance_thread(void *arg);
};
