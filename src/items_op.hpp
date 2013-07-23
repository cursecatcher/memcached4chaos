#include "const_types.h"

namespace items {
    char* item_get_key(const hash_item* item);
    char* item_get_data(const hash_item* item);
    uint64_t item_get_cas(const hash_item* item);
    uint8_t item_get_clsid(const hash_item* item);

    void item_set_cas(const hash_item* item, const uint64_t val);

    size_t ITEM_ntotal(const hash_item *item, bool use_cas);

    uint64_t get_cas_id(); // spostare in Engine??
}
