#include "lru.hpp"


LRU::LRU(DataCache *engine) {
    this->engine = engine;
    pthread_mutex_init(&this->cache_lock, NULL);

    for (int i = 0; i < POWER_LARGEST; i++) {
        this->heads[i] = this->tails[i] = NULL;
        this->sizes[i] = 0;
    }
}

hash_item *LRU::item_alloc(const char *key, size_t nkey, int nbytes) {
    hash_item *it;

    this->lock_cache();
    it = this->do_item_alloc(key, nkey, nbytes);
    this->unlock_cache();
    return it;
}

hash_item *LRU::item_get(const char *key, const size_t nkey) {
    hash_item *it;

    this->lock_cache();
    it = this->do_item_get(key, nkey);
    this->unlock_cache();
    return it;
}

void LRU::item_release(hash_item *it) {
    this->lock_cache();
    this->do_item_release(it);
    this->unlock_cache();
}

void LRU::item_unlink(hash_item *it) {
    this->lock_cache();
    this->do_item_unlink(it);
    this->unlock_cache();
}

ENGINE_ERROR_CODE LRU::store_item(hash_item *it) {
    ENGINE_ERROR_CODE ret;

    this->lock_cache();
    ret = this->do_store_item(it);
    this->unlock_cache();
    return ret;
}



void LRU::item_link_q(hash_item *it) {
    hash_item **head, **tail;

    assert(it->slabs_clsid < POWER_LARGEST);
    assert((it->iflag & ITEM_SLABBED) == 0);

    head = &this->heads[it->slabs_clsid];
    tail = &this->tails[it->slabs_clsid];

    assert(it != *head);
    assert((*head && *tail) || (*head == NULL && *tail == NULL));

    it->prev = NULL;
    it->next = *head;
    if (it->next)
        it->next->prev = it;
    *head = it;
    if (*tail == NULL)
        *tail = it;
    this->sizes[it->slabs_clsid]++;
}

void LRU::item_unlink_q(hash_item *it) {
    hash_item **head, **tail;

    assert(it->slabs_clsid < POWER_LARGEST);

    head = &this->heads[it->slabs_clsid];
    tail = &this->tails[it->slabs_clsid];

    if (*head == it) {
        assert(it->prev == NULL);
        *head = it->next;
    }
    if (*tail == it) {
        assert(it->next == NULL);
        *tail = it->prev;
    }
    assert(it->next != it);
    assert(it->prev != it);

    if (it->next)
        it->next->prev = it->prev;
    if (it->prev)
        it->prev->next = it->next;
    this->sizes[it->slabs_clsid]--;
}

hash_item *LRU::do_item_alloc(const char *key, const size_t nkey, const int nbytes) {
    hash_item *it = NULL;
    size_t ntotal = sizeof(hash_item) + nkey + nbytes;
    unsigned int id;

    if (this->engine->config.use_cas)
        ntotal += sizeof(uint64_t);

    if ((id = this->engine->slabs->slabs_clsid(ntotal)) == 0)
        return NULL;

    if ((it = (hash_item*) this->engine->slabs->slabs_alloc(ntotal, id)) == NULL) {
        /* Could not find an expired item at the tail, and memory allocation
         * failed. Try to evict some items! */

        /* If requested to not push old items out of cache when memory runs out,
         * we're out of luck at this point... */
        if (!this->engine->config.evict_to_free)
            return NULL;

        /* try to get one off the right LRU
         * don't necessariuly unlink the tail because it may be locked: refcount>0
         * search up from tail an item with refcount==0 and unlink it; give up after search_items
         * tries */
        if (this->tails[id] == NULL)
            return NULL;

        int tries = SEARCH_ITEMS;
        hash_item *search;

        for (search = this->tails[id]; tries > 0 && search; tries--, search = search->prev)
            if (search->refcount == 0) {
                this->do_item_unlink(search);
                break;
            }

        if ((it = (hash_item*) this->engine->slabs->slabs_alloc(ntotal, id)) == NULL) {
            /* Last ditch effort. There is a very rare bug which causes
             * refcount leaks. We've fixed most of them, but it still happens,
             * and it may happen in the future.
             * We can reasonably assume no item can stay locked for more than
             * three hours, so if we find one in the tail which is that old,
             * free it anyway. */
             tries = SEARCH_ITEMS;
             for (search = this->tails[id]; tries > 0 && search; tries--, search = search->prev)
                 if (search->refcount != 0 && search->time + TAIL_REPAIR_TIME < this->engine->get_current_time()) {
                     search->refcount = 0; /****/
                     this->do_item_unlink(search);
                     break;
                 }

             if ((it = (hash_item*) this->engine->slabs->slabs_alloc(ntotal, id)) == NULL)
                return NULL;
        }
    }

    assert(it->slabs_clsid == 0);
    it->slabs_clsid = id;
    assert(it != this->heads[it->slabs_clsid]);

    it->next = it->prev = it->h_next = NULL;
    it->refcount = 1; // the caller will have a reference
    it->iflag = this->engine->config.use_cas ? ITEM_WITH_CAS : 0;
    it->nkey = nkey;
    it->nbytes = nbytes;
    memcpy((void *) items::item_get_key(it), key, nkey);

    return it;
}

