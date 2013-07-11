#include "memslab.hpp"

struct settings settings;
volatile rel_time_t current_time;


memslab::memslab(
    const int hashpower_init,
    int nthreads,
    const size_t limit, const double factor, const bool prealloc) {

    /** globals */
    this->cache_lock = new mutex();

    /** associative array */
    pthread_cond_init(&this->assoc_maintenance_cond, NULL);
    this->hash_bulk_move = DEFAULT_HASH_BULK_MOVE;
    this->do_run_maintenance_thread = 1;

    this->old_hashtable = NULL;
    this->hash_items = 0;
    this->expanding = this->started_expanding = false;
    this->expand_bucket = 0;
    this->hashpower =
        hashpower_init > 0 ? hashpower_init : HASHPOWER_DEFAULT;
    this->primary_hashtable =
        (item **) calloc(hashsize(this->hashpower), sizeof(void *));

    if (!this->primary_hashtable) {
        std::cerr << "failed to init hashtable" << std::endl;
        exit(EXIT_FAILURE);
    }

    /** slabbing */
    this->slabs_lock = new mutex();
    this->slabs_rebalance_lock = new mutex();

    this->slab_bulk_check = DEFAULT_SLAB_BULK_CHECK;
    this->mem_base = this->mem_current = NULL;
    this->mem_limit = limit;
    this->mem_malloced = 0;
    this->mem_avail = 0;

    if (prealloc) {
        /* Allocate everything in a big chunk with malloc */
        this->mem_base = malloc(this->mem_limit);
        if (this->mem_base != NULL) {
            this->mem_current = this->mem_base;
            this->mem_avail = this->mem_limit;
        } else { /// VERBOSE
            std::cerr << "Warning: Failed to allocate requested memory in one large chunk." << std::endl;
            std::cerr << "Will allocate in smaller chunks." << std::endl;
        }
    }

    int i = POWER_SMALLEST - 1;
    unsigned int size = sizeof(item) + settings.chunk_size; /// SETTINGS

    memset(this->slabclass, 0, sizeof(this->slabclass));

    while (++i < POWER_LARGEST && size <= settings.item_size_max / factor) { /// SETTINGS
        /* Make sure items are always n-byte aligned */
        if (size % CHUNK_ALIGN_BYTES)
            size += CHUNK_ALIGN_BYTES - (size % CHUNK_ALIGN_BYTES);

        this->slabclass[i].size = size;
        this->slabclass[i].perslab = settings.item_size_max / this->slabclass[i].size; /// SETTINGS
        size *= factor;
        /// SETTINGS
        if (settings.verbose > 1) { /// VERBOSE
            fprintf(stderr, "slab class %3d: chunk size %9u perslab %7u\n",
                    i, slabclass[i].size, slabclass[i].perslab);
        }
    }

    this->power_largest = i;
    this->slabclass[this->power_largest].size = settings.item_size_max; /// SETTINGS
    this->slabclass[this->power_largest].perslab = 1;
    /// SETTINGS
    if (settings.verbose > 1) { /// VERBOSE
        fprintf(stderr, "slab class %3d: chunk size %9u perslab %7u\n",
                i, slabclass[i].size, slabclass[i].perslab);
    }

    /* for the test suite:  faking of how much we've already malloc'd */
    {
        char *t_initial_malloc = getenv("T_MEMD_INITIAL_MALLOC");
        if (t_initial_malloc)
            this->mem_malloced = (size_t)atol(t_initial_malloc);
    }

    if (prealloc)
        slabs_preallocate(this->power_largest);

    /** items */
    #if !defined(HAVE_GCC_ATOMICS) && !defined(__sun)
    this->atomics_mutex = new mutex();
    #endif
    this->init_lock = new mutex();
    pthread_cond_init(&this->init_cond, NULL);

    this->item_global_lock = new mutex();
    pthread_key_create(&this->item_lock_type_key, NULL);

    int power;
    switch (nthreads) {
        case 1:
        case 2:     power = 10; break;
        case 3:     power = 11; break;
        case 4:     power = 12; break;
        default:    power = 13; break;
    }

    this->item_lock_count = hashsize(power);
    this->item_locks =
        (mutex **) calloc(item_lock_count, sizeof(mutex*));

    if (!this->item_locks) {
        std::cerr << "Can't allocate item locks" << std::endl;
        exit(0); /// MEMORY (EPIC) FAIL
    }

    for (int i = 0; i < (int) this->item_lock_count; i++)
        this->item_locks[i] = new mutex();

    // partici i threadsss
}

void memslab::switch_item_lock_type(enum item_lock_types type) {
    std::cerr << "Mi hanno detto che funziona" << std::endl;
}

/*** array associativo ***/
item* memslab::assoc_find(const char *key, const size_t nkey, const uint32_t hv) {
    item *it, *ret = NULL;
    unsigned int oldbucket;

    it = this->expanding &&
         (oldbucket = (hv & hashmask(this->hashpower - 1))) >= this->expand_bucket ?
            this->old_hashtable[oldbucket] :
            this->primary_hashtable[hv & hashmask(this->hashpower)];
/*
    if (this->expanding && (oldbucket = (hv & hashmask(this->hashpower - 1))) >= this->expand_bucket)
        it = this->old_hashtable[oldbucket];
    else
        it = this->primary_hashtable[hv & hashmask(this->hashpower)]; */

    while (it) {
        if ((nkey == it->nkey) && (memcmp(key, ITEM_key(it), nkey) == 0)) {
            ret = it;
            break;
        }
        it = it->h_next;
    }

    return ret;
}

/* Note: this isn't an assoc_update.  The key must not already exist to call this */
int memslab::assoc_insert(item *it, const uint32_t hv) {
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

    return 1;
}

