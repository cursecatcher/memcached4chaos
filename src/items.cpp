#include "items.hpp"


Items::Items() {
}

hash_item *Items::item_alloc(struct default_engine *engine,
                               const void *key, size_t nkey,
                               int flags,/*rel_time_t exptime, */
                               int nbytes, const void *cookie) {
    hash_item *it;

    pthread_mutex_lock(&engine->cache_lock);
    it = do_item_alloc(engine, key, nkey, flags, /*exptime,*/ nbytes, cookie);
    pthread_mutex_unlock(&engine->cache_lock);
    return it;
}

hash_item *Items::item_get(struct default_engine *engine,
                             const void *key, const size_t nkey) {
    hash_item *it;

    pthread_mutex_lock(&engine->cache_lock);
    it = do_item_get(engine, key, nkey);
    pthread_mutex_unlock(&enigne->cache_lock);
    return it;
}

void Items::item_release(struct default_engine *engine, hash_item *it) {
    pthread_mutex_lock(&engine->cache_lock);
    do_item_release(engine, item);
    pthread_mutex_unlock(&engine->cache_lock);
}

void Items::item_unlink(struct default_engine *engine, hash_item *it) {
    pthread_mutex_lock(&engine->cache_lock);
    do_item_unlink(engine, item);
    pthread_mutex_unlock(&engine->cache_lock);
}

ENGINE_ERROR_CODE Items::store_item(struct default_engine *engine,
                                     hash_item *it,
                                     uint64_t cas,
                                     ENGINE_STORE_OPERATION operation,
                                     const void *cookie) {
    ENGINE_ERROR_CODE ret;

    pthread_mutex_lock(&engine->cache_lock);
    ret = do_store_item(engine, item, cas, operation, cookie);
    pthread_mutex_unlock(&engine->cache_lock);
    return ret;
}



