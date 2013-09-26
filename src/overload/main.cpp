#include "../cache/datacache.hpp"
#include <iostream>
#include <cstdio>
#include <ctime>
#include <pthread.h>

using namespace std;


void *client(void *arg);
void *stat(void *arg);

DataCache *cache;
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
unsigned get_per_sec = 0;
int totclient, actclient;

/* n thread set/get
 * 1 thread stat = stampa quante set / get per sec
 *
 * no limits */

int main(int argc, char *argv[]) {

    if (argc != 2 || sscanf(argv[1], "%d", &totclient) != 1 || totclient < 1) {
        cerr << "Inserire nclient > 0" << endl;
        return -1;
    }

    cache = new DataCache(200);

    for (int i = 0; i < totclient; i++) {
        pthread_t tid;

        pthread_create(&tid, NULL, client, NULL);
    }

    pthread_t tid;

    pthread_create(&tid,NULL, stat,NULL);
    pthread_join(tid,NULL);

    return 0;
}

void *client(void *arg) {
    char key[20];
    void *buffer = malloc(2048);
    int32_t len;
    struct timespec sleeptime = {0, 5000}, rem;

    strcpy(key, "helloworld");

    while (1) {
//        cache->getItem(key, len, &buffer);
        cache->storeItem(key, buffer, 500);
        pthread_mutex_lock(&mutex);
        get_per_sec++;
        pthread_mutex_unlock(&mutex);

        nanosleep(&sleeptime, &rem);
    }

    return NULL;
}

void *stat(void *arg) {
    unsigned sec = 1, gps;

    while (1) {
        sleep(1);

        pthread_mutex_lock(&mutex);
        gps = get_per_sec;
        pthread_mutex_unlock(&mutex);

        cout << "get per sec: " << gps / sec << endl;
        sec++;
    }

    return NULL;
}