void memslab::assoc_delete(const char *key, const size_t nkey, const uint32_t hv) {
    item **before = this->_hashitem_before(key, nkey, hv);

    if (*before) {
        item *nxt;
        hash_items--;
/* The DTrace probe cannot be triggered as the last instruction
 * due to possible tail-optimization by the compiler */
        nxt = (*before)->h_next;
        (*before)->h_next = NULL;   /* probably pointless, but whatever. */
        *before = nxt;
        return;
    }
/* Note:  we never actually get here.  the callers don't delete things
 *  they can't find. */
    assert(*before != 0);
}

item** memslab::_hashitem_before (const char *key, const size_t nkey, const uint32_t hv) {
    unsigned int oldbucket;
    item **pos =
        this->expanding &&
        (oldbucket = (hv & hashmask(this->hashpower - 1))) >= this->expand_bucket ?
            &this->old_hashtable[oldbucket] :
            &this->primary_hashtable[hv & hashmask(this->hashpower)];
/*  if (this->expanding && (oldbucket = (hv & hashmask(this->hashpower - 1))) >= this->expand_bucket)
        pos = &this->old_hashtable[oldbucket];
    else
        pos = &this->primary_hashtable[hv & hashmask(this->hashpower)]; */

    while (*pos && ((nkey != (*pos)->nkey) || memcmp(key, ITEM_key(*pos), nkey)))
        pos = &(*pos)->h_next;

    return pos;
}

void memslab::assoc_expand(void) {
    this->old_hashtable = this->primary_hashtable;

    this->primary_hashtable = (item **) calloc(hashsize(this->hashpower + 1), sizeof(void *));
    if (this->primary_hashtable) {
      if (settings.verbose > 1) /// VERBOSE
            std::cerr << "Hash table expansion starting." << std::endl;
        this->hashpower++;
        this->expanding = true;
        this->expand_bucket = 0;
    }
    else {
        /* Bad news, but we can keep running. */
        this->primary_hashtable = this->old_hashtable;
    }
}

void memslab::assoc_start_expand(void) {
    if (this->started_expanding)
        return;
    this->started_expanding = true;
    pthread_cond_signal(&this->assoc_maintenance_cond);
}


//      void do_assoc_move_next_bucket(void);
int memslab::start_assoc_maintenance_thread(void) {
    int ret;
    char *env = getenv("MEMCACHED_HASH_BULK_MOVE");

    if (env != NULL) {
        this->hash_bulk_move = atoi(env);
        if (this->hash_bulk_move == 0) {
            this->hash_bulk_move = DEFAULT_HASH_BULK_MOVE;
        }
    }
    /// VERBOSE
    if ((ret = pthread_create(&this->assoc_maintenance_tid, NULL, assoc_maintenance_thread, (void*) this)) != 0) {
        std::cerr << "Can't create thread: " << strerror(ret) << std::endl;;
        return -1;
    }
    return 0;
}


void memslab::stop_assoc_maintenance_thread(void) {
/// FIX
//    mutex_lock(&this->cache_lock);
    this->cache_lock->lock_trylock();

    this->do_run_maintenance_thread = 0;
    pthread_cond_signal(&this->assoc_maintenance_cond);
/// FIX
    this->cache_lock->unlock();
//    pthread_mutex_unlock(&this->cache_lock);

    /* Wait for the maintenance thread to stop */
    pthread_join(this->assoc_maintenance_tid, NULL);
}

void* memslab::_assoc_maintenance_thread(void) {
    while (this->do_run_maintenance_thread) {
        int ii;

        /* Lock the cache, and bulk move multiple buckets to the new
         * hash table. */
//        item_lock_global();
//        mutex_lock(&this->cache_lock);
        this->item_global_lock->lock();
        this->cache_lock->lock();

        for (ii = 0; ii < this->hash_bulk_move && this->expanding; ++ii) {
            item *it, *next;
            int bucket;

            for (it = this->old_hashtable[this->expand_bucket]; it != NULL; it = next) {
                next = it->h_next;

                bucket = hash(ITEM_key(it), it->nkey) & hashmask(this->hashpower);
                it->h_next = this->primary_hashtable[bucket];
                this->primary_hashtable[bucket] = it;
            }

            this->old_hashtable[this->expand_bucket] = NULL;
            this->expand_bucket++;

            if (this->expand_bucket == hashsize(this->hashpower - 1)) {
                this->expanding = false;
                free(this->old_hashtable);

                if (settings.verbose > 1) /// VERBOSE
                    std::cerr << "Hash table expansion done" << std::endl;
            }
        }

        this->cache_lock->unlock();
        this->item_global_lock->unlock();

        if (!this->expanding) {
            /* finished expanding. tell all threads to use fine-grained locks */
            this->switch_item_lock_type(ITEM_LOCK_GRANULAR);
            std::cout << "slabs_rebalancer_resume()" << std::endl;
            this->slabs_rebalancer_resume();
            /* We are done expanding.. just wait for next invocation */

            std::cout << "cache_lock->lock()" << std::endl;
            this->cache_lock->lock();

            this->started_expanding = false;
            std::cout << "cache_lock->cond_wait()" << std::endl;
            this->cache_lock->cond_wait(&this->assoc_maintenance_cond);
            /* Before doing anything, tell threads to use a global lock */
            std::cout << "cache_lock->unlock()" << std::endl;
            this->cache_lock->unlock();

            std::cout << "slabs_rebalancer_pause()" << std::endl;
            this->slabs_rebalancer_pause();
            this->switch_item_lock_type(ITEM_LOCK_GLOBAL);

            this->cache_lock->lock();
            this->assoc_expand();
            this->cache_lock->unlock();
        }
    }

    return NULL;
}


/*** slabbing ***/

/**** Metodi privati ****/
int memslab::grow_slab_list(const unsigned int id) {
    slabclass_t *p = &this->slabclass[id];

    if (p->slabs == p->list_size) {
        size_t new_size =  (p->list_size != 0) ? p->list_size * 2 : 16;
        void *new_list = realloc(p->slab_list, new_size * sizeof(void *));

        if (new_list == 0)
            return 0;

        p->list_size = new_size;
        p->slab_list = (void **) new_list;
    }

    return 1;
}

