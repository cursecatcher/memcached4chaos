#include "items_op.hpp"

char* items::item_get_key(const hash_item* item) {
    char *key = (char*) (item + 1);

    if (item->iflag & ITEM_WITH_CAS)
        key += sizeof(uint64_t);

    return key;
}

void* items::item_get_data(const hash_item* item) {
    return ((char *) items::item_get_key(item)) + item->nkey;
}

uint64_t items::item_get_cas(const hash_item* item){
    return (item->iflag & ITEM_WITH_CAS) ? (*(uint64_t*)(item+1)) : 0;
}

uint8_t items::item_get_clsid(const hash_item* item) {
    return 0;
}

void items::item_set_cas(const hash_item* item, const uint64_t val) {
    if (item->iflag & ITEM_WITH_CAS)
        *(uint64_t*)(item + 1) = val;
}
