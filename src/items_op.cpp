#include "items_op.hpp"

char* items::item_get_key(const hash_item* item) {
    char *key = (char*) (item + 1);

    if (item->iflag & ITEM_WITH_CAS)
        key += sizeof(uint64_t);

    return key;
}

char* items::item_get_data(const hash_item* item) {
    return ((char *) items::item_get_key(item)) + item->nkey;
}

uint64_t items::item_get_cas(const hash_item* item){
    return (item->iflag & ITEM_WITH_CAS) ? (*(uint64_t*)(item+1)) : 0;
}

void items::item_set_cas(const hash_item* item, const uint64_t val) {
    if (item->iflag & ITEM_WITH_CAS)
        *(uint64_t*)(item + 1) = val;
}

size_t items::ITEM_ntotal(const hash_item *item, bool use_cas) {
    return (sizeof(*item) + item->nkey + item->nbytes) + (use_cas ? sizeof(uint64_t) : 0);
}

uint64_t items::get_cas_id(void) {
    static uint64_t cas_id = 0;
    return ++cas_id;
}
