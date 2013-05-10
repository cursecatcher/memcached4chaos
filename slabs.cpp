#include "slabs.hpp"


slab_allocator::slab_allocator(
        const size_t limit,
        const double factor = 1.25,
        const bool prealloc = false) {

/* init attributes */
    pthread_mutex_init(&this->slabs_lock, NULL);
    pthread_mutex_init(&this->slabs_rebalance_lock, NULL);

    this->mem_base = this->mem_current = NULL;
    this->mem_limit = 0;
    this->mem_malloced = 0;
    this->mem_avail = 0;

/* slabs init */

    int i = POWER_SMALLEST - 1; /// SETTINGS
    unsigned int size = sizeof(item) + settings.chunk_size;

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

    while (++i < POWER_LARGEST && size <= settings.item_size_max / factor) {
        /* Make sure items are always n-byte aligned */
        if (size % CHUNK_ALIGN_BYTES)
            size += CHUNK_ALIGN_BYTES - (size % CHUNK_ALIGN_BYTES);

        this->slabclass[i].size = size;
        this->slabclass[i].perslab = settings.item_size_max / this->slabclass[i].size;
        size *= factor;
        /*  /// SETTINGS
        if (settings.verbose > 1) {
            fprintf(stderr, "slab class %3d: chunk size %9u perslab %7u\n",
                    i, slabclass[i].size, slabclass[i].perslab);
        } */
    }

    this->power_largest = i;
    this->slabclass[this->power_largest].size = settings.item_size_max;
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

    if (prealloc) {
        slabs_preallocate(this->power_largest);
    }
}


