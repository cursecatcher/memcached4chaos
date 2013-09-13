#include "assoc.hpp"

void* fun_assoc_maintenance_thread(void* arg);


AssociativeArray::AssociativeArray(LRU_Lists *lru, unsigned int hashpower) {
    this->lru = lru;
    this->hashpower = hashpower;
    this->primary_hashtable = (hash_item**) calloc(hashsize(this->hashpower), sizeof(void *));

    if (!this->primary_hashtable) {
        std::cerr << "Init hashtable failed: cannot allocate memory." << std::endl;
        abort();
    }
}

hash_item *AssociativeArray::assoc_find(const uint32_t hash, const char *key, const size_t nkey) {
    hash_item *it;
    unsigned int bucket;

    it = this->which_hashtable(hash, bucket) ?
         this->old_hashtable[bucket] :
         this->primary_hashtable[this->get_bucket(hash)];

    for (; it; it = it->h_next) // ricerca nell'hash chain
//        if ((nkey == it->nkey) && memcmp(key, this->lru->item_get_key(it), nkey) == 0)
        if ((hash == it->hv) && memcmp(key, this->lru->item_get_key(it), nkey) == 0)
            break;

    return it;
}

int AssociativeArray::assoc_insert(hash_item *it) {
    unsigned int bucket;

    // shouldn't have duplicately named things defined
    assert(assoc_find(it->hv, this->lru->item_get_key(it), it->nkey) == NULL);

    if (this->which_hashtable(it->hv, bucket)) {
        it->h_next = this->old_hashtable[bucket];
        this->old_hashtable[bucket] = it;
    }
    else {
        bucket = this->get_bucket(it->hv);
        it->h_next = this->primary_hashtable[bucket];
        this->primary_hashtable[bucket] = it;
    }

    this->hash_items++;
    if (!this->expanding && this->hash_items > (hashsize(this->hashpower) * 3) / 2)
        this->assoc_expand();

    return 1;
}

void AssociativeArray::assoc_delete(const uint32_t hash, const char *key, const size_t nkey) {
    hash_item **before = this->hashitem_before(hash, key, nkey);

    if (*before) {
        hash_item *next;
        this->hash_items--;

        next = (*before)->h_next;
        (*before)->h_next = NULL; // probably pointless, but whatever
        *before = next;
        return;
    }

    /* Note: we never actually get here.
     * The callers don't delete things they can't find */
    assert(*before != NULL);
}

void AssociativeArray::assoc_maintenance_thread() {
    bool done = false;

    do {
        this->lru->lock_cache();

        if (this->expanding) {
            hash_item *it, *next;
            int bucket;

            for (it = this->old_hashtable[this->expand_bucket]; it; it = next) {
                next = it->h_next;

//                bucket = this->get_bucket(hash(this->lru->item_get_key(it), it->nkey));
                bucket = this->get_bucket(it->hv);
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
    ((AssociativeArray*) arg)->assoc_maintenance_thread();
    return NULL;
}


void AssociativeArray::assoc_expand() {
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

hash_item** AssociativeArray::hashitem_before(const uint32_t hash, const char *key, const size_t nkey) {
    hash_item **pos;
    unsigned int bucket;

    pos = this-> which_hashtable(hash, bucket) ?
          &this->old_hashtable[bucket] :
          &this->primary_hashtable[this->get_bucket(hash)];

    while //(*pos && ((nkey != (*pos)->nkey) ||
            (*pos && ((hash != (*pos)->hv) ||
            memcmp(key, this->lru->item_get_key(*pos), nkey)))
        pos = &(*pos)->h_next;

    return pos;
}
