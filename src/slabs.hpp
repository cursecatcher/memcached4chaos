
/* powers-of-N allocation structures */

typedef struct {
    unsigned int size;      // sizes of items
    unsigned int perslab;   // how many items per slab

    void **slots;           // list of item ptrs
    unsigned int sl_total;  // size of previous array
    unsigned int sl_curr;   // first free slot

    void *end_page_ptr;         // pointer to next free item at end of page, or 0
    unsigned int end_page_free; // number of items remaining at end of last alloced page

    unsigned int slabs;     // how many slabs were allocated for this class

    void **slab_list;       // array of slab pointers
    unsigned int list_size; // size of prev array

    unsigned int killing;  // index+1 of dying slab, or zero if none
    size_t requested; // The number of requested bytes
} slabclass_t;

class Slabs {
private:
    slabclass_t slabclass[MAX_NUMBER_OF_SLAB_CLASSES];
    size_t mem_limit;
    size_t mem_malloced;
    int power_largest;

    void *mem_base;
    void *mem_current;
    size_t mem_avail;

    pthread_mutex_t lock; // Access to the slab allocator is protected by this lock


    void *do_slabs_alloc(Engine *engine, const size_t size, unsigned int id);
    void do_slabs_free(Engine *engine, void *ptr, const size_t size, unsigned int id);
    int do_slabs_newslab(Engine *engine, const unsigned int id);
    bool grow_slab_list(Engine *engine, const unsigned int id);
    void *memory_allocate(Engine *engine, size_t size);

public:

    Slabs(Engine *engine, const size_t size, const double factor, const bool prealloc);
    unsigned int slabs_clsid(Engine *engine, const size_t size);
    void *slabs_alloc(Engine *engine, const size_t size, unsigned int id);
    void slabs_free(Engine *engine, void *ptr, size_t size, unsigned int id);
    void slabs_adjust_mem_requested(Engine *engine, unsigned int id, size_t old, size_t ntotal);
};
