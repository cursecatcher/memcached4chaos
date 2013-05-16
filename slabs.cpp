#include "slabs.hpp"


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

    int i = POWER_SMALLEST - 1; /// SETTINGS
    unsigned int size = sizeof(item);//sizeof(item) + settings.chunk_size;

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
    while (++i < POWER_LARGEST && size <= /*settings.item_size_max*/10 / factor) {
        /* Make sure items are always n-byte aligned */
        if (size % CHUNK_ALIGN_BYTES)
            size += CHUNK_ALIGN_BYTES - (size % CHUNK_ALIGN_BYTES);

        this->slabclass[i].size = size;     /// SETTINGS
        this->slabclass[i].perslab = /*settings.item_size_max*/10 / this->slabclass[i].size;
        size *= factor;
        /*  /// SETTINGS
        if (settings.verbose > 1) {
            fprintf(stderr, "slab class %3d: chunk size %9u perslab %7u\n",
                    i, slabclass[i].size, slabclass[i].perslab);
        } */
    }

    this->power_largest = i;     /// SETTINGS
    this->slabclass[this->power_largest].size = 10;//settings.item_size_max;
    this->slabclass[this->power_largest].perslab = 1;
    /* /// SETTINGS
    if (settings.verbose > 1) {
        fprintf(stderr, "slab class %3d: chunk size %9u perslab %7u\n",
                i, slabclass[i].size, slabclass[i].perslab);
    } */

    /* for the test suite:  faking of how much we've already malloc'd */
    {
        char *t_initial_malloc = getenv("T_MEMD_INITIAL_MALLOC");
        if (t_initial_malloc) {
            this->mem_malloced = (size_t)atol(t_initial_malloc);
        }

    }
/*
    if (prealloc) {
        slabs_preallocate(this->power_largest);
    } */
}


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
	return NULL;
}


void slab_allocator::slabs_free(void *ptr, size_t size, unsigned int id) {
	;
}


enum reassign_result_type slab_allocator::slabs_reassign(int src, int dst) {
	return (enum reassign_result_type) 0;
}

int slab_allocator::slab_automove_decision(int *src, int *dst) {
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
    return 0;
}

int slab_allocator::slab_rebalance_move(void) {
    return 0;
}

int slab_allocator::slab_rebalance_finish(void) {
    return 0;
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