void Items::item_link_q(struct default_engine *engine, hash_item *it) {
    hash_item **head, **tail;

    assert(it->slabs_clsid < POWER_LARGEST);
    assert((it->iflag & ITEM_SLABBED) == 0);

    head = &this->heads[it->slabs_clsid];
    tail = &this->tails[it->slabs_clsid];

    assert(it != *head);
    assert((*head && *tail) || (*head == NULL && *tail == NULL);

    it->prev = NULL;
    it->next = *head;
    if (it->next)
        it->next->prev = it;
    *head = it;
    if (*tail == NULL)
        *tail = it;
    this->sizes[it->slabs_clsid]++;
}

void Items::item_unlink_q(struct default_engine *engine, hash_item *it) {
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

hash_item *Items::do_item_alloc(struct default_engine *engine,
                                  const void *key, const size_t nkey,
                                  const int flags, const rel_time_t exptime,
                                  const int nbytes,
                                  const void *cookie) {
    hash_item *it = NULL;
    size_t ntotal = sizeof(hash_item) + nkey + nbytes;
    unsigned int id;

    if (engine->config.use_cas)
        ntotal += sizeof(uint64_t);

    if ((id = slabs_clsid(engine, ntotal)) == 0)
        return 0;

    if ((it = engine->slabs->slabs_alloc(engine, ntotal, id)) == NULL) {
        /* Could not find an expired item at the tail, and memory allocation
         * failed. Try to evict some items! */
        tries = search_items; // da definire magari

        /* If requested to not push old items out of cache when memory runs out,
         * we're out of luck at this point... */
        if (engine->config.evict_to_free == 0)
            return NULL;

        /* try to get one off the right LRU
         * don't necessariuly unlink the tail because it may be locked: refcount>0
         * search up from tail an item with refcount==0 and unlink it; give up after search_items
         * tries */
        if (this->tails[id] == NULL)
            return NULL;

        hash_item *search;

        for (search = this->tails[id]; tries > 0 && search; tries--, search = search->prev)
            if (search->refcount == 0) {
                this->do_item_unlink(engine, search);
                break;
            }

        if ((it = engine->slabs->slabs_alloc(engine, ntotal, id)) == NULL) {
            /* Last ditch effort. There is a very rare bug which causes
             * refcount leaks. We've fixed most of them, but it still happens,
             * and it may happen in the future.
             * We can reasonably assume no item can stay locked for more than
             * three hours, so if we find one in the tail which is that old,
             * free it anyway. */
             tries = search_items;
             for (search = this->tails[id]; tries > 0 && search; tries--, search = search->prev) {
                 search->refcount = 0; /****/
                 this->do_item_unlink(engine, search);
                 break;
             }

             if ((it = engine->slabs->slabs_alloc(engine, ntotal, id)) == NULL)
                return NULL;
        }
    }

    assert(it->slabs_clsid == 0);
    it->slabs_clsid = id;

    assert(it != this->heads[it->slabs_clsid]);
    it->next = it->prev = it->h_next = NULL;
    it->refcount = 1; // the caller will have a reference
    it->iflag = engine->config.use_cas ? ITEM_WITH_CAS : 0;
    it->nkey = nkey;
    it->nbytes = nbytes;
    it->flags = flags;
    memcpy((void *) item_get_key(it), key, nkey);

    return it;
}

hash_item *Items::do_item_get(struct default_engine *engine,
                                const char *key, const size_t nkey) {

    rel_time_t current_time = engine-> server.core->get_current_time(); // ?
    hash_item *it = assoc_find(engine, hash(key, nkey), key, nkey);

    if (it) {
        if (engine->config.oldest_live != 0 &&
            engine->config.oldest_live <= current_time &&
            it->time <= engine->config.oldest_live) {

            this->do_item_unlink(engine, it);
            it = NULL;
        }
        else {
            it->refcount++;
            this->do_item_update(engine, it);
        }
    }

    return it;
}

int Items::do_item_link(struct default_engine *engine, hash_item *it) {
    assert((it->iflag & (ITEM_LINKED | ITEM_SLABBED)) == 0);
    assert(it->nbytes < (1024 * 1024)); // 1 MB max size
    it->iflag |= ITEM_LINKED;
    it->time = engine->server.core->get_current_time(); //globals?
    engine->assoc->assoc_insert(engine, hash(item_get_key(it), it->nkey), it);

    /* Allocate a new CAS ID on link. */
    item_set_cas(NULL, NULL, it, get_cas_id()); // BOH
    this->item_link_q(engine, it);

    return 1;
}

void Items::do_item_unlink(struct default_engine *engine, hash_item *it) {
    if ((it->iflag & ITEM_LINKED) != 0) {
        it->iflag &= ~ITEM_LINKED;
        engine->assoc->assoc_delete(engine, hash(item_get_key(it), it->nkey),
                                    item_get_key(it), it->nkey);
        this->item_unlink_q(engine, it);
        if (it->refcount == 0)
            this->item_free(engine, it);
    }
}

void Items::do_item_release(struct default_engine *engine, hash_item *it) {
    if (it->refcount)
        it->refcount--;
    if (it->refcount == 0 && (it->iflag & ITEM_LINKED) == 0)
        this->item_free(engine, it);
}

void Items::do_item_update(struct default_engine *engine, hash_item *it) {
    rel_time_t current_time = engine->server.core->get_current_time();

    if (it->time < current_time - ITEM_UPDATE_INTERVAL) {
        assert((it->iflag & ITEM_SLABBED) == 0);

        if ((it->iflag & ITEM_LINKED) != 0) {
            this->item_unlink_q(engine, it);
            it->time = current_time;
            this->item_link_q(engine, it);
        }
    }
}

int Items::do_item_replace(struct default_engine *engine,
                            hash_item *it, hash_item *new_it) {

    assert((it->iflag & ITEM_SLABBED) == 0);

    this->do_item_unlink(engine, it);
    return do_item_link(engine, new_it);
}

void Items::item_free(struct default_engine *engine, hash_item *it) {
    size_t ntotal = ITEM_ntotal(engine, it);

    assert((it->iflag & ITEM_LINKED) == 0);
    assert(it != this->heads[it->slabs_clsid]);
    assert(it != this->tails[it->slabs_clsid]);
    assert(it->refcount == 0);

    // so slab size changer can tell later if item is already free or not
    unsigned int clsid = it->slabs_clsid;
    it->slabs_clsid = 0;
    it->iflag |= ITEM_SLABBED;
    engine->slabs->slabs_free(engine, it, ntotal, clsid);
}


