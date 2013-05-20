#include <cstdio>

#include "assoc.hpp"


#define LARGEST_ID POWER_LARGEST

class items_management {
    slab_allocator *slabbing;
    assoc_array *associative;

    item *heads[LARGEST_ID];
    item *tails[LARGEST_ID];
    unsigned int sizes[LARGEST_ID];

    size_t item_make_header(const uint8_t nkey, const int flags,
                             const int nbytes, char *suffix, uint8_t *nsuffix);

public:
    items_management();

    item *do_item_alloc(char *key, const size_t nkey, const int flags, const rel_time_t exptime, const int nbytes, const uint32_t cur_hv);
    void item_free(item *it);
    bool item_size_ok(const size_t nkey, const int flags, const int nbytes);

    int  do_item_link(item *it, const uint32_t hv);     /** may fail if transgresses limits */
    void do_item_unlink(item *it, const uint32_t hv);
    void do_item_unlink_nolock(item *it, const uint32_t hv);
    void do_item_remove(item *it);
    void do_item_update(item *it);   /** update LRU time to current and reposition */
    int  do_item_replace(item *it, item *new_it, const uint32_t hv);
    item *do_item_get(const char *key, const size_t nkey, const uint32_t hv);
};