void memslab::split_slab_page_into_freelist(char *ptr, const unsigned int id) {
    slabclass_t *p = &this->slabclass[id];

    for (int x = 0; x < (int) p->perslab; x++) {
        this->do_slabs_free(ptr, 0, id);
        ptr += p->size;
    }
}

int memslab::do_slabs_newslab(const unsigned int id) {
    slabclass_t *p = &this->slabclass[id];
    char *ptr;
    /// SETTINGS
    int len = settings.slab_reassign ? settings.item_size_max
        : p->size * p->perslab;

    if ((this->mem_limit && this->mem_malloced + len > this->mem_limit && p->slabs > 0) ||
        (this->grow_slab_list(id) == 0) ||
        ((ptr = (char*) this->memory_allocate((size_t) len)) == 0)) {

        return 0;
    }

    memset(ptr, 0, (size_t)len);
    this->split_slab_page_into_freelist(ptr, id);

    p->slab_list[p->slabs++] = ptr;
    this->mem_malloced += len;

    return 1;
}

void *memslab::do_slabs_alloc(const size_t size, unsigned int id) {
    slabclass_t *p;
    void *ret = NULL;
    item *it = NULL;

    if (id < POWER_SMALLEST || id > (unsigned) this->power_largest) {
        return NULL;
    }

    p = &this->slabclass[id];
    assert(p->sl_curr == 0 || ((item *)p->slots)->slabs_clsid == 0);

    /* fail unless we have space at the end of a recently allocated page,
       we have something on our freelist, or we could allocate a new page */
    if (! (p->sl_curr != 0 || this->do_slabs_newslab(id) != 0)) {
        /* We don't have more memory available */
        ret = NULL;
    } else if (p->sl_curr != 0) {
        /* return off our freelist */
        it = (item *)p->slots;
        p->slots = it->next;
        if (it->next)
            it->next->prev = 0;
        p->sl_curr--;
        ret = (void *)it;
    }

    if (ret)
        p->requested += size;

    return ret;
}

void memslab::do_slabs_free(void *ptr, const size_t size, unsigned int id) {
    slabclass_t *p;
    item *it;

    assert(((item *)ptr)->slabs_clsid == 0);
    assert(id >= POWER_SMALLEST && id <= (unsigned) this->power_largest);

    if (id < POWER_SMALLEST || id > (unsigned) this->power_largest)
        return;

    p = &this->slabclass[id];

    it = (item *)ptr;
    it->it_flags |= ITEM_SLABBED;
    it->prev = 0;
    it->next = (item*) p->slots;

    if (it->next)
        it->next->prev = it;

    p->slots = it;
    p->sl_curr++;
    p->requested -= size;

    return;
}

enum reassign_result_type memslab::do_slabs_reassign(int src, int dst) {
    if (this->slab_rebalance_signal != 0)
        return REASSIGN_RUNNING;

    if (src == dst)
        return REASSIGN_SRC_DST_SAME;

    /* Special indicator to choose ourselves. */
    if (src == -1) {
        src = this->slabs_reassign_pick_any(dst);
        /* TODO: If we end up back at -1, return a new error type */
    }

    if (src < POWER_SMALLEST || src > this->power_largest ||
        dst < POWER_SMALLEST || dst > this->power_largest)
        return REASSIGN_BADCLASS;

    if (this->slabclass[src].slabs < 2)
        return REASSIGN_NOSPARE;

    this->slab_rebal.s_clsid = src;
    this->slab_rebal.d_clsid = dst;

    this->slab_rebalance_signal = 1;
    pthread_cond_signal(&this->slab_rebalance_cond);

    return REASSIGN_OK;
}

void* memslab::memory_allocate(size_t size) {
    void *ret;

    if (this->mem_base == NULL) {
        /* We are not using a preallocated large memory chunk */
        ret = malloc(size);
    } else {
        ret = this->mem_current;

        if (size > this->mem_avail)
            return NULL;

        /* mem_current pointer _must_ be aligned!!! */
        if (size % CHUNK_ALIGN_BYTES)
            size += CHUNK_ALIGN_BYTES - (size % CHUNK_ALIGN_BYTES);

        this->mem_current = ((char*) this->mem_current) + size;
        this->mem_avail = size < this->mem_avail ? this->mem_avail - size : 0;
    }

    return ret;
}


/* Iterate at most once through the slab classes and pick a "random" source.
 * I like this better than calling rand() since rand() is slow enough that we
 * can just check all of the classes once instead.
 */
int memslab::slabs_reassign_pick_any(int dst) {
    static int cur = POWER_SMALLEST - 1;
    int tries = this->power_largest - POWER_SMALLEST + 1;

    for (; tries > 0; tries--) {
        cur++;
        if (cur > this->power_largest)
            cur = POWER_SMALLEST;
        if (cur == dst)
            continue;
        if (this->slabclass[cur].slabs > 1)
            return cur;
    }

    return -1;
}

void memslab::slabs_preallocate (const unsigned int maxslabs) {
    int i;
    unsigned int prealloc = 0;

    /* pre-allocate a 1MB slab in every size class so people don't get
       confused by non-intuitive "SERVER_ERROR out of memory"
       messages.  this is the most common question on the mailing
       list.  if you really don't want this, you can rebuild without
       these three lines.  */

    for (i = POWER_SMALLEST; i <= POWER_LARGEST; i++) {
        if (++prealloc > maxslabs)
            return;
        if (this->do_slabs_newslab(i) == 0) { /// VERBOSE
            std::cerr << "Error while preallocating slab memory!" << std::endl;
            std::cerr << "If using -L or other prealloc options, max memory must be ";
            std::cerr << "at least " << this->power_largest << " megabytes." << std::endl;
            exit(1);
        }
    }

}

