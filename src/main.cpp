#include <iostream>
#include <pthread.h>
#include <queue>

#include "datacache.hpp"

using namespace std;

enum opt_t { GET_BY_KEY, STORE_KV, DELETE_BY_KEY };

typedef struct {
    string key;
    string value;
    enum opt_t op;
} req_t;

typedef struct {
    queue<req_t> req_queue;
    pthread_mutex_t queue_lock;
    pthread_mutex_t cond_mutex;
    pthread_cond_t cond;
} datathread_t;

void *server(void *arg);
void *reader(void *arg);
void *writer(void *arg);


int main() {
    datathread_t data;

    pthread_mutex_init(&data.queue_lock, NULL);
    pthread_mutex_init(&data.cond_mutex, NULL);
    pthread_cond_init(&data.cond, NULL);
}

void *server(void *arg) {
    datathread_t *data; (datathread_t*) arg;
    req_t temp;

    while (true) {
        pthread_mutex_lock(&data->queue_lock);
        if (data->req_queue.empty()) {
            pthread_mutex_unlock(&data->queue_lock);
            pthread_cond_wait(&data->cond, &data->cond_mutex);
            pthread_cond_lock(&data->queue_lock);
        }
        temp = data->req_queue.front(); /* preleva elemento */
        data->req_queue.pop();
        pthread_mutex_unlock(&data->queue_lock);

        /* elabora temp */
        /* latenza */
    }
}

void *reader(void *arg) {
    datathread_t *data = (datathread_t*) arg;

    while (true) {
        /* crea richiesta : k */

        pthread_mutex_lock(&data->queue_lock);
        data->req_queue.push_back(/*push richiesta*/);
        pthread_cond_signal(&data->cond_mutex);
        pthread_mutex_unlock(&data->queue_lock);

        /* latenza : sleep */

        /* attendi risposta da server */
    }

    return NULL;
}

void *writer(void *arg) {
    datathread_t *data = (datathread_t*) arg;

    while (true) {
        /* crea richiesta : k, v */

        pthread_mutex_lock(&data->queue_lock);
        data->req_queue.push_back(/*push richiesta*/);
        pthread_cond_signal(&data->cond_mutex);
        pthread_mutex_unlock(&data->queue_lock);

        /* latenza : sleep */

        /* attendi conferma store */
    }

    return NULL;
}
