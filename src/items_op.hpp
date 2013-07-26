#ifndef ITEMS_OP_HPP
#define ITEMS_OP_HPP

#include "const_types.h"

namespace items {
    inline char* item_get_key(const hash_item* item) {
        return (char*) (item + 1);
    }

    inline char* item_get_data(const hash_item* item) {
        return ((char *) items::item_get_key(item)) + item->nkey;
    }

    inline size_t ITEM_ntotal(const hash_item *item) {
        return (sizeof(hash_item) + item->nkey + item->nbytes);
    }
}
#endif
