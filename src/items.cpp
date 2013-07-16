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

        hash_item *search; /** riprendi da..... **/
    }

    if (it == NULL && (it = slabs_alloc(engine, ntotal, id)) == NULL) {

/** .....quiiiiiii!!! **/
        for (search = engine->items.tails[id]; tries > 0 && search != NULL; tries--, search=search->prev) {
            if (search->refcount == 0) {
                if (search->exptime == 0 || search->exptime > current_time) {
                    engine->items.itemstats[id].evicted++;
                    engine->items.itemstats[id].evicted_time = current_time - search->time;
                    if (search->exptime != 0) {
                        engine->items.itemstats[id].evicted_nonzero++;
                    }
                    pthread_mutex_lock(&engine->stats.lock);
                    engine->stats.evictions++;
                    pthread_mutex_unlock(&engine->stats.lock);
                    engine->server.stat->evicting(cookie,
                                                  item_get_key(search),
                                                  search->nkey);
                } else {
                    engine->items.itemstats[id].reclaimed++;
                    pthread_mutex_lock(&engine->stats.lock);
                    engine->stats.reclaimed++;
                    pthread_mutex_unlock(&engine->stats.lock);
                }
                do_item_unlink(engine, search);
                break;
            }
        }
        it = slabs_alloc(engine, ntotal, id);
        if (it == 0) {
            engine->items.itemstats[id].outofmemory++;
            /* Last ditch effort. There is a very rare bug which causes
             * refcount leaks. We've fixed most of them, but it still happens,
             * and it may happen in the future.
             * We can reasonably assume no item can stay locked for more than
             * three hours, so if we find one in the tail which is that old,
             * free it anyway.
             */
            tries = search_items;
            for (search = engine->items.tails[id]; tries > 0 && search != NULL; tries--, search=search->prev) {
                if (search->refcount != 0 && search->time + TAIL_REPAIR_TIME < current_time) {
                    engine->items.itemstats[id].tailrepairs++;
                    search->refcount = 0;
                    do_item_unlink(engine, search);
                    break;
                }
            }
            it = slabs_alloc(engine, ntotal, id);
            if (it == 0) {
                return NULL;
            }
        }
    }

    assert(it->slabs_clsid == 0);

    it->slabs_clsid = id;

    assert(it != engine->items.heads[it->slabs_clsid]);

    it->next = it->prev = it->h_next = 0;
    it->refcount = 1;     /* the caller will have a reference */
    DEBUG_REFCNT(it, '*');
    it->iflag = engine->config.use_cas ? ITEM_WITH_CAS : 0;
    it->nkey = nkey;
    it->nbytes = nbytes;
    it->flags = flags;
    memcpy((void*)item_get_key(it), key, nkey);
    it->exptime = exptime;
    return it;
}

hash_item *Items::do_item_get(struct default_engine *engine,
                                const char *key, const size_t nkey) {
}

int Items::do_item_link(struct default_engine *engine, hash_item *it) {
}

void Items::do_item_unlink(struct default_engine *engine, hash_item *it) {
}

void Items::do_item_release(struct default_engine *engine, hash_item *it) {
}

void Items::do_item_update(struct default_engine *engine, hash_item *it) {
}

int Items::do_item_replace(struct default_engine *engine,
                            hash_item *it, hash_item *new_it) {
}

void Items::item_free(struct default_engine *engine, hash_item *it) {
}


