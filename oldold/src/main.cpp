#include "datacache.hpp"


int main(void) {
    datacache *cache;

    cache = new datacache();
    cache->start_cache();


    sleep(5);

}