/**** Metodi pubblici ****/
unsigned int memslab::slabs_clsid(const size_t size) {
    int res = POWER_SMALLEST;

    if (size == 0)
        return 0;

    while (size > this->slabclass[res].size)
        if (res++ == this->power_largest)     /* won't fit in the biggest slab */
            return 0;

    return res;
}

void* memslab::slabs_alloc(size_t size, unsigned int id) {
    void *ret;

//    pthread_mutex_lock(&this->slabs_lock);
    this->slabs_lock->lock();
    ret = this->do_slabs_alloc(size, id);
    this->slabs_lock->unlock();
//    pthread_mutex_unlock(&this->slabs_lock);

    return ret;
}

void memslab::slabs_free(void *ptr, size_t size, unsigned int id) {
//   pthread_mutex_lock(&this->slabs_lock);
    this->slabs_lock->lock();
    this->do_slabs_free(ptr, size, id);
    this->slabs_lock->unlock();
//    pthread_mutex_unlock(&this->slabs_lock);
}

enum reassign_result_type memslab::slabs_reassign(int src, int dst) {
    enum reassign_result_type ret;

//    if (pthread_mutex_trylock(&this->slabs_rebalance_lock) != 0) {
    if (this->slabs_rebalance_lock->trylock() != 0) {
        ret = REASSIGN_RUNNING;
    }
    else {
        ret = this->do_slabs_reassign(src, dst);
        this->slabs_rebalance_lock->unlock();
//        pthread_mutex_unlock(&this->slabs_rebalance_lock);
    }

    return ret;
}

void memslab::slabs_adjust_mem_requested(unsigned int id, size_t old, size_t ntotal) {
//    pthread_mutex_lock(&this->slabs_lock);
    this->slabs_lock->lock();

    if (id < POWER_SMALLEST || id > (unsigned) this->power_largest) {
        std::cerr << "Internal error! Invalid slab class" << std::endl; /// VERBOSE
        abort();
    }

    slabclass_t *p = &this->slabclass[id];
    p->requested = p->requested - old + ntotal;

    this->slabs_lock->unlock();
//    mutex_unlock(&this->slabs_lock);
}

int memslab::slab_automove_decision(int *src, int *dst) {
    static uint64_t evicted_old[POWER_LARGEST];
    static unsigned int slab_zeroes[POWER_LARGEST];
    static unsigned int slab_winner = 0;
    static unsigned int slab_wins   = 0;
    uint64_t evicted_new[POWER_LARGEST];
    uint64_t evicted_diff = 0;
    uint64_t evicted_max  = 0;
    unsigned int highest_slab = 0;
    unsigned int total_pages[POWER_LARGEST];
    int i;
    int source = 0;
    int dest = 0;
    static rel_time_t next_run;

    /* Run less frequently than the slabmove tester. */
    if (current_time >= next_run)
        next_run = current_time + 10;
    else
        return 0;

//    mutex_lock(&this->cache_lock);
    this->cache_lock->lock();

    for (i = POWER_SMALLEST; i < this->power_largest; i++)
        total_pages[i] = this->slabclass[i].slabs;

    this->cache_lock->unlock();
//    mutex_unlock(&this->cache_lock);

    /* Find a candidate source; something with zero evicts 3+ times */
    for (i = POWER_SMALLEST; i < this->power_largest; i++) {
        evicted_diff = evicted_new[i] - evicted_old[i];
        if (evicted_diff == 0 && total_pages[i] > 2) {
            slab_zeroes[i]++;
            if (source == 0 && slab_zeroes[i] >= 3)
                source = i;
        } else {
            slab_zeroes[i] = 0;
            if (evicted_diff > evicted_max) {
                evicted_max = evicted_diff;
                highest_slab = i;
            }
        }
        evicted_old[i] = evicted_new[i];
    }

    /* Pick a valid destination */
    if (slab_winner != 0 && slab_winner == highest_slab) {
        slab_wins++;
        if (slab_wins >= 3)
            dest = slab_winner;
    } else {
        slab_wins = 1;
        slab_winner = highest_slab;
    }

    if (source && dest) {
        *src = source;
        *dst = dest;
        return 1;
    }
    return 0;
}

void memslab::_slab_maintenance_thread(void) {
   int src, dest;

    while (this->do_run_slab_thread) {
        /// SETTINGS
        if (settings.slab_automove == 1) {
            if (this->slab_automove_decision(&src, &dest) == 1) {
                /* Blind to the return codes. It will retry on its own */
                this->slabs_reassign(src, dest);
            }
            sleep(1);
        } else {
            /* Don't wake as often if we're not enabled.
             * This is lazier than setting up a condition right now. */
            sleep(5);
        }
    }
}

int memslab::start_slab_maintenance_thread(void) {
    int ret;
    char *env = getenv("MEMCACHED_SLAB_BULK_CHECK");

    this->slab_rebalance_signal = 0;
    this->slab_rebal.slab_start = NULL;

    if (env != NULL) {
        this->slab_bulk_check = atoi(env);
        if (this->slab_bulk_check == 0) {
            this->slab_bulk_check = DEFAULT_SLAB_BULK_CHECK;
        }
    }

    if (pthread_cond_init(&this->slab_rebalance_cond, NULL) != 0) { /// VERBOSE
        std::cerr << "Can't initialize rebalance condition" << std::endl;
        return -1;
    }

//    pthread_mutex_init(&this->slabs_rebalance_lock, NULL);
    this->slabs_rebalance_lock = new mutex();

    if ((ret = pthread_create(&this->slab_maintenance_tid, NULL,
                              slab_maintenance_thread, (void*) this)) != 0) { /// VERBOSE
        std::cerr << "Can't reate slab maintenance thread: " << strerror(ret) << std::cerr;
        return -1;
    }
    if ((ret = pthread_create(&this->slab_rebalance_tid, NULL,
                              slab_rebalance_thread, (void*) this)) != 0) { /// VERBOSE
        std::cerr << "Can't create rebalance thread: " << strerror(ret) << std::endl;
        return -1;
    }

    return 0;
}


