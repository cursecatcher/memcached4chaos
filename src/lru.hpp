#ifndef LRU_HPP
#define LRU_HPP

#include <cassert>
#include "hash.h"
#include "items_op.hpp"
#include "datacache.hpp"

/** previous declarations **/
class Assoc;
class DataCache;


class LRU {
private:
    DataCache* engine;
    pthread_mutex_t cache_lock;

    hash_item *heads[POWER_LARGEST];
    hash_item *tails[POWER_LARGEST];
    unsigned int sizes[POWER_LARGEST];


    void item_link_q(hash_item *it);
    void item_unlink_q(hash_item *it);
    hash_item *do_item_alloc(const char *key, const size_t nkey,
                             const int flags, const int nbytes);
    hash_item *do_item_get(const char *key, const size_t nkey);
    int do_item_link(hash_item *it);
    void do_item_unlink(hash_item *it);
    void do_item_release(hash_item *it);
    void do_item_update(hash_item *it);
    int do_item_replace(hash_item *it, hash_item *new_it);
    void item_free(hash_item *it);
    ENGINE_ERROR_CODE do_store_item(hash_item *it);

public:
    LRU(DataCache *engine);

    /** Allocate and initialize a new item structure
     * @param engine handle to the storage engine
     * @param key the key for the new item
     * @param nkey the number of bytes in the key
     * @param flags the flags in the new item
     * @param nbytes the number of bytes in the body for the item
     * @return a pointer to an item on success or NULL otherwise */
    hash_item *item_alloc(const char *key, size_t nkey, int flags, int nbytes);

    /** Get an item from the cache
     * @param engine handle to the storage engine
     * @param key the key for the item to get
     * @param nkey the number of bytes in the key
     * @return pointer to the item if it exists or NULL otherwise */
    hash_item *item_get(const char *key, const size_t nkey);

    /** Release our reference to the current item
     * @param engine handle to the storage engine
     * @param it the item to release */
    void item_release(hash_item *it);

    /** Unlink the item from the hash table (make it inaccessible)
     * @param engine handle to the storage engine
     * @param it the item to unlink */
    void item_unlink(hash_item *it);

    /** Store an item in the cache
     * @param engine handle to the storage engine
     * @param item the item to store
     * @param cas the cas value (OUT)
     * @return ENGINE_SUCCESS on success
     *
     * @todo should we refactor this into hash_item ** and remove the cas
     *       there so that we can get it from the item instead? */
    ENGINE_ERROR_CODE store_item(hash_item *it);

    inline void lock_cache() { pthread_mutex_lock(&this->cache_lock); }
    inline void unlock_cache() { pthread_mutex_unlock(&this->cache_lock); }
};
#endif
