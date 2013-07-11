#include "datacache.hpp"

/*
void init_settings(void) {
    settings.use_cas = true;
//    settings.access = 0700;
//    settings.port = 11211;
//    settings.udpport = 11211;
    // By default this string should be NULL for getaddrinfo()
    settings.inter = NULL;
    settings.maxbytes = 64 * 1024 * 1024; // default is 64MB
//    settings.maxconns = 1024;         // to limit connections-related memory to about 5MB
    settings.verbose = 0;
    settings.oldest_live = 0;
    settings.evict_to_free = 1;       // push old items out of cache when memory runs out
//    settings.socketpath = NULL;       // by default, not using a unix socket
    settings.factor = 1.25;
    settings.chunk_size = 48;         // space for a modest key and value
    settings.num_threads = 4;         // N workers
//    settings.num_threads_per_udp = 0;
//    settings.prefix_delimiter = ':';
//    settings.detail_enabled = 0;
//    settings.reqs_per_event = 20;
//    settings.backlog = 1024;
//    settings.binding_protocol = negotiating_prot;
    settings.item_size_max = 1024 * 1024; // The famous 1MB upper limit.
//    settings.maxconns_fast = false;
    settings.hashpower_init = 0;
    settings.slab_reassign = false;
    settings.slab_automove = 0;
//    settings.shutdown_command = false;
} */


datacache::datacache() {
    this->cache = new memslab(0, 0, 0, 0.0, false);
}

int datacache::start_cache() {
    if (this->cache->start_assoc_maintenance_thread() == -1) {
        cerr << "Can't create associative maintenance thread. Abort" << endl;
    }

    if (this->cache->start_slab_maintenance_thread() == -1) {
        cerr << "Can't create slab maintenance thread. Abort" << endl;
    }

    /** start threads **/

    return 0;
}

int datacache::get_item(const char *key, int32_t& buffLen, void **returnBuffer) {
    return 0;
}

int datacache::store_item(const char *key, const void *buffer, int32_t bufferLen) {
    return 0;
}

int datacache::delete_item(const char *key) {
    return 0;
}