void memslab::stop_slab_maintenance_thread(void) {
//    mutex_lock(&this->cache_lock);
    this->cache_lock->lock();
    this->do_run_slab_thread = 0;
    this->do_run_slab_rebalance_thread = 0;
    pthread_cond_signal(&this->slab_maintenance_cond);
    this->cache_lock->unlock();
//    mutex_unlock(&this->cache_lock);

    /* Wait for the maintenance thread to stop */
    pthread_join(this->slab_maintenance_tid, NULL);
    pthread_join(this->slab_rebalance_tid, NULL);
}


void memslab::slabs_rebalancer_pause(void) {
    this->slabs_rebalance_lock->lock();
//	pthread_mutex_lock(&this->slabs_rebalance_lock);
}


void memslab::slabs_rebalancer_resume(void) {
    this->slabs_rebalance_lock->unlock();
//	pthread_mutex_unlock(&this->slabs_rebalance_lock);
}

int memslab::slab_rebalance_start(void) {
    slabclass_t *s_cls;
    int no_go = 0;

//    pthread_mutex_lock(&this->cache_lock);
//    pthread_mutex_lock(&this->slabs_lock);
    this->cache_lock->lock();
    this->slabs_lock->lock();

    if (this->slab_rebal.s_clsid < POWER_SMALLEST ||
        this->slab_rebal.s_clsid > this->power_largest  ||
        this->slab_rebal.d_clsid < POWER_SMALLEST ||
        this->slab_rebal.d_clsid > this->power_largest  ||
        this->slab_rebal.s_clsid == this->slab_rebal.d_clsid)
        no_go = -2;

    s_cls = &this->slabclass[this->slab_rebal.s_clsid];

    if (!this->grow_slab_list(this->slab_rebal.d_clsid)) {
        no_go = -1;
    }

    if (s_cls->slabs < 2)
        no_go = -3;

    if (no_go != 0) {
        this->slabs_lock->unlock();
        this->cache_lock->unlock();
//        pthread_mutex_unlock(&this->slabs_lock);
//        pthread_mutex_unlock(&this->cache_lock);
        return no_go; /* Should use a wrapper function... */
    }

    s_cls->killing = 1;

    this->slab_rebal.slab_start = s_cls->slab_list[s_cls->killing - 1];
    this->slab_rebal.slab_end = (char *) this->slab_rebal.slab_start + (s_cls->size * s_cls->perslab);
    this->slab_rebal.slab_pos = this->slab_rebal.slab_start;
    this->slab_rebal.done = 0;

    /* Also tells do_item_get to search for items in this slab */
    this->slab_rebalance_signal = 2;

    /// SETTINGS
    if (settings.verbose > 1) /// VERBOSE
        std::cerr << "Started a slab rebalance" << std::endl;

    this->slabs_lock->unlock();
    this->cache_lock->unlock();
//    pthread_mutex_unlock(&this->slabs_lock);
//    pthread_mutex_unlock(&this->cache_lock);

    return 0;
}

int memslab::slab_rebalance_move(void) {
    slabclass_t *s_cls;
    int x;
    int was_busy = 0;
    int refcount = 0;
    enum move_status status = MOVE_PASS;

//    pthread_mutex_lock(&this->cache_lock);
//    pthread_mutex_lock(&this->slabs_lock);
    this->cache_lock->lock();
    this->slabs_lock->lock();

    s_cls = &this->slabclass[this->slab_rebal.s_clsid];

    for (x = 0; x < this->slab_bulk_check; x++) {
        item *it = (item *) this->slab_rebal.slab_pos;
        status = MOVE_PASS;
        if (it->slabs_clsid != 255) {
            mutex *hold_lock = NULL;
            uint32_t hv = hash(ITEM_key(it), it->nkey);

            if ((hold_lock = item_trylock(hv)) == NULL) {
                status = MOVE_LOCKED;
            } else {
                refcount = refcount_incr(&it->refcount);
                if (refcount == 1) { /* item is unlinked, unused */
                    if (it->it_flags & ITEM_SLABBED) {
                        /* remove from slab freelist */
                        if (s_cls->slots == it) {
                            s_cls->slots = it->next;
                        }
                        if (it->next) it->next->prev = it->prev;
                        if (it->prev) it->prev->next = it->next;
                        s_cls->sl_curr--;
                        status = MOVE_DONE;
                    } else {
                        status = MOVE_BUSY;
                    }
                }
                else if (refcount == 2) { /* item is linked but not busy */
                    if ((it->it_flags & ITEM_LINKED) != 0) {
                        do_item_unlink_nolock(it, hash(ITEM_key(it), it->nkey));
                        status = MOVE_DONE;
                    } else {
                        /* refcount == 1 + !ITEM_LINKED means the item is being
                         * uploaded to, or was just unlinked but hasn't been freed
                         * yet. Let it bleed off on its own and try again later */
                        status = MOVE_BUSY;
                    }
                } else {
                    /// SETTINGS
                    if (settings.verbose > 2) { /// VERBOSE
                        fprintf(stderr, "Slab reassign hit a busy item: refcount: %d (%d -> %d)\n",
                            it->refcount, slab_rebal.s_clsid, slab_rebal.d_clsid);
                    }
                    status = MOVE_BUSY;
                }
                hold_lock->unlock();
//                mutex_unlock((pthread_mutex_t *) hold_lock);
            }
        }

        switch (status) {
            case MOVE_DONE:
                it->refcount = 0;
                it->it_flags = 0;
                it->slabs_clsid = 255;
                break;
            case MOVE_BUSY:
                refcount_decr(&it->refcount);
            case MOVE_LOCKED:
                this->slab_rebal.busy_items++;
                was_busy++;
                break;
            case MOVE_PASS:
                break;
        }

        this->slab_rebal.slab_pos = (char *) this->slab_rebal.slab_pos + s_cls->size;

        if (this->slab_rebal.slab_pos >= this->slab_rebal.slab_end)
            break;
    }

    if (this->slab_rebal.slab_pos >= this->slab_rebal.slab_end) {
        /* Some items were busy, start again from the top */
        if (this->slab_rebal.busy_items) {
            this->slab_rebal.slab_pos = this->slab_rebal.slab_start;
            this->slab_rebal.busy_items = 0;
        } else {
            this->slab_rebal.done++;
        }
    }

    this->slabs_lock->unlock();
    this->cache_lock->unlock();
//    pthread_mutex_unlock(&this->slabs_lock);
//    pthread_mutex_unlock(&this->cache_lock);

    return was_busy;
}

