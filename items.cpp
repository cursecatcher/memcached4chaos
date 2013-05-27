#include "items.hpp"

items_management::items_management() {

}

unsigned short items_management::refcount_incr(unsigned short *refcount) {
#ifdef HAVE_GCC_ATOMICS
    return __sync_add_and_fetch(refcount, 1);
#elif defined(__sun)
    return atomic_inc_ushort_nv(refcount);
#else
    unsigned short res;
    mutex_lock(&atomics_mutex);
    (*refcount)++;
    res = *refcount;
    mutex_unlock(&atomics_mutex);
    return res;
#endif
}

unsigned short items_management::refcount_decr(unsigned short *refcount) {
#ifdef HAVE_GCC_ATOMICS
    return __sync_sub_and_fetch(refcount, 1);
#elif defined(__sun)
    return atomic_dec_ushort_nv(refcount);
#else
    unsigned short res;
    mutex_lock(&atomics_mutex);
    (*refcount)--;
    res = *refcount;
    mutex_unlock(&atomics_mutex);
    return res;
#endif
}

int mutex_lock(pthread_mutex_t *lock) {
    while (pthread_mutex_trylock(lock));
    return 0;
}


void items_management::item_lock(uint32_t hv) {
    uint8_t *lock_type = (uint8_t *) pthread_getspecific(this->item_lock_type_key);

//    if (likely(*lock_type == ITEM_LOCK_GRANULAR))
    if (*lock_type == ITEM_LOCK_GRANULAR)
        mutex_lock(&this->item_locks[(hv & hashmask(hashpower)) % this->item_lock_count]);
    else
        mutex_lock(&this->item_global_lock);
}

void items_management::item_unlock(uint32_t hv) {
    uint8_t *lock_type = (uint8_t *) pthread_getspecific(this->item_lock_type_key);

//    if (likely(*lock_type == ITEM_LOCK_GRANULAR))
    if (*lock_type == ITEM_LOCK_GRANULAR)
        mutex_unlock(&this->item_locks[(hv & hashmask(hashpower)) % this->item_lock_count]);
    else
        mutex_unlock(&this->item_global_lock);
}

void *items_management::item_trylock(uint32_t hv) {
    pthread_mutex_t *lock =
        &this->item_locks[(hv & hashmask(hashpower)) % this->item_lock_count];

    return (pthread_mutex_trylock(lock) == 0 ? lock : NULL);
}

void items_management::item_trylock_unlock(void *lock) {
    pthread_mutex_unlock((pthread_mutex_t *) lock);
}



/**
 * Generates the variable-sized part of the header for an object.
 *
 * key     - The key
 * nkey    - The length of the key
 * flags   - key flags
 * nbytes  - Number of bytes to hold value and addition CRLF terminator
 * suffix  - Buffer for the "VALUE" line suffix (flags, size).
 * nsuffix - The length of the suffix is stored here.
 *
 * Returns the total size of the header.
 */
size_t items_management::
    item_make_header(const uint8_t nkey, const int flags, const int nbytes,
                     char *suffix, uint8_t *nsuffix) {
    /* suffix is defined at 40 chars elsewhere.. */
    *nsuffix = (uint8_t) snprintf(suffix, 40, " %d %d\r\n", flags, nbytes - 2);
    return sizeof(item) + nkey + *nsuffix + nbytes;
}


void items_management::item_link_q(item *it) { /* item is the new head */
    item **head, **tail;

    assert(it->slabs_clsid < LARGEST_ID);
    assert((it->it_flags & ITEM_SLABBED) == 0);

    head = &this->heads[it->slabs_clsid];
    tail = &this->tails[it->slabs_clsid];

    assert(it != *head);
    assert((*head && *tail) || (*head == 0 && *tail == 0));

    it->prev = 0;
    it->next = *head;

    if (it->next)
        it->next->prev = it;

    *head = it;

    if (*tail == 0)
        *tail = it;

    this->sizes[it->slabs_clsid]++;

    return;
}

