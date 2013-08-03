#include "lru.hpp"


LRU_Queues::LRU_Queues(const struct config settings) {
    this->assoc = new Assoc(this, settings.hashpower);
    this->slabs = new Slabs(settings);
    this->settings = settings;

    pthread_mutex_init(&this->cache_lock, NULL);

    for (int i = 0; i < POWER_LARGEST; i++) {
        this->heads[i] = this->tails[i] = NULL;
        this->sizes[i] = 0;
    }
}

void LRU_Queues::item_link_q(hash_item *it) {
    hash_item **head, **tail;

    assert(it->slabs_clsid < POWER_LARGEST);
    assert((it->iflag & ITEM_SLABBED) == NO_FLAGS);

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

void LRU_Queues::item_unlink_q(hash_item *it) {
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

hash_item *LRU_Queues::do_item_alloc(const char *key, const size_t nkey, const int nbytes) {
    hash_item *it;
    size_t ntotal = sizeof(hash_item) + nkey + nbytes;
    unsigned int id;

    if ((id = this->slabs->slabs_clsid(ntotal)) == 0)
        return NULL;

    if ((it = (hash_item*) this->slabs->slabs_alloc(ntotal, id)) == NULL) {
        /* try to get one off the right LRU
         * don't necessariuly unlink the tail because it may be locked: refcount>0
         * search up from tail an item with refcount==0 and unlink it;
         * give up after search_items tries */
        if (this->tails[id] == NULL)
            return NULL;

        int tries = SEARCH_ITEMS;
        hash_item *search;

        for (search = this->tails[id]; tries > 0 && search; tries--, search = search->prev)
            if (search->refcount == 0) {
                this->do_item_unlink(search);
                break;
            }

        if ((it = (hash_item*) this->slabs->slabs_alloc(ntotal, id)) == NULL) {
            /* Last ditch effort. There is a very rare bug which causes
             * refcount leaks. We've fixed most of them, but it still happens,
             * and it may happen in the future.
             * We can reasonably assume no item can stay locked for more than
             * three hours, so if we find one in the tail which is that old,
             * free it anyway. */
             rel_time_t current_time = this->get_current_time();
             for (search = this->tails[id], tries = SEARCH_ITEMS; tries > 0 && search; tries--, search = search->prev)
                 if (search->refcount != 0 && search->time + TAIL_REPAIR_TIME < current_time) {
                     search->refcount = 0;
                     this->do_item_unlink(search);
                     break;
                 }

            if ((it = (hash_item*) this->slabs->slabs_alloc(ntotal, id)) == NULL) {
                /* blocco aggiunto:
                 * se tutti i metodi 'gentili' hanno fallito, riciclo la
                 * memoria occupata dall'elemento LRU (least recently used);
                 * in questo modo abbiamo sempre un po' di memoria disponibile,
                 * e l'allocazione riesce sempre (credo!) */
                search = tails[id];
                tails[id] = tails[id]->prev; // nuovo LRU element
                tails[id]->prev->next = NULL;
                search->refcount = 0;
                this->do_item_unlink(search);

                if ((it = (hash_item*) this->slabs->slabs_alloc(ntotal, id)) == NULL)
                    return NULL;
            }
        }
    }

    assert(it->slabs_clsid == 0);
    it->slabs_clsid = id;
    assert(it != this->heads[it->slabs_clsid]);

    it->next = it->prev = it->h_next = NULL;
    it->refcount = 1; // the caller will have a reference
    it->iflag = NO_FLAGS;
    it->nkey = nkey;
    it->nbytes = nbytes;
    memcpy((void *) this->item_get_key(it), key, nkey);

    return it;
}

hash_item *LRU_Queues::do_item_get(const char *key, const size_t nkey) {
    hash_item *it = this->assoc->assoc_find(hash(key, nkey), key, nkey);

    if (it) {
        it->refcount++;
        this->do_item_update(it);
    }

    return it;
}

int LRU_Queues::do_item_link(hash_item *it) {
    assert((it->iflag & (ITEM_LINKED | ITEM_SLABBED)) == NO_FLAGS);
    assert(it->nbytes < this->settings.item_size_max);

    it->iflag |= ITEM_LINKED;
    it->time = this->get_current_time();

    this->assoc->assoc_insert(hash(this->item_get_key(it), it->nkey), it);
    this->item_link_q(it);

    return 1;
}

void LRU_Queues::do_item_unlink(hash_item *it) {
    if ((it->iflag & ITEM_LINKED) != NO_FLAGS) {
        char *key = this->item_get_key(it);

        it->iflag &= ~ITEM_LINKED;
        this->assoc->assoc_delete(hash(key, it->nkey), key, it->nkey);
        this->item_unlink_q(it);

        if (it->refcount == 0)
            this->item_free(it);
    }
}

void LRU_Queues::do_item_release(hash_item *it) {
    if (it->refcount)
        it->refcount--;
    if (it->refcount == 0 && (it->iflag & ITEM_LINKED) == NO_FLAGS)
        this->item_free(it);
}

void LRU_Queues::do_item_update(hash_item *it) {
    rel_time_t current_time = this->get_current_time();

    if (it->time < current_time - ITEM_UPDATE_INTERVAL) {
        assert((it->iflag & ITEM_SLABBED) == NO_FLAGS);

        if ((it->iflag & ITEM_LINKED) != NO_FLAGS) {
            this->item_unlink_q(it);
            it->time = current_time;
            this->item_link_q(it);
        }
    }
}

int LRU_Queues::do_item_replace(hash_item *it, hash_item *new_it) {
    assert((it->iflag & ITEM_SLABBED) == NO_FLAGS);

    this->do_item_unlink(it);
    return this->do_item_link(new_it);
}

void LRU_Queues::item_free(hash_item *it) {
    size_t ntotal = this->ITEM_ntotal(it);

    assert((it->iflag & ITEM_LINKED) == NO_FLAGS);
    assert(it != this->heads[it->slabs_clsid]);
    assert(it != this->tails[it->slabs_clsid]);
    assert(it->refcount == 0);

    // so slab size changer can tell later if item is already free or not
    unsigned int clsid = it->slabs_clsid;
    it->slabs_clsid = 0;
    it->iflag |= ITEM_SLABBED;
    this->slabs->slabs_free(it, ntotal, clsid);
}

void LRU_Queues::do_store_item(hash_item *it) {
    hash_item *old_it = this->do_item_get(this->item_get_key(it), it->nkey);

    if (old_it != NULL) {
        this->do_item_replace(old_it, it);
        this->do_item_release(old_it);
    }
    else {
        this->do_item_link(it);
    }
}
