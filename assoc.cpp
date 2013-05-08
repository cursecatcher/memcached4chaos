#include "assoc.hpp"

assoc_array::assoc_array() {
    pthread_cond_init(&this->maintenance_cond, NULL);
    this->hashpower = HASHPOWER_DEFAULT;
    this->primary_hashtable = NULL;
    this->old_hashtable = NULL;
    this->hash_items = 0;
    this->expanding = this->started_expanding = false;
    this->expand_bucket = 0;
}

void assoc_array::assoc_init(const int hashpower_init) {
    if (hashpower_init)
        this->hashpower = hashpower_init;

    this->primary_hashtable = (item **) calloc(hashsize(this->hashpower), sizeof(void *));

    if (! this->primary_hashtable) {
        std::cerr << "Failed to init hashtable." << std::endl;
        exit(EXIT_FAILURE);
    }
/// STATS
/*
    STATS_LOCK();
    stats.hash_power_level = hashpower;
    stats.hash_bytes = hashsize(hashpower) * sizeof(void *);
    STATS_UNLOCK();
*/
}

item* assoc_array::assoc_find(const char *key, const size_t nkey, const uint32_t hv) {
    item *it, *ret = NULL;
    unsigned int oldbucket;

    if (expanding && (oldbucket = (hv & hashmask(hashpower - 1))) >= expand_bucket)
        it = old_hashtable[oldbucket];
    else
        it = primary_hashtable[hv & hashmask(hashpower)];

//    int depth = 0; /// TRACE

    while (it) {
        if ((nkey == it->nkey) && (memcmp(key, ITEM_key(it), nkey) == 0)) {
            ret = it;
            break;
        }
        it = it->h_next;
//        ++depth; /// TRACE
    }

//    MEMCACHED_ASSOC_FIND(key, nkey, depth); /// TRACE
    return ret;
}

/* Note: this isn't an assoc_update.  The key must not already exist to call this */
int assoc_array::assoc_insert(item *it, const uint32_t hv) {
    unsigned int oldbucket;

//    assert(assoc_find(ITEM_key(it), it->nkey) == 0);  /* shouldn't have duplicately named things defined */

    if (expanding && (oldbucket = (hv & hashmask(hashpower - 1))) >= expand_bucket) {
        it->h_next = old_hashtable[oldbucket];
        old_hashtable[oldbucket] = it;
    } else {
        it->h_next = primary_hashtable[hv & hashmask(hashpower)];
        primary_hashtable[hv & hashmask(hashpower)] = it;
    }

    hash_items++;
    if (! expanding && hash_items > (hashsize(hashpower) * 3) / 2) {
        assoc_start_expand();
    }

//    MEMCACHED_ASSOC_INSERT(ITEM_key(it), it->nkey, hash_items); /// TRACE
    return 1;
}

void assoc_array::assoc_delete(const char *key, const size_t nkey, const uint32_t hv) {
    ;
}

void assoc_array::do_assoc_move_next_bucket(void) {
    ;
}

int assoc_array::start_assoc_maintenance_thread(void) {
    return 0;
}

void assoc_array::stop_assoc_maintenance_thread(void) {
    ;
}