void items_management::item_unlink_q(item *it) {
    item **head, **tail;

    assert(it->slabs_clsid < LARGEST_ID);

    head = &this->heads[it->slabs_clsid];
    tail = &this->tails[it->slabs_clsid];

    if (*head == it) {
        assert(it->prev == 0);
        *head = it->next;
    }
    if (*tail == it) {
        assert(it->next == 0);
        *tail = it->prev;
    }

    assert(it->next != it);
    assert(it->prev != it);

    if (it->next)
        it->next->prev = it->prev;
    if (it->prev)
        it->prev->next = it->next;

    this->sizes[it->slabs_clsid]--;

    return;
}


item *items_management::
    do_item_alloc(char *key, const size_t nkey, const int flags,
                  const rel_time_t exptime, const int nbytes,
                  const uint32_t cur_hv) {
    uint8_t nsuffix;
    item *it = NULL;
    char suffix[40];
    size_t ntotal = this->item_make_header(nkey + 1, flags, nbytes, suffix, &nsuffix);
    unsigned int id;

    if (settings.use_cas) {
        ntotal += sizeof(uint64_t);
    }

    if ((id = this->slabbing->slabs_clsid(ntotal)) == 0) /// SLABS
        return 0;

    mutex_lock(&cache_lock);
    /* do a quick check if we have any expired items in the tail.. */
    int tries = 5;
    int tried_alloc = 0;
    item *search;
    void *hold_lock = NULL;
    rel_time_t oldest_live = settings.oldest_live;

    search = this->tails[id];
    /* We walk up *only* for locked items. Never searching for expired.
     * Waste of CPU for almost all deployments */
    for (; tries > 0 && search != NULL; tries--, search=search->prev) {
        uint32_t hv = hash(ITEM_key(search), search->nkey);
        /* Attempt to hash item lock the "search" item. If locked, no
         * other callers can incr the refcount
         */
        /* FIXME: I think we need to mask the hv here for comparison? */
        if (hv != cur_hv && (hold_lock = item_trylock(hv)) == NULL)
            continue;
        /* Now see if the item is refcount locked */
        if (refcount_incr(&search->refcount) != 2) { /// THREADS
            refcount_decr(&search->refcount);
            /* Old rare bug could cause a refcount leak. We haven't seen
             * it in years, but we leave this code in to prevent failures
             * just in case */
            if (search->time + TAIL_REPAIR_TIME < current_time) {
                search->refcount = 1;
                this->do_item_unlink_nolock(search, hv);
            }
            if (hold_lock)
                item_trylock_unlock(hold_lock); /// THREADS
            continue;
        }

        /* Expired or flushed */
        if ((search->exptime != 0 && search->exptime < current_time)
            || (search->time <= oldest_live && oldest_live <= current_time)) {

            it = search;
            this->slabbing->slabs_adjust_mem_requested(it->slabs_clsid, ITEM_ntotal(it), ntotal); /// SLABS
            this->do_item_unlink_nolock(it, hv);
            /* Initialize the item block: */
            it->slabs_clsid = 0;
        }
        else if ((it = (item*) this->slabbing->slabs_alloc(ntotal, id)) == NULL) { /// SLABS
            tried_alloc = 1;

            if (settings.evict_to_free != 0) {
                it = search;
                this->slabbing->slabs_adjust_mem_requested(it->slabs_clsid, ITEM_ntotal(it), ntotal); /// STATS
                this->do_item_unlink_nolock(it, hv);
                /* Initialize the item block: */
                it->slabs_clsid = 0;

                /* If we've just evicted an item, and the automover is set to
                 * angry bird mode, attempt to rip memory into this slab class.
                 * TODO: Move valid object detection into a function, and on a
                 * "successful" memory pull, look behind and see if the next alloc
                 * would be an eviction. Then kick off the slab mover before the
                 * eviction happens.
                 */
                if (settings.slab_automove == 2)
                    this->slabbing->slabs_reassign(-1, id); /// SLABS
            }
        }

        refcount_decr(&search->refcount); /// THREADS
        /* If hash values were equal, we don't grab a second lock */
        if (hold_lock)
            item_trylock_unlock(hold_lock); /// THREADS
        break;
    }

    if (!tried_alloc && (tries == 0 || search == NULL))
        it = (item*) this->slabbing->slabs_alloc(ntotal, id); /// SLABS

    if (it == NULL) {
        mutex_unlock(&cache_lock);
        return NULL;
    }

    assert(it->slabs_clsid == 0);
    assert(it != this->heads[id]);

    /* Item initialization can happen outside of the lock; the item's already
     * been removed from the slab LRU.
     */
    it->refcount = 1;     /* the caller will have a reference */
    mutex_unlock(&cache_lock);
    it->next = it->prev = it->h_next = 0;
    it->slabs_clsid = id;

    it->it_flags = settings.use_cas ? ITEM_CAS : 0; /// SETTINGS
    it->nkey = nkey;
    it->nbytes = nbytes;
    memcpy(ITEM_key(it), key, nkey);
    it->exptime = exptime;
    memcpy(ITEM_suffix(it), suffix, (size_t)nsuffix);
    it->nsuffix = nsuffix;

    return it;
}

