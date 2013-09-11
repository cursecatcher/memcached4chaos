#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <pthread.h>
#include <unistd.h>
#include <fstream>

#include "../cache/datacache.hpp"

#define MIN_LATENCY 5 //ms
#define DELTA_LATENCY 10 //ms


struct stat {
    int nwriters, nreaders;
    int32_t num_opt;
    int32_t store_ok;
    int32_t store_failed;
    int32_t cache_success;
    int32_t cache_miss;
    /* tempi */
};

inline void latency() {
    usleep(MIN_LATENCY + rand() % DELTA_LATENCY);
}

void add_stats(struct stat stats);

void *writer(void *arg);
void *reader(void *arg);
void *control(void *arg);

/* globals */
struct stat tot_stats = {};
pthread_mutex_t stats_lock = PTHREAD_MUTEX_INITIALIZER;
bool exit_flag = false;

int main(int argc, char *argv[]) {
    pthread_t *tid_readers, *tid_writers;
    pthread_t tid;

    srand(time(NULL));

    assert(argc == 3);
    assert(sscanf(argv[1], "%d", &tot_stats.nwriters) == 1);
    assert(tot_stats.nwriters > 0);
    assert(sscanf(argv[2], "%d", &tot_stats.nreaders) == 1);
    assert(tot_stats.nreaders > 0);

    DataCache *cache = new DataCache();
    tid_readers = new pthread_t[tot_stats.nreaders];
    tid_writers = new pthread_t[tot_stats.nwriters];

    for (int i = 0; i < tot_stats.nwriters; i++)
        assert(pthread_create(&tid_writers[i], NULL, writer, (void*) cache) == 0);

    for (int i = 0; i < tot_stats.nreaders; i++)
        assert(pthread_create(&tid_readers[i], NULL, reader, (void*) cache) == 0);

    assert(pthread_create(&tid, NULL, control, NULL) == 0);

    pthread_join(tid, NULL);
    for (int i = 0; i < tot_stats.nwriters; i++)
        pthread_join(tid_writers[i], NULL);

    for (int i = 0; i < tot_stats.nreaders; i++)
        pthread_join(tid_readers[i], NULL);

    std::fstream file;
    file.open("testlog.log", std::fstream::binary | std::fstream::app | std::fstream::out);
    file.write((char *) &tot_stats, sizeof(tot_stats));
    file.close();
    //~ pthread_mutex_lock(&stats_lock);
    //~ cout << "Stats:" << endl;
    //~ cout << "num writers: " << tot_stats.nwriters << endl;
    //~ cout << "num readers: " << tot_stats.nreaders << endl;
    //~ cout << "num opt: " << tot_stats.num_opt << endl;
    //~ cout << "num store ok: " << tot_stats.store_ok << endl;
    //~ cout << "num store failed: " << tot_stats.store_failed << endl;
    //~ cout << "num find ok: " << tot_stats.cache_success << endl;
    //~ cout << "num find failed: " << tot_stats.cache_miss << endl;
    //~ pthread_mutex_unlock(&stats_lock);

    return 0;
}

void add_stats(struct stat stats) {
    pthread_mutex_lock(&stats_lock);
    tot_stats.num_opt += stats.num_opt;
    tot_stats.store_ok += stats.store_ok;
    tot_stats.store_failed += stats.store_failed;
    tot_stats.cache_success += stats.cache_success;
    tot_stats.cache_miss += stats.cache_miss;
    pthread_mutex_unlock(&stats_lock);
}

void *writer(void *arg) {
    DataCache *cache = (DataCache *) arg;
    struct stat local_stats = {};

    char key[10];
    char value[50];
    void *buffer = (void*) value;
    int32_t bufflen = 23; //???

    while (!exit_flag) {
        local_stats.num_opt++;

        if (cache->store_item(key, buffer, bufflen))
            local_stats.store_ok++;
        else
            local_stats.store_failed++;

        /* check ret */
        latency();
    }

    add_stats(local_stats);
    return NULL;
}

void *reader(void *arg) {
    DataCache *cache = (DataCache *) arg;
    struct stat local_stats = {};

    char key[10];
    void *buffer = malloc(50);
    int32_t bytereaded;

    while (!exit_flag) {
        local_stats.num_opt++;

        if (cache->get_item(key, bytereaded, &buffer))
            local_stats.cache_success++;
        else
            local_stats.cache_miss++;
        /* check ret */
        latency();
    }

    add_stats(local_stats);
    return NULL;
}


void *control(void *arg) {
    (void) arg;

    sleep(1);
    exit_flag = true;

    return NULL;
}
