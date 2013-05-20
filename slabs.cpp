#include "slabs.hpp"

/**** Costruttore ****/
slab_allocator::slab_allocator
    (const size_t limit, const double factor, const bool prealloc) {

/* init attributes */
    pthread_mutex_init(&this->slabs_lock, NULL);
    pthread_mutex_init(&this->slabs_rebalance_lock, NULL);

    this->mem_base = this->mem_current = NULL;
    this->mem_limit = 0;
    this->mem_malloced = 0;
    this->mem_avail = 0;

    this->slab_bulk_check = DEFAULT_SLAB_BULK_CHECK;

/* slabs init */

    int i = POWER_SMALLEST - 1;
    unsigned int size = sizeof(item) + settings.chunk_size; /// SETTINGS

    this->mem_limit = limit;

    if (prealloc) {
        /* Allocate everything in a big chunk with malloc */
        this->mem_base = malloc(this->mem_limit);
        if (this->mem_base != NULL) {
            this->mem_current = this->mem_base;
            this->mem_avail = this->mem_limit;
        } else {
            std::cerr << "Warning: Failed to allocate requested memory in one large chunk." << std::endl;
            std::cerr << "Will allocate in smaller chunks." << std::endl;
        }
    }

    memset(this->slabclass, 0, sizeof(this->slabclass));
    /// SETTINGS
    while (++i < POWER_LARGEST && size <= settings.item_size_max / factor) {
        /* Make sure items are always n-byte aligned */
        if (size % CHUNK_ALIGN_BYTES)
            size += CHUNK_ALIGN_BYTES - (size % CHUNK_ALIGN_BYTES);

        this->slabclass[i].size = size;
        this->slabclass[i].perslab = settings.item_size_max / this->slabclass[i].size; /// SETTINGS
        size *= factor;
        /// SETTINGS
        if (settings.verbose > 1) {
            fprintf(stderr, "slab class %3d: chunk size %9u perslab %7u\n",
                    i, slabclass[i].size, slabclass[i].perslab);
        }
    }

    this->power_largest = i;
    this->slabclass[this->power_largest].size = settings.item_size_max; /// SETTINGS
    this->slabclass[this->power_largest].perslab = 1;
    /// SETTINGS
    if (settings.verbose > 1) {
        fprintf(stderr, "slab class %3d: chunk size %9u perslab %7u\n",
                i, slabclass[i].size, slabclass[i].perslab);
    }

    /* for the test suite:  faking of how much we've already malloc'd */
    {
        char *t_initial_malloc = getenv("T_MEMD_INITIAL_MALLOC");
        if (t_initial_malloc) {
            this->mem_malloced = (size_t)atol(t_initial_malloc);
        }

    }

    if (prealloc) {
        slabs_preallocate(this->power_largest);
    }
}

