/* *alloc */
#include <cassert>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <pthread.h>
#include <iostream>

#include "defines.h"

typedef unsigned int rel_time_t;

typedef struct _stritem {
    struct _stritem *next;
    struct _stritem *prev;
    struct _stritem *h_next;    /* hash chain next */
    rel_time_t      time;       /* least recent access */
    rel_time_t      exptime;    /* expire time */
    int             nbytes;     /* size of data */
    unsigned short  refcount;
    uint8_t         nsuffix;    /* length of flags-and-length string */
    uint8_t         it_flags;   /* ITEM_* above */
    uint8_t         slabs_clsid;/* which slab class we're in */
    uint8_t         nkey;       /* key length, w/terminating null and padding */
    /* this odd type prevents type-punning issues when we do
     * the little shuffle to save space when not using CAS. */
    union {
        uint64_t cas;
        char end;
    } data[];
    /* if it_flags & ITEM_CAS we have 8 bytes CAS */
    /* then null-terminated key */
    /* then " flags length\r\n" (no terminating null) */
    /* then data with terminating \r\n (no terminating null; it's binary!) */
} item;

pthread_mutex_t cache_lock;


class assoc_array {
    private:
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

        void *assoc_maintenance_thread(void *arg);

        item** _hashitem_before (const char *key, const size_t nkey, const uint32_t hv);
        void assoc_expand(void);
        void assoc_start_expand(void);

        void assoc_init(const int hashpower_init);
        item *assoc_find(const char *key, const size_t nkey, const uint32_t hv);
        int assoc_insert(item *it, const uint32_t hv);
        void assoc_delete(const char *key, const size_t nkey, const uint32_t hv);
//      void do_assoc_move_next_bucket(void);
        int start_assoc_maintenance_thread(void);
        void stop_assoc_maintenance_thread(void);


};
