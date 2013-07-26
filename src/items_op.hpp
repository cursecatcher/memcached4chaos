#include "const_types.h"

namespace items {
    inline char* item_get_key(const hash_item* item) {
        char *key = (char*) (item + 1);

        if (item->iflag & ITEM_WITH_CAS)
            key += sizeof(uint64_t);

        return key;
    }

    inline char* item_get_data(const hash_item* item) {
        return ((char *) items::item_get_key(item)) + item->nkey;
    }

    inline size_t ITEM_ntotal(const hash_item *item, bool use_cas) {
        return (sizeof(*item) + item->nkey + item->nbytes) + (use_cas ? sizeof(uint64_t) : 0);
    }

    inline uint64_t get_cas_id() {
        static uint64_t cas_id = 0;
        return ++cas_id;
    }

    inline uint64_t item_get_cas(const hash_item* item) {
        return (item->iflag & ITEM_WITH_CAS) ? (*(uint64_t*)(item+1)) : 0;
    }

    inline void item_set_cas(const hash_item* item, const uint64_t val) {
        if (item->iflag & ITEM_WITH_CAS)
            *(uint64_t*)(item + 1) = val;
    }
}
