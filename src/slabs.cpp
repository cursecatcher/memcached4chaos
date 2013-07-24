#include "slabs.hpp"

Slabs::Slabs(DataCache *engine, const size_t limit, const double factor, const bool prealloc) {
    this->engine = engine;
    this->mem_limit = limit;

    if (prealloc) {
        /* Allocate everything in a big chunk with malloc */
        if ((this->mem_base = malloc(this->mem_limit)) != NULL) {
            this->mem_current = this->mem_base;
            this->mem_avail = this->mem_limit;
        }
        else {
            throw "ENGINE_ENOMEM";
        }
    }

    memset(this->slabclass, 0, sizeof(this->slabclass));

    int i = POWER_SMALLEST - 1;
    unsigned int size = sizeof(hash_item) + this->engine->config.chunk_size;

    while (++i < POWER_LARGEST && size <= this->engine->config.item_size_max / factor) {
        /* make sure items are always n-byte aligned */
        if (size % CHUNK_ALIGN_BYTES)
            size += CHUNK_ALIGN_BYTES - (size % CHUNK_ALIGN_BYTES);

        this->slabclass[i].size = size;
        this->slabclass[i].perslab = this->engine->config.item_size_max / size;
        size *= factor;
    }

    this->power_largest = i;
    this->slabclass[this->power_largest].size = this->engine->config.item_size_max;
    this->slabclass[this->power_largest].perslab = 1;
    pthread_mutex_init(&this->lock, NULL);

    /* for the test suite: faking of how much we've already malloc'd */
    {
        char *t_initial_malloc = getenv("T_MEMD_INITIAL_MALLOC");
        if (t_initial_malloc)
            this->mem_malloced = (size_t) atol(t_initial_malloc);
    }
#ifndef DONT_PREALLOC_SLABS
    {
        char *pre_alloc = getenv("T_MEMD_SLABS_ALLOC");
        if (pre_alloc == NULL || atoi(pre_alloc) != 0)
            this->slabs_preallocate(this->power_largest);
    }
#endif
}

unsigned int Slabs::slabs_clsid(const size_t size) {
    unsigned res = POWER_SMALLEST;

    if (size == 0)
        return 0;

    while (size > this->slabclass[res].size)
        if (res++ == this->power_largest) /* won't fit in the biggest slab */
            return 0;

    return res;
}

void *Slabs::slabs_alloc(const size_t size, unsigned int id) {
    void *ret;

    pthread_mutex_lock(&this->lock);
    ret = this->do_slabs_alloc(size, id);
    pthread_mutex_unlock(&this->lock);

    return ret;
}

void Slabs::slabs_free(void *ptr, size_t size, unsigned int id) {
    pthread_mutex_lock(&this->lock);
    this->do_slabs_free(ptr, size, id);
    pthread_mutex_unlock(&this->lock);
}

void Slabs::slabs_adjust_mem_requested(unsigned int id, size_t old, size_t ntotal) {
    pthread_mutex_lock(&this->lock);
    assert(id >= POWER_SMALLEST && id <= this->power_largest);
    this->slabclass[id].requested += ntotal - old;
    pthread_mutex_unlock(&this->lock);
}


void *Slabs::do_slabs_alloc(const size_t size, unsigned int id) {
    void *ret = NULL;

    if (id < POWER_SMALLEST || id > this->power_largest)
        return NULL;

#ifdef USE_SYSTEM_MALLOC
    if (this->mem_limit == 0 || this->mem_malloced + size <= this->mem_limit) {
        this->mem_malloced += size;
        ret = malloc(size);
    }
    return ret;
#endif

     slabclass_t *p = &this->slabclass[id];

    /* fail unless we have space at the end of a recently allocated page,
       we have something on our freelist, or we could allocate a new page */
    if (!p->end_page_ptr && !p->sl_curr && !this->do_slabs_newslab(id)) {
        // we don't have more memory available
        ret = NULL;
    }
    else if (p->sl_curr) {
        // return off our freelist
        ret = p->slots[--p->sl_curr];
    }
    else {
        //if we recently allocated a whole page, return from that
        assert(p->end_page_ptr != NULL);
        ret = p->end_page_ptr;
        p->end_page_ptr = (--p->end_page_free != 0) ? ((caddr_t) p->end_page_ptr) + p->size : NULL;
    }

    if (ret)
        p->requested += size;

    return ret;
}

void Slabs::do_slabs_free(void *ptr, const size_t size, unsigned int id) {
    if (id < POWER_SMALLEST || id > this->power_largest)
        return;

#ifdef USE_SYSTEM_MALLOC
    this->mem_malloced -= size;
    free(ptr);
    return;
#endif

    slabclass_t *p = &this->slabclass[id];

    if (p->sl_curr == p->sl_total) {
        // need more space on the free list
        int new_size = (p->sl_total != 0) ? p->sl_total * 2 : 16; // 16 is arbitrary
        void **new_slots = (void **) realloc(p->slots, new_size * sizeof(void *));
        if (new_slots == NULL)
            return;
        p->slots = new_slots;
        p->sl_total = new_size;
    }
    p->slots[p->sl_curr++] = ptr;
    p->requested -= size;
}

int Slabs::do_slabs_newslab(const unsigned int id) {
    slabclass_t *p = &this->slabclass[id];
    int len = p->size * p->perslab;
    void *ptr;

    if ((this->mem_limit && this->mem_malloced + len > this->mem_limit && p->slabs > 0) ||
        (!this->grow_slab_list(id)) ||
        ((ptr = this->memory_allocate((size_t) len)) == NULL)) {

        return 0;
    }

    memset(ptr, 0, (size_t) len);
    p->end_page_ptr = ptr;
    p->end_page_free = p->perslab;

    p->slab_list[p->slabs++] = ptr;
    this->mem_malloced += len;

    return 1;

}

bool Slabs::grow_slab_list(const unsigned int id) {
    slabclass_t *p = &this->slabclass[id];

    if (p->slabs == p->list_size) {
        size_t new_size = (p->list_size != 0) ? p->list_size * 2 : 16;
        void *new_list = realloc(p->slab_list, new_size * sizeof(void *));
        if (new_list == NULL)
            return false;
        p->list_size = new_size;
        p->slab_list = (void **) new_list;
    }

    return true;
}

void *Slabs::memory_allocate(size_t size) {
    void *ret;

    if (this->mem_base == NULL) {
        // we are not using a preallocated large memory chunk
        ret = malloc(size);
    }
    else {
        ret = this->mem_current;

        if (size > this->mem_avail)
            return NULL;
        if (size % CHUNK_ALIGN_BYTES)
            size += CHUNK_ALIGN_BYTES - (size % CHUNK_ALIGN_BYTES);

        this->mem_current = ((char*) this->mem_current) + size;
        this->mem_avail -= (size < this->mem_avail) ? size : 0;
    }

    return ret;
}
