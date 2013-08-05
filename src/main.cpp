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
    uint64_t *nreq;
} client_buffer;

struct sleep_thr {
    pthread_mutex_t *cout_mutex;
    threadpool *thpool;

    int ttl;
};

void *client_routine(void *arg);
void reader_routine(void *arg);
void writer_routine(void *arg);

void *sleeper(void* arg) {
    struct sleep_thr *data = (struct sleep_thr*) arg;

    sleep(data->ttl);
    pthread_mutex_lock(data->cout_mutex); // stop alle telefonate!

    return NULL;
}

int main(int argc, char *argv[]) {
    DataCache *cache;
    threadpool *thpool;
    pthread_mutex_t cout_mutex = PTHREAD_MUTEX_INITIALIZER;
    client_buffer *clients;
    struct sleep_thr data;
    int nworkers, nclients;
    uint64_t nreq = 0;

    if (argc != 4) {
        std::cerr << "Usage: " << argv[0] << " 'nworkers' 'nreaderclients' 'timetolive'" << std::endl;
        return -1;
    }

    if (sscanf(argv[1], "%d", &nworkers) != 1 ||
        sscanf(argv[2], "%d", &nclients) != 1 ||
        sscanf(argv[3], "%d", &data.ttl) != 1) {
        std::cerr << "args must be integers" << std::endl;
        return -2;
    }

    cache = new DataCache();
    thpool = new threadpool(nworkers);
    clients = new client_buffer[nclients+1];

    /* clients[1] = writer
     * clients[2...nclients-2] = reader */
    for (int i = 0; i <= nclients; i++) {
        pthread_mutex_init(&clients[i].mutex, NULL);
        pthread_cond_init(&clients[i].cond, NULL);
        clients[i].valuebuffer = malloc(MAX_VALUE_SIZE);
        clients[i].cache = cache;
        clients[i].thpool = thpool;
        clients[i].index = i;
        clients[i].cout_mutex = &cout_mutex;
        clients[i].nreq = &nreq;
        pthread_create(&clients[i].tid, NULL, client_routine, &clients[i]);
    }

//    clients[0].valuebuffer = argv[3];
    pthread_t t;
    data.cout_mutex = &cout_mutex;
    data.thpool = thpool;

    pthread_create(&t, NULL, sleeper, (void*) &data);
    pthread_join(t, NULL);

    std::cout << "\nNum worker thread: " << nworkers << std::endl;
    std::cout << "Num clients: " << nclients << std::endl;
    std::cout << "Time to live: " << data.ttl << std::endl;

    return 0;
}

void reader_routine(void *arg) {
    client_buffer *buffer = (client_buffer *) arg;
    bool op_ok;

    op_ok = buffer->cache->get_item(buffer->keybuffer, buffer->val_len, &buffer->valuebuffer);
    pthread_cond_signal(&buffer->cond);

    pthread_mutex_lock(buffer->cout_mutex);
    (*buffer->nreq)++;
    std::cout << "req #" << *buffer->nreq << "...get " << (op_ok ? "done" : "failed") << std::endl;
    pthread_mutex_unlock(buffer->cout_mutex);

    usleep(AVG_LATENCY_TIME);
}

void writer_routine(void *arg) {
    client_buffer *buffer = (client_buffer *) arg;
    bool op_ok;

    op_ok = buffer->cache->store_item(buffer->keybuffer, buffer->valuebuffer, buffer->val_len);
    pthread_cond_signal(&buffer->cond);

    pthread_mutex_lock(buffer->cout_mutex);
    (*buffer->nreq)++;
    std::cout << "req #" << *buffer->nreq << "...store " << (op_ok ? "done" : "failed") << std::endl;
    pthread_mutex_unlock(buffer->cout_mutex);

    usleep(AVG_LATENCY_TIME);
}

void *client_routine(void *arg) {
    client_buffer *buffer = (client_buffer *) arg;
    void (*client_task)(void*);

    client_task = buffer->index == 0 ? writer_routine : reader_routine;

    while (true) {
        /* genera richiesta
         * scrive richiesta sul buffer */

        buffer->thpool->add_task(client_task, arg);
        pthread_cond_wait(&buffer->cond, &buffer->mutex);
    }

    return NULL;
}
