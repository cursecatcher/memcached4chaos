#ifndef ENGINE_HPP
#define ENGINE_HPP

#include <string.h>

#include "lru.hpp"
#include "const_types.h"

/** previous declarations **/
class LRU;


class DataCache {
private:
    struct config config;
    LRU *lru;

public:
    DataCache();

    bool get_item(const char *key, int32_t &bufflen, void **outbuffer);
    bool store_item(const char *key, const void *inbuffer, int32_t bufflen);
    bool delete_item(const char *key);
};
#endif
