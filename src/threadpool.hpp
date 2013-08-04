#ifndef THREADPOOL_HPP
#define THREADPOOL_HPP

#include <pthread.h>

#include <cassert>
#include <cstdlib>
#include <iostream>
#include <queue>

#define QUEUE_SIZE 10000


typedef unsigned int uint;
typedef struct threadpool_task* task;

struct threadpool_task{
    void (*routine)(void*);
    void *data;
};


class threadpool {
private:
    struct threadpool_task tasks[QUEUE_SIZE];

    std::queue<task> free_tasks_queue;
    std::queue<task> tasks_queue;

    pthread_t *tids;
    uint nthreads;
    bool stop_flag;

    // the access to free_tasks_queue is protected by this lock
    pthread_mutex_t free_tasks_mutex;
	pthread_cond_t free_tasks_cond;
    // the access to tasks_queue is protected by this lock
	pthread_mutex_t mutex;
	pthread_cond_t cond;


    threadpool_task *get_task_to_do();

    inline void init_task(threadpool_task *t) {
        t->routine = NULL;
        t->data = NULL;
    }
    inline void lock_ftq() {
        pthread_mutex_lock(&this->free_tasks_mutex);
    }
    inline void unlock_ftq() {
        pthread_mutex_unlock(&this->free_tasks_mutex);
    }
    inline void lock_tq() {
        pthread_mutex_lock(&this->mutex);
    }
    inline void unlock_tq() {
        pthread_mutex_unlock(&this->mutex);
    }

public:
    /**
     * This method creates a newly allocated thread pool
     * @param nthreads The number of worker thread used in this pool.
     *
     * @throw ... */
    threadpool(uint nthreads);

    /**
     * This method stops all the worker threads (stop&exit) and frees
     * the allocated memory. In case blocking != 0 the caller will block
     * until all workers have exited.
     * */
    ~threadpool();


    /**
     * This method adds a routine to be executed
     *
     * @param routine The routine to be executed
     * @param data The data to be passed to the routine
     *
     * @return 0 on success
     * @return -1 on failure
     * */
    int add_task(void (*routine)(void*), void *data);

    /** Worker thread procedure */
    void *worker_routine();
};
#endif //THREADPOOL_HPP
