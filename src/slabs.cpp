#include "slabs.hpp"

Slabs::Slabs(Engine *engine, const size_t size, const double factor, const bool prealloc) {
    unsigned int size = sizeof(hash_item) + engine->config.chunk_size;

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
    while (++i < POWER_LARGEST && size <= engine->config.item_size_max / factor) {
        /* make sure items are always n-byte aligned */
        if (size % CHUNK_ALIGN_BYTES)
            size += CHUNK_ALIGN_BYTES - (size % CHUNK_ALIGN_BYTES);

        this->slabclass[i].size = size;
        this->slabclass[i].perslab = engine->config.item_size_max / size;
        size *= factor;
    }

    this->power_largest = i;
    this->slabclass[this->power_largest].size = engine->config.item_size_max;
    this->slabclass[this->power_largest].perslab = 1;

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

unsigned int Slabs::slabs_clsid(Engine *engine, const size_t size) {
    int res = POWER_SMALLEST;

    if (size == 0)
        return 0;

    while (size > this->slabclass[res].size)
        if (res++ == this->power_largest) /* won't fit in the biggest slab */
            return 0;

    return res;
}

void *Slabs::slabs_alloc(Engine *engine, const size_t size, unsigned int id) {
    void *ret;

    this->mutex->lock();
    ret = do_slabs_alloc(engine, size, id);
    this->mutex->unlock();

    return ret;
}

void Slabs::slabs_free(Engine *engine, void *ptr, size_t size, unsigned int id) {
    this->mutex->lock();
    do_slabs_free(engine, ptr, size, id);
    this->mutex->unlock();
}

void Slabs::slabs_adjust_mem_requested(Engine *engine, unsigned int id, size_t old, size_t ntotal) {
    this->mutex->lock();
    assert(id >= POWER_SMALLEST && id <= this->power_largest);
    this->slabclass[id].requested += ntotal - old;
    this->mutex->unlock();
}


void *Slabs::do_slabs_alloc(Engine *engine, const size_t size, unsigned int id) {
    void *ret = NULL;

    if (id < POWER_SMALLEST || id > engine->slabs.power_largest)
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
    if (!p->end_page_ptr && !p->sl_curr && !this->do_slabs_newslab(engine, id)) {
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

void Slabs::do_slabs_free(Engine *engine, void *ptr, const size_t size, unsigned int id) {
    if (id < POWER_SMALLEST || id > engine->slabs.power_largest)
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
        void **new_slots = realloc(p->slots, new_size * sizeof(void *));
        if (new_slots == NULL)
            return;
        p->slots = new_slots;
        p->sl_total = new_size;
    }
    p->slots[p->sl_curr++] = ptr;
    p->requested -= size;
}

int Slabs::do_slabs_newslab(Engine *engine, const unsigned int id) {
    slabclass_t *p = this->slabclass[id];
    int len = p->size * p->perslab;
    void *ptr;

    if ((this->mem_limit && this->mem_malloced + len > this->mem_limit && p->slabs > 0) ||
        (!this->grow_slab_list(engine, id)) ||
        ((ptr = this->memory_allocate(engine, (size_t) len)) == NULL) {

        return 0;
    }

    memset(ptr, 0, (size_t) len);
    p->end_page_ptr = ptr;
    p->end_page_free = p->perslab;

    p->slab_list[p->slabs++] = ptr;
    this->mem_malloced += len;

    return 1;

}

bool Slabs::grow_slab_list(Engine *engine, const unsigned int id) {
    slabclass_t *p = &this->slabclass[id];

    if (p->slabs == p->list_size) {
        size_t new_size = (p->list_size != 0) ? p->list_size * 2 : 16;
        void *new_list = realloc(p->slab_list, new_size * sizeof(void *));
        if (new_list == NULL)
            return false;
        p->list_size = new_size;
        p->slab_list = new_list;
    }

    return true;
}

void *Slabs::memory_allocate(Engine *engine, size_t size) {
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