void items_management::item_free(item *it) {
    size_t ntotal = ITEM_ntotal(it);
    unsigned int clsid;

    assert((it->it_flags & ITEM_LINKED) == 0);
    assert(it != this->heads[it->slabs_clsid]);
    assert(it != this->tails[it->slabs_clsid]);
    assert(it->refcount == 0);

    /* so slab size changer can tell later if item is already free or not */
    clsid = it->slabs_clsid;
    it->slabs_clsid = 0;
    this->slabbing->slabs_free(it, ntotal, clsid);
}

bool items_management::
    item_size_ok(const size_t nkey, const int flags, const int nbytes) {

    char prefix[40];
    uint8_t nsuffix;
    size_t ntotal =
        this->item_make_header(nkey + 1, flags, nbytes, prefix, &nsuffix) +
        (settings.use_cas ? sizeof(uint64_t) : 0);

    return this->slabbing->slabs_clsid(ntotal) != 0;
}

/** may fail if transgresses limits */
int items_management::do_item_link(item *it, const uint32_t hv) {
    assert((it->it_flags & (ITEM_LINKED|ITEM_SLABBED)) == 0);
    mutex_lock(&cache_lock);

    it->it_flags |= ITEM_LINKED;
    it->time = current_time;

    /* Allocate a new CAS ID on link. */
    ITEM_set_cas(it, (settings.use_cas) ? get_cas_id() : 0);

    this->associative->assoc_insert(it, hv);
    this->item_link_q(it);

    refcount_incr(&it->refcount);
    mutex_unlock(&cache_lock);

    return 1;
}

void items_management::do_item_unlink(item *it, const uint32_t hv) {
    mutex_lock(&cache_lock);
    if ((it->it_flags & ITEM_LINKED) != 0) {
        it->it_flags &= ~ITEM_LINKED;
        this->associative->assoc_delete(ITEM_key(it), it->nkey, hv);
        this->item_unlink_q(it);
        this->do_item_remove(it);
    }
    mutex_unlock(&cache_lock);
}

void items_management::do_item_unlink_nolock(item *it, const uint32_t hv) {
    if ((it->it_flags & ITEM_LINKED) != 0) {
        it->it_flags &= ~ITEM_LINKED;
        this->associative->assoc_delete(ITEM_key(it), it->nkey, hv);
        this->item_unlink_q(it);
        this->do_item_remove(it);
    }
}

void items_management::do_item_remove(item *it) {
    assert((it->it_flags & ITEM_SLABBED) == 0);

    if (refcount_decr(&it->refcount) == 0)
        this->item_free(it);
}

/** update LRU time to current and reposition */
void items_management::do_item_update(item *it) {
    if (it->time < current_time - ITEM_UPDATE_INTERVAL) {
        assert((it->it_flags & ITEM_SLABBED) == 0);

        mutex_lock(&cache_lock);
        if ((it->it_flags & ITEM_LINKED) != 0) {
            this->item_unlink_q(it);
            it->time = current_time;
            this->item_link_q(it);
        }
        mutex_unlock(&cache_lock);
    }
}

int items_management::do_item_replace(item *it, item *new_it, const uint32_t hv) {
    assert((it->it_flags & ITEM_SLABBED) == 0);
    this->do_item_unlink(it, hv);
    return this->do_item_link(new_it, hv);
}