/**** Metodi privati ****/
int slab_allocator::grow_slab_list(const unsigned int id) {
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

void slab_allocator::split_slab_page_into_freelist(char *ptr, const unsigned int id) {
    slabclass_t *p = &this->slabclass[id];

    for (int x = 0; x < p->perslab; x++) {
        this->do_slabs_free(ptr, 0, id);
        ptr += p->size;
    }
}

int slab_allocator::do_slabs_newslab(const unsigned int id) {
    slabclass_t *p = &this->slabclass[id];
    char *ptr;
    /// SETTINGS
    int len = settings.slab_reassign ? settings.item_size_max
        : p->size * p->perslab;

    if ((this->mem_limit && this->mem_malloced + len > this->mem_limit && p->slabs > 0) ||
        (this->grow_slab_list(id) == 0) ||
        ((ptr = (char*) this->memory_allocate((size_t) len)) == 0)) {

//        MEMCACHED_SLABS_SLABCLASS_ALLOCATE_FAILED(id); /// TRACE
        return 0;
    }

    memset(ptr, 0, (size_t)len);
    this->split_slab_page_into_freelist(ptr, id);

    p->slab_list[p->slabs++] = ptr;
    this->mem_malloced += len;
//    MEMCACHED_SLABS_SLABCLASS_ALLOCATE(id); /// TRACE

    return 1;
}

void *slab_allocator::do_slabs_alloc(const size_t size, unsigned int id) {
    slabclass_t *p;
    void *ret = NULL;
    item *it = NULL;

    if (id < POWER_SMALLEST || id > this->power_largest) {
//        MEMCACHED_SLABS_ALLOCATE_FAILED(size, 0); /// TRACE
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
/*
    if (ret) { /// TRACE
        p->requested += size;
        MEMCACHED_SLABS_ALLOCATE(size, id, p->size, ret); /// TRACE
    } else {
        MEMCACHED_SLABS_ALLOCATE_FAILED(size, id); /// TRACE
    } */

    return ret;
}

void slab_allocator::do_slabs_free(void *ptr, const size_t size, unsigned int id) {
    slabclass_t *p;
    item *it;

    assert(((item *)ptr)->slabs_clsid == 0);
    assert(id >= POWER_SMALLEST && id <= this->power_largest);

    if (id < POWER_SMALLEST || id > this->power_largest)
        return;

//    MEMCACHED_SLABS_FREE(size, id, ptr); /// TRACE
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

enum reassign_result_type slab_allocator::do_slabs_reassign(int src, int dst) {
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

void* slab_allocator::memory_allocate(size_t size) {
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
        this->mem_avail = size < this->mem_avail ?
                          this->mem_avail - size : 0;
    }

    return ret;
}


/* Iterate at most once through the slab classes and pick a "random" source.
 * I like this better than calling rand() since rand() is slow enough that we
 * can just check all of the classes once instead.
 */
int slab_allocator::slabs_reassign_pick_any(int dst) {
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

void slab_allocator::slabs_preallocate (const unsigned int maxslabs) {
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
        if (this->do_slabs_newslab(i) == 0) {
            std::cerr << "Error while preallocating slab memory!" << std::endl;
            std::cerr << "If using -L or other prealloc options, max memory must be ";
            std::cerr << "at least " << this->power_largest << " megabytes." << std::endl;
            exit(1);
        }
    }

}

/**** Metodi pubblici ****/
unsigned int slab_allocator::slabs_clsid(const size_t size) {
    int res = POWER_SMALLEST;

    if (size == 0)
        return 0;

    while (size > slabclass[res].size)
        if (res++ == power_largest)     /* won't fit in the biggest slab */
            return 0;

    return res;
}

void* slab_allocator::slabs_alloc(size_t size, unsigned int id) {
    void *ret;

    pthread_mutex_lock(&this->slabs_lock);
    ret = do_slabs_alloc(size, id);
    pthread_mutex_unlock(&this->slabs_lock);

    return ret;
}

void slab_allocator::slabs_free(void *ptr, size_t size, unsigned int id) {
    pthread_mutex_lock(&this->slabs_lock);
    do_slabs_free(ptr, size, id);
    pthread_mutex_unlock(&this->slabs_lock);
}

enum reassign_result_type slab_allocator::slabs_reassign(int src, int dst) {
    enum reassign_result_type ret;

    if (pthread_mutex_trylock(&this->slabs_rebalance_lock) != 0) {
        ret = REASSIGN_RUNNING;
    }
    else {
        ret = do_slabs_reassign(src, dst);
        pthread_mutex_unlock(&this->slabs_rebalance_lock);
    }

    return ret;
}

void slab_allocator::slabs_adjust_mem_requested(unsigned int id, size_t old, size_t ntotal) {
    pthread_mutex_lock(&this->slabs_lock);

    if (id < POWER_SMALLEST || id > this->power_largest) {
        std::cerr << "Internal error! Invalid slab class" << std::endl;
        abort();
    }

    slabclass_t *p = &this->slabclass[id];
    p->requested = p->requested - old + ntotal;

    pthread_mutex_unlock(&this->slabs_lock);
}

int slab_allocator::slab_automove_decision(int *src, int *dst) {
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

//    item_stats_evictions(evicted_new); /// STATS ?
    pthread_mutex_lock(&cache_lock);

    for (i = POWER_SMALLEST; i < this->power_largest; i++)
        total_pages[i] = this->slabclass[i].slabs;

    pthread_mutex_unlock(&cache_lock);

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

void slab_allocator::_slab_maintenance_thread(void) {
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

int slab_allocator::start_slab_maintenance_thread(void) {
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

    if (pthread_cond_init(&this->slab_rebalance_cond, NULL) != 0) {
        std::cerr << "Can't initialize rebalance condition" << std::endl;
        return -1;
    }

    pthread_mutex_init(&this->slabs_rebalance_lock, NULL);

    if ((ret = pthread_create(&this->maintenance_tid, NULL,
                              slab_maintenance_thread, (void*) this)) != 0) {
        std::cerr << "Can't reate slab maintenance thread: " << strerror(ret) << std::cerr;
        return -1;
    }
    if ((ret = pthread_create(&this->rebalance_tid, NULL,
                              slab_rebalance_thread, (void*) this)) != 0) {
        std::cerr << "Can't create rebalance thread: " << strerror(ret) << std::endl;
        return -1;
    }

    return 0;
}


void slab_allocator::stop_slab_maintenance_thread(void) {
    mutex_lock(&cache_lock);
    this->do_run_slab_thread = 0;
    this->do_run_slab_rebalance_thread = 0;
    pthread_cond_signal(&this->maintenance_cond);
    pthread_mutex_unlock(&cache_lock);

    /* Wait for the maintenance thread to stop */
    pthread_join(this->maintenance_tid, NULL);
    pthread_join(this->rebalance_tid, NULL);
}


void slab_allocator::slabs_rebalancer_pause(void) {
	pthread_mutex_lock(&this->slabs_rebalance_lock);
}


void slab_allocator::slabs_rebalancer_resume(void) {
	pthread_mutex_unlock(&this->slabs_rebalance_lock);
}

int slab_allocator::slab_rebalance_start(void) {
    slabclass_t *s_cls;
    int no_go = 0;

    pthread_mutex_lock(&cache_lock);
    pthread_mutex_lock(&this->slabs_lock);

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
        pthread_mutex_unlock(&this->slabs_lock);
        pthread_mutex_unlock(&cache_lock);
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
    if (settings.verbose > 1)
        std::cerr << "Started a slab rebalance" << std::endl;

    pthread_mutex_unlock(&this->slabs_lock);
    pthread_mutex_unlock(&cache_lock);

/// STATS
/*    STATS_LOCK();
    stats.slab_reassign_running = true;
    STATS_UNLOCK(); */

    return 0;
}

int slab_allocator::slab_rebalance_move(void) {
    slabclass_t *s_cls;
    int x;
    int was_busy = 0;
    int refcount = 0;
    enum move_status status = MOVE_PASS;

    pthread_mutex_lock(&cache_lock);
    pthread_mutex_lock(&this->slabs_lock);

    s_cls = &this->slabclass[this->slab_rebal.s_clsid];

    for (x = 0; x < this->slab_bulk_check; x++) {
        item *it = (item *) this->slab_rebal.slab_pos;
        status = MOVE_PASS;
        if (it->slabs_clsid != 255) {
            void *hold_lock = NULL;
            uint32_t hv = hash::hash_function(ITEM_key(it), it->nkey);
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
                        do_item_unlink_nolock(it, hash::hash_function(ITEM_key(it), it->nkey));
                        status = MOVE_DONE;
                    } else {
                        /* refcount == 1 + !ITEM_LINKED means the item is being
                         * uploaded to, or was just unlinked but hasn't been freed
                         * yet. Let it bleed off on its own and try again later */
                        status = MOVE_BUSY;
                    }
                } else {
                    /// SETTINGS
                    if (settings.verbose > 2) {
                        fprintf(stderr, "Slab reassign hit a busy item: refcount: %d (%d -> %d)\n",
                            it->refcount, slab_rebal.s_clsid, slab_rebal.d_clsid);
                    }
                    status = MOVE_BUSY;
                }
                item_trylock_unlock(hold_lock);
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

    pthread_mutex_unlock(&this->slabs_lock);
    pthread_mutex_unlock(&cache_lock);

    return was_busy;
}

int slab_allocator::slab_rebalance_finish(void) {
    slabclass_t *s_cls;
    slabclass_t *d_cls;

    pthread_mutex_lock(&cache_lock);
    pthread_mutex_lock(&this->slabs_lock);

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

    pthread_mutex_unlock(&this->slabs_lock);
    pthread_mutex_unlock(&cache_lock);

/*    STATS_LOCK(); /// STATS
    stats.slab_reassign_running = false;
    stats.slabs_moved++;
    STATS_UNLOCK(); */
    /// SETTINGS
    if (settings.verbose > 1)
        std::cerr << "Finished a slab move" << std::endl;
}

void slab_allocator::_slab_rebalance_thread(void) {
    int was_busy = 0;
    /* So we first pass into cond_wait with the mutex held */
    mutex_lock(&this->slabs_rebalance_lock);

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
            pthread_cond_wait(&this->slab_rebalance_cond, &this->slabs_rebalance_lock);
        }
    }
}

void *slab_maintenance_thread(void* arg) {
    ((slab_allocator *) arg)->_slab_maintenance_thread();
    return NULL;
}

void *slab_rebalance_thread(void* arg) {
    ((slab_allocator *) arg)->_slab_rebalance_thread();
    return NULL;
}
