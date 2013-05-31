#include <iostream>
#include <queue>
#include <pthread.h>
#include <semaphore.h>

using namespace std;

/*
 * set(key, value)
 * get(key)
 * delete(key)*/
typedef enum{ SET_REQ = 0, GET_REQ, DELETE_REQ} req_type;

typedef struct {
    string key;
    string value;
    req_type op_select;
} req_t;

void* worker(void* arg);
void* setter(void* arg);

class thpool {
public:
    thpool(int nthreads);
    void _worker();
    void _setter();
    void wait_end();



private:
    int nthreads;
    pthread_t *tids;
    pthread_mutex_t iq_lock;
    pthread_cond_t insert_cond;
    pthread_mutex_t cout_lock;
//    sem_t sem_iq;
    queue<req_t> global_iq; /* input queue */
    queue<string> global_oq; /* output queue */
};