hash_item *LRU::do_item_get(const char *key, const size_t nkey) {
    rel_time_t current_time = this->engine->get_current_time(); // ?
    hash_item *it = this->engine->assoc->assoc_find(hash(key, nkey), key, nkey);

    if (it != NULL) {
        if (this->engine->config.oldest_live != 0 &&
            this->engine->config.oldest_live <= current_time &&
            it->time <= this->engine->config.oldest_live) {

            this->do_item_unlink(it);
            it = NULL;
        }
        else {
            it->refcount++;
            this->do_item_update(it);
        }
    }

    return it;
}

int LRU::do_item_link(hash_item *it) {
    assert((it->iflag & (ITEM_LINKED | ITEM_SLABBED)) == 0);
    assert(it->nbytes < (1024 * 1024)); // 1 MB max size
    it->iflag |= ITEM_LINKED;
    it->time = this->engine->get_current_time();
    this->engine->assoc->assoc_insert(hash(items::item_get_key(it), it->nkey), it);

    /* Allocate a new CAS ID on link. */
    items::item_set_cas(it, items::get_cas_id()); // BOH
    this->item_link_q(it);

    return 1;
}

void LRU::do_item_unlink(hash_item *it) {
    if ((it->iflag & ITEM_LINKED) != 0) {
        it->iflag &= ~ITEM_LINKED;
        char *key = items::item_get_key(it);
        this->engine->assoc->assoc_delete(hash(key, it->nkey), key, it->nkey);
        this->item_unlink_q(it);
        if (it->refcount == 0)
            this->item_free(it);
    }
}

void LRU::do_item_release(hash_item *it) {
    if (it->refcount)
        it->refcount--;
    if (it->refcount == 0 && (it->iflag & ITEM_LINKED) == 0)
        this->item_free(it);
}

void LRU::do_item_update(hash_item *it) {
    rel_time_t current_time = this->engine->get_current_time();

    if (it->time < current_time - ITEM_UPDATE_INTERVAL) {
        assert((it->iflag & ITEM_SLABBED) == 0);

        if ((it->iflag & ITEM_LINKED) != 0) {
            this->item_unlink_q(it);
            it->time = current_time;
            this->item_link_q(it);
        }
    }
}

int LRU::do_item_replace(hash_item *it, hash_item *new_it) {
    assert((it->iflag & ITEM_SLABBED) == 0);

    this->do_item_unlink(it);
    return this->do_item_link(new_it);
}

void LRU::item_free(hash_item *it) {
    size_t ntotal = items::ITEM_ntotal(it, engine->config.use_cas);

    assert((it->iflag & ITEM_LINKED) == 0);
    assert(it != this->heads[it->slabs_clsid]);
    assert(it != this->tails[it->slabs_clsid]);
    assert(it->refcount == 0);

    // so slab size changer can tell later if item is already free or not
    unsigned int clsid = it->slabs_clsid;
    it->slabs_clsid = 0;
    it->iflag |= ITEM_SLABBED;
    this->engine->slabs->slabs_free(it, ntotal, clsid);
}

ENGINE_ERROR_CODE LRU::do_store_item(hash_item *it) {
    hash_item *old_it = this->do_item_get(items::item_get_key(it), it->nkey);

    if (old_it != NULL) {
        this->do_item_replace(old_it, it);
        this->do_item_release(old_it);
    }
    else {
        this->do_item_link(it);
    }

    return ENGINE_SUCCESS;
}