item *items_management::
    do_item_get(const char *key, const size_t nkey, const uint32_t hv) {
    //mutex_lock(&cache_lock);
    item *it = this->associative->assoc_find(key, nkey, hv);

    if (it != NULL) {
        refcount_incr(&it->refcount);
        /* Optimization for slab reassignment. prevents popular items from
         * jamming in busy wait. Can only do this here to satisfy lock order
         * of item_lock, cache_lock, slabs_lock. */
        if (this->slabbing->get_slab_rebalance_signal() && /// SLABS
            ((void *)it >= this->slabbing->get_slab_rebal().slab_start &&
            (void *)it < this->slabbing->get_slab_rebal().slab_end)) {

            this->do_item_unlink_nolock(it, hv);
            this->do_item_remove(it);
            it = NULL;
        }
    }
    //mutex_unlock(&cache_lock);
    int was_found = 0;

    if (settings.verbose > 2) {
        if (it == NULL) {
            std::cerr << "> NOT FOUND " << key;
        } else {
            std::cerr << "> FOUND KEY " << ITEM_key(it);
            was_found++;
        }
    }

    if (it != NULL) {
        if (settings.oldest_live != 0 &&
            settings.oldest_live <= current_time &&
            it->time <= settings.oldest_live) {

            this->do_item_unlink(it, hv);
            this->do_item_remove(it);

            it = NULL;

            if (was_found)
                std::cerr << " -nuked by flush";

        } else if (it->exptime != 0 && it->exptime <= current_time) {
            this->do_item_unlink(it, hv);
            this->do_item_remove(it);

            it = NULL;

            if (was_found)
                std::cerr << " -nuked by expire";

        } else {
            it->it_flags |= ITEM_FETCHED;
        }
    }

    if (settings.verbose > 2)
        std::cerr << std::endl;

    return it;
}

int items_management::do_store_item(item *it, const uint32_t hv) {
    item *old_it = this->do_item_get(ITEM_key(it), it->nkey, hv);
    int stored;

    if (old_it != NULL) {
        stored = this->item_replace(old_it, it, hv);
        this->do_item_remove(old_it);
    }
    else {
        stored = this->do_item_link(it, hv); //item_link??
    }

    return stored;
}


item *items_management::item_alloc(char *key, size_t nkey, int flags, int nbytes) {
    item *it;
    /* do_item_alloc handles its own locks */
    it = this->do_item_alloc(key, nkey, flags, (rel_time_t) 0, nbytes, 0);

    return it;
}

item *items_management::item_get(const char *key, const size_t nkey) {
    item *it;
    uint32_t hv = hash(key, nkey);

    this->item_lock(hv);
    it = this->do_item_get(key, nkey, hv);
    this->item_unlock(hv);

    return it;
}

void items_management::item_remove(item *item) {
    uint32_t hv;
    hv = hash(ITEM_key(item), item->nkey);

    this->item_lock(hv);
    this->do_item_remove(item);
    this->item_unlock(hv);
}

int items_management::item_replace(item *old_it, item *new_it, const uint32_t hv) {
	return this->do_item_replace(old_it, new_it, hv);
}

int items_management::item_link(item *item) {
    int ret;
    uint32_t hv = hash(ITEM_key(item), item->nkey);

    this->item_lock(hv);
    ret = this->do_item_link(item, hv);
    this->item_unlock(hv);

    return ret;
}

void items_management::item_unlink(item *item) {
    uint32_t hv = hash(ITEM_key(item), item->nkey);

    this->item_lock(hv);
    this->do_item_unlink(item, hv);
    this->item_unlock(hv);
}

void items_management::item_update(item *item) {
    uint32_t hv = hash(ITEM_key(item), item->nkey);

    this->item_lock(hv);
    this->do_item_update(item);
    this->item_unlock(hv);
}

int items_management::store_item(item *item) {
    int ret;
    uint32_t hv = hash(ITEM_key(item), item->nkey);

    this->item_lock(hv);
    ret = this->do_store_item(item, hv);
    this->item_unlock(hv);

    return ret;
}