void memslab::slab_rebalance_finish(void) {
    slabclass_t *s_cls;
    slabclass_t *d_cls;

//    pthread_mutex_lock(&this->cache_lock);
//    pthread_mutex_lock(&this->slabs_lock);
    this->cache_lock->lock();
    this->slabs_lock->lock();

    s_cls = &this->slabclass[this->slab_rebal.s_clsid];
    d_cls = &this->slabclass[this->slab_rebal.d_clsid];

    /* At this point the stolen slab is completely clear */
    s_cls->slab_list[s_cls->killing - 1] = s_cls->slab_list[s_cls->slabs - 1];
    s_cls->slabs--;
    s_cls->killing = 0;

    /// SETTINGS
    memset(this->slab_rebal.slab_start, 0, (size_t) settings.item_size_max);

    d_cls->slab_list[d_cls->slabs++] = this->slab_rebal.slab_start;
    this->split_slab_page_into_freelist((char*) this->slab_rebal.slab_start, this->slab_rebal.d_clsid);

    this->slab_rebal.done       = 0;
    this->slab_rebal.s_clsid    = 0;
    this->slab_rebal.d_clsid    = 0;
    this->slab_rebal.slab_start = NULL;
    this->slab_rebal.slab_end   = NULL;
    this->slab_rebal.slab_pos   = NULL;

    this->slab_rebalance_signal = 0;

//    pthread_mutex_unlock(&this->slabs_lock);
//    pthread_mutex_unlock(&this->cache_lock);
    this->slabs_lock->unlock();
    this->cache_lock->unlock();

    /// SETTINGS
    if (settings.verbose > 1) /// VERBOSE
        std::cerr << "Finished a slab move" << std::endl;
}

void memslab::_slab_rebalance_thread(void) {
    int was_busy = 0;
    /* So we first pass into cond_wait with the mutex held */
//    mutex_lock(&this->slabs_rebalance_lock);
    this->slabs_rebalance_lock->lock();

    while (this->do_run_slab_rebalance_thread) {
        if (this->slab_rebalance_signal == 1) {
            if (this->slab_rebalance_start() < 0) {
                /* Handle errors with more specifity as required. */
                this->slab_rebalance_signal = 0;
            }

            was_busy = 0;
        } else if (this->slab_rebalance_signal && this->slab_rebal.slab_start != NULL) {
            was_busy = this->slab_rebalance_move();
        }

        if (this->slab_rebal.done) {
            this->slab_rebalance_finish();
        } else if (was_busy) {
            /* Stuck waiting for some items to unlock, so slow down a bit
             * to give them a chance to free up */
            usleep(50);
        }

        if (this->slab_rebalance_signal == 0) {
            /* always hold this lock while we're running */
//            pthread_cond_wait(&this->slab_rebalance_cond, &this->slabs_rebalance_lock);
            this->slabs_rebalance_lock->cond_wait(&this->slab_rebalance_cond);
        }
    }
}


/*** items ***/
unsigned short memslab::refcount_incr(unsigned short *refcount) {
#ifdef HAVE_GCC_ATOMICS
    return __sync_add_and_fetch(refcount, 1);
#elif defined(__sun)
    return atomic_inc_ushort_nv(refcount);
#else
    unsigned short res;
    this->atomics_mutex->lock();
//    mutex_lock(&this->atomics_mutex);
    (*refcount)++;
    res = *refcount;
    this->atomics_mutex->unlock();
//    mutex_unlock(&this->atomics_mutex);
    return res;
#endif
}

unsigned short memslab::refcount_decr(unsigned short *refcount) {
#ifdef HAVE_GCC_ATOMICS
    return __sync_sub_and_fetch(refcount, 1);
#elif defined(__sun)
    return atomic_dec_ushort_nv(refcount);
#else
    unsigned short res;
    this->atomics_mutex->lock();
//    mutex_lock(&this->atomics_mutex);
    (*refcount)--;
    res = *refcount;
//    mutex_unlock(&this->atomics_mutex);
    this->atomics_mutex->unlock();
    return res;
#endif
}


void memslab::item_lock(uint32_t hv) {
    uint8_t *lock_type = (uint8_t *) pthread_getspecific(this->item_lock_type_key);

//    if (likely(*lock_type == ITEM_LOCK_GRANULAR))
    if (*lock_type == ITEM_LOCK_GRANULAR)
        this->item_locks[(hv & hashmask(this->hashpower)) % this->item_lock_count]->lock();
//        mutex_lock(&this->item_locks[(hv & hashmask(this->hashpower)) % this->item_lock_count]);
    else {
        this->item_global_lock->lock();
//        mutex_lock(&this->item_global_lock);
    }
}

