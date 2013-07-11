#include <iostream>
#include "memslab.hpp"

using namespace std;

void init_settings(void);

class datacache {
public:
    datacache();
    int start_cache();

    int get_item(const char *key, int32_t& buffLen, void **returnBuffer);
    int store_item(const char *key, const void *buffer, int32_t bufferLen);
    int delete_item(const char *key);

private:
    memslab *cache;
};
