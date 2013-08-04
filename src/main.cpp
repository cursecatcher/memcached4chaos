#include "datacache.hpp"
#include "threadpool.hpp"

#include <iostream>
#include <cstdio>

#define AVG_LATENCY_TIME 100 //ms
#define MAX_KEY_SIZE 250
#define MAX_VALUE_SIZE 1024 * 1024


typedef struct {
    DataCache *cache;
    threadpool *thpool;
    int index; // 0 writer, reader otherwise

    char keybuffer[MAX_KEY_SIZE];
    void *valuebuffer;
    int32_t val_len;

    pthread_t tid;
    pthread_mutex_t mutex;
    pthread_cond_t cond; //wait for reply from server

    pthread_mutex_t *cout_mutex;
} client_buffer;

void *client_routine(void *arg);
void reader_routine(void *arg);
void writer_routine(void *arg);

int main(int argc, char *argv[]) {
    DataCache *cache;
    threadpool *thpool;
    pthread_mutex_t cout_mutex = PTHREAD_MUTEX_INITIALIZER;
    client_buffer *clients;
    int nworkers, nclients;

    if (argc != 3) {
        std::cerr << "Usage: " << argv[0] << " 'nworkers' 'nreaderclients'" << std::endl;
        return -1;
    }

    if (sscanf(argv[1], "%d", &nworkers) != 1 || sscanf(argv[2], "%d", &nclients) != 1) {
        std::cerr << "argv[1] and argv[2] must be integers" << std::endl;
        return -2;
    }

    cache = new DataCache();
    thpool = new threadpool(nworkers);

    clients = new client_buffer[nclients+1];

    /* clients[0] = writer
     * clients[1...nclients-1] = reader */
    for (int i = 0; i <= nclients; i++) {
        pthread_mutex_init(&clients[i].mutex, NULL);
        pthread_cond_init(&clients[i].cond, NULL);
        clients[i].valuebuffer = malloc(MAX_VALUE_SIZE);
        clients[i].cache = cache;
        clients[i].thpool = thpool;
        clients[i].index = i;
        clients[i].cout_mutex = &cout_mutex;
        pthread_create(&clients[i].tid, NULL, client_routine, &clients[i]);
    }

    pthread_join(clients[1].tid, NULL);

    return 0;
}

void reader_routine(void *arg) {
    client_buffer *buffer = (client_buffer *) arg;
    bool op_ok;

    op_ok = buffer->cache->get_item(buffer->keybuffer, buffer->val_len, &buffer->valuebuffer);
    pthread_cond_signal(&buffer->cond);

    pthread_mutex_lock(buffer->cout_mutex);
    std::cout << (op_ok ? "get done successfully" : "get PHAILLLL") << std::endl;
    pthread_mutex_unlock(buffer->cout_mutex);
}

void writer_routine(void *arg) {
    client_buffer *buffer = (client_buffer *) arg;
    bool op_ok;

    op_ok = buffer->cache->store_item(buffer->keybuffer, buffer->valuebuffer, buffer->val_len);
    pthread_cond_signal(&buffer->cond);

    pthread_mutex_lock(buffer->cout_mutex);
    std::cout << (op_ok ? "store done successfully" : "store PHAILLLL") << std::endl;
    pthread_mutex_unlock(buffer->cout_mutex);
}

void *client_routine(void *arg) { //passare selettore di qualche tipo, tgz ne so porcomondo
    client_buffer *buffer = (client_buffer *) arg;
    void (*client_task)(void*);

    client_task = buffer->index == 0 ? writer_routine : reader_routine;

    while (true) {
        /* genera richiesta
         * scrive richiesta sul buffer */
//        usleep(AVG_LATENCY_TIME / 2);

//        usleep(AVG_LATENCY_TIME); //latency
        buffer->thpool->add_task(client_task, arg);
        pthread_cond_wait(&buffer->cond, &buffer->mutex);
//        usleep(AVG_LATENCY_TIME); //latency

//        usleep(2 * AVG_LATENCY_TIME);
    }

    return NULL;
}
