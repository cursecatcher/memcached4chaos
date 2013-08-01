#include "assoc.hpp"

#define hashsize(n) ((uint32_t)1<<(n))
#define hashmask(n) (hashsize(n)-1)
#define DEFAULT_HASH_BULK_MOVE 1

void* fun_assoc_maintenance_thread(void* arg);


Assoc::Assoc(LRU *lru, unsigned int hashpower) {
    this->lru = lru;
    this->hashpower = hashpower;
    this->primary_hashtable = (hash_item**) calloc(hashsize(this->hashpower), sizeof(void *));

    if (!this->primary_hashtable)
        throw "SMERDO!";
}

hash_item *Assoc::assoc_find(uint32_t hash, const char *key, const size_t nkey) {
    hash_item *it;
    unsigned int bucket;

    if (this->expanding && (bucket = hash & hashmask(this->hashpower-1)) >= this->expand_bucket)
        it = this->old_hashtable[bucket];
    else
        it = this->primary_hashtable[hash & hashmask(this->hashpower)];

    for (; it; it = it->h_next) // gestione delle collisioni
        if ((nkey == it->nkey) && memcmp(key, this->lru->item_get_key(it), nkey) == 0)
            break;

    return it;
}

int Assoc::assoc_insert(uint32_t hash, hash_item *it) {
    unsigned int bucket;

    // shouldn't have duplicately named things defined
    assert(assoc_find(hash, this->lru->item_get_key(it), it->nkey) == NULL);

    if (this->expanding && (bucket = hash & hashmask(this->hashpower-1)) >= this->expand_bucket) {
        it->h_next = this->old_hashtable[bucket];
        this->old_hashtable[bucket] = it;
    }
    else {
        it->h_next = this->primary_hashtable[bucket = hash & hashmask(this->hashpower)];
        this->primary_hashtable[bucket] = it;
    }

    this->hash_items++;
    if (!this->expanding && this->hash_items > (hashsize(this->hashpower) * 3) / 2)
        this->assoc_expand();

    return 1;

}

void Assoc::assoc_delete(uint32_t hash, const char *key, const size_t nkey) {
    hash_item **before = this->hashitem_before(hash, key, nkey);

    if (*before) {
        hash_item *next;
        this->hash_items--;

        next = (*before)->h_next;
        (*before)->h_next = NULL; // probably pointless, but whatever
        *before = next;
        return;
    }

    /* Note: we never actually get here. The callers don't delete things
     * they can't find */
    assert(*before != NULL);
}

void Assoc::assoc_maintenance_thread() {
    bool done = false;

    do {
        this->lru->lock_cache();

        for (int ii = 0; ii < DEFAULT_HASH_BULK_MOVE && this->expanding; ii++) {
            hash_item *it, *next;
            int bucket;

            for (it = this->old_hashtable[this->expand_bucket]; it; it = next) {
                next = it->h_next;

                bucket = hash(this->lru->item_get_key(it), it->nkey) & hashmask(this->hashpower);
                it->h_next = this->primary_hashtable[bucket];
                this->primary_hashtable[bucket] = it;
            }

            this->old_hashtable[this->expand_bucket++] = NULL;

            if (this->expand_bucket == hashsize(this->hashpower-1)) {
                this->expanding = false;
                free(this->old_hashtable);
                //hash table expansion done!
            }
        }
        if (!this->expanding)
            done = true;

        this->lru->unlock_cache();
    } while (!done);
}


void* fun_assoc_maintenance_thread(void* arg) {
    ((Assoc*) arg)->assoc_maintenance_thread();
    return NULL;
}


void Assoc::assoc_expand() {
    this->old_hashtable = this->primary_hashtable;
    this->primary_hashtable = (hash_item**) calloc(hashsize(this->hashpower+1), sizeof(void *));

    if (this->primary_hashtable) {
        this->hashpower++;
        this->expanding = true;
        this->expand_bucket = 0;

        // start a thread to do the expansion
        int ret = 0;
        pthread_t tid;
        pthread_attr_t attr;

        if (pthread_attr_init(&attr) != 0 ||
            pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED) != 0 ||
            (ret = pthread_create(&tid, &attr, fun_assoc_maintenance_thread, this) != 0)) {
            //can't create thread -> expansion failed
            this->hashpower--;
            this->expanding = false;
            free(this->primary_hashtable);
            this->primary_hashtable = this->old_hashtable;
        }
    }
    else {
        // bad news, but we can keep running
        this->primary_hashtable = this->old_hashtable;
    }
}

hash_item** Assoc::hashitem_before(uint32_t hash, const char *key, const size_t nkey) {
    hash_item **pos;
    unsigned int bucket;

    if (this->expanding && (bucket = hash & hashmask(this->hashpower-1)) >= this->expand_bucket)
        pos = &this->old_hashtable[bucket];
    else
        pos = &this->primary_hashtable[hash & hashmask(this->hashpower)];

    while (*pos && ((nkey != (*pos)->nkey) || memcmp(key, this->lru->item_get_key(*pos), nkey)))
        pos = &(*pos)->h_next;

    return pos;
}