void memslab::item_unlock(uint32_t hv) {
    uint8_t *lock_type = (uint8_t *) pthread_getspecific(this->item_lock_type_key);

//    if (likely(*lock_type == ITEM_LOCK_GRANULAR))
    if (*lock_type == ITEM_LOCK_GRANULAR) {
        this->item_locks[(hv & hashmask(this->hashpower)) % this->item_lock_count]->unlock();
//        mutex_unlock(&this->item_locks[(hv & hashmask(this->hashpower)) % this->item_lock_count]);
    }
    else {
        this->item_global_lock->unlock();
//        mutex_unlock(&this->item_global_lock);
    }
}

//void *memslab::item_trylock(uint32_t hv) {
mutex *memslab::item_trylock(uint32_t hv) {
//    pthread_mutex_t *lock =
//        &this->item_locks[(hv & hashmask(this->hashpower)) % this->item_lock_count];
    mutex *l =
        this->item_locks[(hv & hashmask(this->hashpower)) & this->item_lock_count];

    return (l->trylock() == 0 ? l : NULL);
//    return (pthread_mutex_trylock(lock) == 0 ? lock : NULL);
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
size_t memslab::
    item_make_header(const uint8_t nkey, const int flags, const int nbytes,
                     char *suffix, uint8_t *nsuffix) {
    /* suffix is defined at 40 chars elsewhere.. */
    *nsuffix = (uint8_t) snprintf(suffix, 40, " %d %d\r\n", flags, nbytes - 2);
    return sizeof(item) + nkey + *nsuffix + nbytes;
}


void memslab::item_link_q(item *it) { /* item is the new head */
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
}

void memslab::item_unlink_q(item *it) {
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
}


item *memslab::
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

    if ((id = this->slabs_clsid(ntotal)) == 0) /// SLABS
        return 0;

//    mutex_lock(&this->cache_lock);
    this->cache_lock->lock();
    /* do a quick check if we have any expired items in the tail.. */
    int tries = 5;
    int tried_alloc = 0;
    item *search;
//    void *hold_lock = NULL;
    mutex *hold_lock = NULL;
    rel_time_t oldest_live = settings.oldest_live;

    search = this->tails[id];
    /* We walk up *only* for locked items. Never searching for expired.
     * Waste of CPU for almost all deployments */
    for (; tries > 0 && search != NULL; tries--, search=search->prev) {
        uint32_t hv = hash(ITEM_key(search), search->nkey);
        /* Attempt to hash item lock the "search" item. If locked, no
         * other callers can incr the refcount */
        /* FIXME: I think we need to mask the hv here for comparison? */
        if (hv != cur_hv && (hold_lock = item_trylock(hv)) == NULL)
            continue;
        /* Now see if the item is refcount locked */
        if (this->refcount_incr(&search->refcount) != 2) { /// THREADS
            this->refcount_decr(&search->refcount);
            /* Old rare bug could cause a refcount leak. We haven't seen
             * it in years, but we leave this code in to prevent failures
             * just in case */
            if (search->time + TAIL_REPAIR_TIME < current_time) {
                search->refcount = 1;
                this->do_item_unlink_nolock(search, hv);
            }
            if (hold_lock)
                hold_lock->unlock();
//                mutex_unlock((pthread_mutex_t *) hold_lock);

            continue;
        }

        /* Expired or flushed */
        if ((search->exptime != 0 && search->exptime < current_time)
            || (search->time <= oldest_live && oldest_live <= current_time)) {

            it = search;
            this->slabs_adjust_mem_requested(it->slabs_clsid, ITEM_ntotal(it), ntotal); /// SLABS
            this->do_item_unlink_nolock(it, hv);
            /* Initialize the item block: */
            it->slabs_clsid = 0;
        }
        else if ((it = (item*) this->slabs_alloc(ntotal, id)) == NULL) { /// SLABS
            tried_alloc = 1;

            if (settings.evict_to_free != 0) {
                it = search;
                this->slabs_adjust_mem_requested(it->slabs_clsid, ITEM_ntotal(it), ntotal); /// STATS
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
                    this->slabs_reassign(-1, id); /// SLABS
            }
        }

        this->refcount_decr(&search->refcount); /// THREADS
        /* If hash values were equal, we don't grab a second lock */
        if (hold_lock)
            hold_lock->unlock();
//            mutex_unlock((pthread_mutex_t *) hold_lock);

        break;
    }

    if (!tried_alloc && (tries == 0 || search == NULL))
        it = (item*) this->slabs_alloc(ntotal, id); /// SLABS

    if (it == NULL) {
        this->cache_lock->unlock();
//        mutex_unlock(&this->cache_lock);
        return NULL;
    }

    assert(it->slabs_clsid == 0);
    assert(it != this->heads[id]);

    /* Item initialization can happen outside of the lock; the item's already
     * been removed from the slab LRU.
     */
    it->refcount = 1;     /* the caller will have a reference */
//    mutex_unlock(&this->cache_lock);
    this->cache_lock->unlock();
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

void memslab::item_free(item *it) {
    size_t ntotal = ITEM_ntotal(it);
    unsigned int clsid;

    assert((it->it_flags & ITEM_LINKED) == 0);
    assert(it != this->heads[it->slabs_clsid]);
    assert(it != this->tails[it->slabs_clsid]);
    assert(it->refcount == 0);

    /* so slab size changer can tell later if item is already free or not */
    clsid = it->slabs_clsid;
    it->slabs_clsid = 0;
    this->slabs_free(it, ntotal, clsid);
}

bool memslab::
    item_size_ok(const size_t nkey, const int flags, const int nbytes) {

    char prefix[40];
    uint8_t nsuffix;
    size_t ntotal =
        this->item_make_header(nkey + 1, flags, nbytes, prefix, &nsuffix) +
        (settings.use_cas ? sizeof(uint64_t) : 0);

    return this->slabs_clsid(ntotal) != 0;
}

/** may fail if transgresses limits */
int memslab::do_item_link(item *it, const uint32_t hv) {
    assert((it->it_flags & (ITEM_LINKED|ITEM_SLABBED)) == 0);
//    mutex_lock(&this->cache_lock);
    this->cache_lock->lock();

    it->it_flags |= ITEM_LINKED;
    it->time = current_time;

    /* Allocate a new CAS ID on link. */
    ITEM_set_cas(it, (settings.use_cas) ? get_cas_id() : 0);

    this->assoc_insert(it, hv);
    this->item_link_q(it);
    this->refcount_incr(&it->refcount);

//    mutex_unlock(&this->cache_lock);
    this->cache_lock->unlock();

    return 1;
}

void memslab::do_item_unlink(item *it, const uint32_t hv) {
//    mutex_lock(&this->cache_lock);
    this->cache_lock->lock();
    this->do_item_unlink_nolock(it, hv);
    this->cache_lock->unlock();
//    mutex_unlock(&this->cache_lock);
}

void memslab::do_item_unlink_nolock(item *it, const uint32_t hv) {
    if ((it->it_flags & ITEM_LINKED) != 0) {
        it->it_flags &= ~ITEM_LINKED;
        this->assoc_delete(ITEM_key(it), it->nkey, hv);
        this->item_unlink_q(it);
        this->do_item_remove(it);
    }
}

void memslab::do_item_remove(item *it) {
    assert((it->it_flags & ITEM_SLABBED) == 0);

    if (this->refcount_decr(&it->refcount) == 0)
        this->item_free(it);
}

/** update LRU time to current and reposition */
void memslab::do_item_update(item *it) {
    if (it->time < current_time - ITEM_UPDATE_INTERVAL) {
        assert((it->it_flags & ITEM_SLABBED) == 0);

//        mutex_lock(&this->cache_lock);
        this->cache_lock->lock();
        if ((it->it_flags & ITEM_LINKED) != 0) {
            this->item_unlink_q(it);
            it->time = current_time;
            this->item_link_q(it);
        }
        this->cache_lock->unlock();
//        mutex_unlock(&this->cache_lock);
    }
}

int memslab::do_item_replace(item *it, item *new_it, const uint32_t hv) {
    assert((it->it_flags & ITEM_SLABBED) == 0);
    this->do_item_unlink(it, hv);
    return this->do_item_link(new_it, hv);
}

item *memslab::
    do_item_get(const char *key, const size_t nkey, const uint32_t hv) {
    //mutex_lock(&this->cache_lock);
    item *it = this->assoc_find(key, nkey, hv);

    if (it != NULL) {
        this->refcount_incr(&it->refcount);
        /* Optimization for slab reassignment. prevents popular items from
         * jamming in busy wait. Can only do this here to satisfy lock order
         * of item_lock, this->cache_lock, slabs_lock. */
        if (this->slab_rebalance_signal && /// SLABS
            ((void *)it >= this->slab_rebal.slab_start &&
            (void *)it < this->slab_rebal.slab_end)) {

            this->do_item_unlink_nolock(it, hv);
            this->do_item_remove(it);
            it = NULL;
        }
    }
    //mutex_unlock(&this->cache_lock);
    int was_found = 0; /// VERBOSE

    if (settings.verbose > 2) { /// VERBOSE
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

            if (was_found) /// VERBOSE
                std::cerr << " -nuked by flush";

        } else if (it->exptime != 0 && it->exptime <= current_time) {
            this->do_item_unlink(it, hv);
            this->do_item_remove(it);

            it = NULL;

            if (was_found) /// VERBOSE
                std::cerr << " -nuked by expire";

        } else {
            it->it_flags |= ITEM_FETCHED;
        }
    }

    if (settings.verbose > 2) /// VERBOSE
        std::cerr << std::endl;

    return it;
}

