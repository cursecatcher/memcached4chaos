#include <cassert>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <pthread.h>
#include <iostream>

#include "hash.hpp"
#include "defines.h"

using namespace hash;

extern pthread_mutex_t cache_lock;

void* assoc_maintenance_thread(void *arg);

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
        assoc_array(const int hashpower_init);

        inline int get_hash_bulk_move() { return this->hash_bulk_move; }
        inline int get_do_run_maintenance_thread() { return this->do_run_maintenance_thread; }
        inline pthread_cond_t get_maintenance_cond() { return this->maintenance_cond; }
        inline unsigned int get_hashpower() { return this->hashpower; }
        inline item** get_primary_hashtable() { return this->primary_hashtable; }
        inline item* get_primary_hashtable(int bucket) { return this->primary_hashtable[bucket]; }
        inline item** get_old_hashtable() { return this->old_hashtable; }
        inline item* get_old_hashtable(int bucket) { return this->old_hashtable[bucket]; }
        inline unsigned int get_hash_items() { return this->hash_items; }
        inline bool get_expanding() { return this->expanding; }
        inline bool get_started_expanding() { return this->started_expanding; }
        inline unsigned int get_expand_bucket() { return this->expand_bucket; }

        inline void set_hash_bulk_move(int nv) { this->hash_bulk_move = nv; }
        inline void set_do_run_maintenance_thread(int nv) { this->do_run_maintenance_thread = nv; }
        inline void set_get_maintenance_cond(pthread_cond_t nv);
        inline void set_hashpower(unsigned int nv) { this->hashpower = nv; }
        inline void set_primary_hashtable(...);
        inline void set_old_hashtable(...);
        inline void set_hash_items(unsigned int nv) {this->hash_items = nv; }
        inline void set_expanding(bool nv) { this->expanding = nv; }
        inline void set_started_expanding(bool nv) {this->started_expanding = nv; }
        inline void set_expand_bucket(unsigned int nv) { this->expand_bucket = nv; }

//        void assoc_init(const int hashpower_init);
        item *assoc_find(const char *key, const size_t nkey, const uint32_t hv);
        int assoc_insert(item *it, const uint32_t hv);
        void assoc_delete(const char *key, const size_t nkey, const uint32_t hv);

        item** _hashitem_before (const char *key, const size_t nkey, const uint32_t hv);
        void assoc_expand(void);
        void assoc_start_expand(void);

//      void do_assoc_move_next_bucket(void);
        int start_assoc_maintenance_thread(void);
        void stop_assoc_maintenance_thread(void);
};
