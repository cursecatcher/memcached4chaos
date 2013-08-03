#ifndef LRU_HPP
#define LRU_HPP

#include <cassert>
#include <pthread.h>

#include "assoc.hpp"
#include "slabs.hpp"
#include "const_types.h"
#include "hash.h"

/** previous declarations **/
class Assoc;
class Slabs;


class LRU_Queues {
private:
    struct config settings;
    Assoc *assoc;
    Slabs *slabs;

    pthread_mutex_t cache_lock;

    hash_item *heads[POWER_LARGEST];
    hash_item *tails[POWER_LARGEST];
    unsigned int sizes[POWER_LARGEST];


    void item_link_q(hash_item *it);
    void item_unlink_q(hash_item *it);
    hash_item *do_item_alloc(const char *key, const size_t nkey, const int nbytes);
    hash_item *do_item_get(const char *key, const size_t nkey);
    int do_item_link(hash_item *it);
    void do_item_unlink(hash_item *it);
    void do_item_release(hash_item *it);
    void do_item_update(hash_item *it);
    int do_item_replace(hash_item *it, hash_item *new_it);
    void item_free(hash_item *it);
    void do_store_item(hash_item *it);

public:
    LRU_Queues(const struct config settings);

    /** Allocate and initialize a new item structure
     * @param key the key for the new item
     * @param nkey the number of bytes in the key
     * @param nbytes the number of bytes in the body for the item
     * @return a pointer to an item on success or NULL otherwise */
    inline hash_item *item_alloc(const char *key, size_t nkey, int nbytes) {
        hash_item *it;

        this->lock_cache();
        it = this->do_item_alloc(key, nkey, nbytes);
        this->unlock_cache();
        return it;
    }

    /** Get an item from the cache
     * @param key the key for the item to get
     * @param nkey the number of bytes in the key
     * @return pointer to the item if it exists or NULL otherwise */
    inline hash_item *item_get(const char *key, const size_t nkey) {
        hash_item *it;

        this->lock_cache();
        it = this->do_item_get(key, nkey);
        this->unlock_cache();
        return it;
    }

    /** Release our reference to the current item
     * @param it the item to release */
    inline void item_release(hash_item *it) {
        this->lock_cache();
        this->do_item_release(it);
        this->unlock_cache();
    }

    /** Unlink the item from the hash table (make it inaccessible)
     * @param it the item to unlink */
    inline void item_unlink(hash_item *it) {
        this->lock_cache();
        this->do_item_unlink(it);
        this->unlock_cache();
    }

    /** Store an item in the cache
     * @param item the item to store */
    inline void store_item(hash_item *it) {
        this->lock_cache();
        this->do_store_item(it);
        this->unlock_cache();
    }

    inline char* item_get_key(const hash_item* item) {
        return (char*) (item + 1);
    }
    inline void* item_get_data(const hash_item* item) {
        return ((char *) this->item_get_key(item)) + item->nkey;
    }
    inline size_t ITEM_ntotal(const hash_item *item) {
        return (sizeof(hash_item) + item->nkey + item->nbytes);
    }
    inline void lock_cache() {
        pthread_mutex_lock(&this->cache_lock);
    }
    inline void unlock_cache() {
        pthread_mutex_unlock(&this->cache_lock);
    }
    inline rel_time_t get_current_time() {
        return (rel_time_t) time(NULL);
    }
};
#endif