int memslab::do_store_item(item *it, const uint32_t hv) {
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


item *memslab::item_alloc(char *key, size_t nkey, int flags, int nbytes) {
    item *it;
    /* do_item_alloc handles its own locks */
    it = this->do_item_alloc(key, nkey, flags, (rel_time_t) 0, nbytes, 0);

    return it;
}

item *memslab::item_get(const char *key, const size_t nkey) {
    item *it;
    uint32_t hv = hash(key, nkey);

    this->item_lock(hv);
    it = this->do_item_get(key, nkey, hv);
    this->item_unlock(hv);

    return it;
}

void memslab::item_remove(item *item) {
    uint32_t hv;
    hv = hash(ITEM_key(item), item->nkey);

    this->item_lock(hv);
    this->do_item_remove(item);
    this->item_unlock(hv);
}

int memslab::item_replace(item *old_it, item *new_it, const uint32_t hv) {
	return this->do_item_replace(old_it, new_it, hv);
}

int memslab::item_link(item *item) {
    int ret;
    uint32_t hv = hash(ITEM_key(item), item->nkey);

    this->item_lock(hv);
    ret = this->do_item_link(item, hv);
    this->item_unlock(hv);

    return ret;
}

void memslab::item_unlink(item *item) {
    uint32_t hv = hash(ITEM_key(item), item->nkey);

    this->item_lock(hv);
    this->do_item_unlink(item, hv);
    this->item_unlock(hv);
}

void memslab::item_update(item *item) {
    uint32_t hv = hash(ITEM_key(item), item->nkey);

    this->item_lock(hv);
    this->do_item_update(item);
    this->item_unlock(hv);
}

int memslab::store_item(item *item) {
    int ret;
    uint32_t hv = hash(ITEM_key(item), item->nkey);

    this->item_lock(hv);
    ret = this->do_store_item(item, hv);
    this->item_unlock(hv);

    return ret;
}



/*** threads ***/
void* assoc_maintenance_thread(void *arg) {
    return ((memslab *) arg)->_assoc_maintenance_thread();
}

void *slab_maintenance_thread(void* arg) {
    ((memslab *) arg)->_slab_maintenance_thread();
    return NULL;
}

void *slab_rebalance_thread(void* arg) {
    ((memslab *) arg)->_slab_rebalance_thread();
    return NULL;
}
