#include "assoc.hpp"

pthread_mutex_t cache_lock;

assoc_array::assoc_array(const int hashpower_init) {
    pthread_cond_init(&this->maintenance_cond, NULL);

    this->hash_bulk_move = DEFAULT_HASH_BULK_MOVE;
    this->do_run_maintenance_thread = 1;

//    this->hashpower = HASHPOWER_DEFAULT;
//    this->primary_hashtable = NULL;
    this->old_hashtable = NULL;
    this->hash_items = 0;
    this->expanding = this->started_expanding = false;
    this->expand_bucket = 0;

    this->hashpower = hashpower_init ? hashpower_init : HASHPOWER_DEFAULT;

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

/*
void assoc_array::assoc_init(const int hashpower_init) {
    if (hashpower_init)
        this->hashpower = hashpower_init;

    this->primary_hashtable = (item **) calloc(hashsize(this->hashpower), sizeof(void *));

    if (! this->primary_hashtable) {
        std::cerr << "Failed to init hashtable." << std::endl;
        exit(EXIT_FAILURE);
    }
/// STATS
//    STATS_LOCK();
//    stats.hash_power_level = hashpower;
//    stats.hash_bytes = hashsize(hashpower) * sizeof(void *);
//    STATS_UNLOCK();
} */

item* assoc_array::assoc_find(const char *key, const size_t nkey, const uint32_t hv) {
    item *it, *ret = NULL;
    unsigned int oldbucket;

    if (this->expanding && (oldbucket = (hv & hashmask(this->hashpower - 1))) >= this->expand_bucket)
        it = this->old_hashtable[oldbucket];
    else
        it = this->primary_hashtable[hv & hashmask(this->hashpower)];

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

    if (this->expanding && (oldbucket = (hv & hashmask(this->hashpower - 1))) >= this->expand_bucket) {
        it->h_next = this->old_hashtable[oldbucket];
        this->old_hashtable[oldbucket] = it;
    } else {
        it->h_next = this->primary_hashtable[hv & hashmask(this->hashpower)];
        this->primary_hashtable[hv & hashmask(this->hashpower)] = it;
    }

    this->hash_items++;
    if (! this->expanding && this->hash_items > (hashsize(this->hashpower) * 3) / 2) {
        this->assoc_start_expand();
    }

//    MEMCACHED_ASSOC_INSERT(ITEM_key(it), it->nkey, hash_items); /// TRACE
    return 1;
}

void assoc_array::assoc_delete(const char *key, const size_t nkey, const uint32_t hv) {
    item **before = this->_hashitem_before(key, nkey, hv);

    if (*before) {
        item *nxt;
        hash_items--;
/* The DTrace probe cannot be triggered as the last instruction
 * due to possible tail-optimization by the compiler */
//        MEMCACHED_ASSOC_DELETE(key, nkey, hash_items); /// TRACE
        nxt = (*before)->h_next;
        (*before)->h_next = NULL;   /* probably pointless, but whatever. */
        *before = nxt;
        return;
    }
/* Note:  we never actually get here.  the callers don't delete things
 *  they can't find. */
    assert(*before != 0);
}

item** assoc_array::_hashitem_before (const char *key, const size_t nkey, const uint32_t hv) {
    item **pos;
    unsigned int oldbucket;

    if (this->expanding && (oldbucket = (hv & hashmask(this->hashpower - 1))) >= this->expand_bucket) {
        pos = &this->old_hashtable[oldbucket];
    } else {
        pos = &this->primary_hashtable[hv & hashmask(this->hashpower)];
    }

    while (*pos && ((nkey != (*pos)->nkey) || memcmp(key, ITEM_key(*pos), nkey))) {
        pos = &(*pos)->h_next;
    }

    return pos;
}

void assoc_array::assoc_expand(void) {
    this->old_hashtable = this->primary_hashtable;

    this->primary_hashtable = (item **) calloc(hashsize(this->hashpower + 1), sizeof(void *));
    if (this->primary_hashtable) {
/// SETTINGS
/*      if (settings.verbose > 1)
            std::cerr << "Hash table expansion starting." << std::endl; */
        this->hashpower++;
        this->expanding = true;
        this->expand_bucket = 0;
/// STATS
/*      STATS_LOCK();
        stats.hash_power_level = hashpower;
        stats.hash_bytes += hashsize(hashpower) * sizeof(void *);
        stats.hash_is_expanding = 1;
        STATS_UNLOCK(); */
    } else {
        this->primary_hashtable = this->old_hashtable;
        /* Bad news, but we can keep running. */
    }
}

void assoc_array::assoc_start_expand(void) {
    if (this->started_expanding)
        return;
    this->started_expanding = true;
    pthread_cond_signal(&this->maintenance_cond);
}


//      void do_assoc_move_next_bucket(void);
int assoc_array::start_assoc_maintenance_thread(void) {
    int ret;
    char *env = getenv("MEMCACHED_HASH_BULK_MOVE");

    if (env != NULL) {
        this->hash_bulk_move = atoi(env);
        if (this->hash_bulk_move == 0) {
            this->hash_bulk_move = DEFAULT_HASH_BULK_MOVE;
        }
    }
    if ((ret = pthread_create(&this->maintenance_tid, NULL, assoc_maintenance_thread, (void*) this)) != 0) {
        std::cerr << "Can't create thread: " << strerror(ret) << std::endl;;
        return -1;
    }
    return 0;
}



void assoc_array::stop_assoc_maintenance_thread(void) {
/// FIX
//    mutex_lock(&cache_lock);
    while (pthread_mutex_trylock(&cache_lock))
        ;

    this->do_run_maintenance_thread = 0;
    pthread_cond_signal(&this->maintenance_cond);
/// FIX
    pthread_mutex_unlock(&cache_lock);

    /* Wait for the maintenance thread to stop */
    pthread_join(this->maintenance_tid, NULL);
}


void* assoc_maintenance_thread(void *arg) {
    assoc_array *array = (assoc_array *) arg;

    while (array->get_do_run_maintenance_thread()) {
        int ii = 0;

        /* Lock the cache, and bulk move multiple buckets to the new
         * hash table. */
        item_lock_global();
        mutex_lock(&cache_lock);

        for (ii = 0; ii < array->get_hash_bulk_move() && array->get_expanding(); ++ii) {
            item *it, *next;
            int bucket;

            for (it = array->old_hashtable[array->expand_bucket]; it != NULL; it = next) {
                next = it->h_next;

                bucket = (int) hash::hash_function(ITEM_key(it), it->nkey) & hashmask(array->get_hashpower());
                it->h_next = array->primary_hashtable[bucket];
                array->primary_hashtable[bucket] = it;
            }

            array->old_hashtable[array->expand_bucket] = NULL;
            array->expand_bucket++;

            if (array->get_expand_bucket() == hashsize(array->get_hashpower() - 1)) {
                array->expanding = false;
                free(array->get_old_hashtable());
/// STATS
/*              STATS_LOCK();
                stats.hash_bytes -= hashsize(hashpower - 1) * sizeof(void *);
                stats.hash_is_expanding = 0;
                STATS_UNLOCK(); */
/// SETTINGS
/*              if (settings.verbose > 1)
                    std::cerr << "Hash table expansion done." << endl; */
            }
        }

        mutex_unlock(&cache_lock);
        item_unlock_global();

        if (!array->get_expanding()) {
            /* finished expanding. tell all threads to use fine-grained locks */
            switch_item_lock_type(ITEM_LOCK_GRANULAR);
            slabs_rebalancer_resume();
            /* We are done expanding.. just wait for next invocation */
            mutex_lock(&cache_lock);
            array->started_expanding = false;
            pthread_cond_wait(&array->get_maintenance_cond(), &cache_lock);
            /* Before doing anything, tell threads to use a global lock */
            mutex_unlock(&cache_lock);
            slabs_rebalancer_pause();
            switch_item_lock_type(ITEM_LOCK_GLOBAL);
            mutex_lock(&cache_lock);
            array->assoc_expand();
            mutex_unlock(&cache_lock);
        }
    }

    return NULL;
}

