#ifndef ASSOC_HPP
#define ASSOC_HPP

#include <malloc.h>
#include <pthread.h>

#include "lru.hpp"
#include "const_types.h"

#define hashsize(n) ((uint32_t)1<<(n))
#define hashmask(n) (hashsize(n)-1)

/** previous declarations **/
class LRU_Queues;

class Assoc {
private:
    LRU_Queues *lru;

    unsigned int hashpower; // how many powers of 2's worth of buckets we use
    unsigned int hash_items; // Number of items in the hash table.

    // Main hash table. This is where we look except during expansion.
    hash_item** primary_hashtable;

    /* Previous hash table. During expansion, we look here for keys that haven't
     * been moved over to the primary yet. */
    hash_item** old_hashtable;

    bool expanding; // Flag: Are we in the middle of expanding now?

    /* During expansion we migrate values with bucket granularity;
     * this is how far we've gotten so far.
     * Ranges from 0 .. hashsize(hashpower - 1) - 1. */
    unsigned int expand_bucket;

    void assoc_expand();
    hash_item** hashitem_before(const uint32_t hash, const char *key, const size_t nkey);

    inline bool which_hashtable(const uint32_t hash, unsigned int &bucket) {
        return (this->expanding && (bucket = hash & hashmask(this->hashpower-1)) >= this->expand_bucket);
    }

public:
    Assoc(LRU_Queues *lru, unsigned int hashpower);

    hash_item *assoc_find(const uint32_t hash, const char *key, const size_t nkey);
    int assoc_insert(const uint32_t hash, hash_item *it);
    void assoc_delete(const uint32_t hash, const char *key, const size_t nkey);
    void assoc_maintenance_thread();
};
#endif
