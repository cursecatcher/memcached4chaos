#include "threadpool.hpp"

void *fun_worker_routine(void* arg) {
    return ((threadpool*) arg)->worker_routine();
}

threadpool::threadpool(uint nthreads) {
    bool init_failed = false;

    if (nthreads == 0) {
        std::cerr << "The number of thread must be greater than 0." << std::endl;
        init_failed = true;
    }
    else if (pthread_mutex_init(&this->free_tasks_mutex, NULL) || pthread_mutex_init(&this->mutex, NULL)) {
        std::cerr << "Cannot initialize mutex." << std::endl;
        init_failed = true;
    }
    else if (pthread_cond_init(&this->free_tasks_cond, NULL) || pthread_cond_init(&this->cond, NULL)) {
        std::cerr << "Cannot initialize condition variable." << std::endl;
        init_failed = true;
    }
    else if ((this->tids = (pthread_t*) calloc(nthreads, sizeof(pthread_t))) == NULL) {
        std::cerr << "Cannot allocate memory with calloc." << std::endl;
        init_failed = true;
    }

    if (init_failed)
        abort();

    this->nthreads = nthreads;

    // add all the tasks to the free queue
    for (int i = 0; i < QUEUE_SIZE; i++) {
        this->init_task(&this->tasks[i]);
        this->free_tasks_queue.push(&this->tasks[i]);
    }

    this->stop_flag = false;

    for (uint i = 0; i < nthreads; i++)
        if (pthread_create(&this->tids[i], NULL, fun_worker_routine, (void*) this)) {
            std::cerr << "Cannot create thread" << std::endl;
            abort();
        }
}

threadpool::~threadpool() {
    this->stop_flag = true;

    this->lock_tq();
    pthread_cond_broadcast(&this->cond);
    this->unlock_tq();

    for (uint i = 0; i < this->nthreads; i++)
        pthread_join(this->tids[i], NULL);

    free(this->tids);
    free(this->tasks);
}

int threadpool::add_task(void (*routine)(void*), void *data) {
    task newtask;

    /******* begin critical section: free_tasks_queue *******/
    this->lock_ftq(); // lock the resource

    // if the queue is empty, wait until the queue has a task
    while (this->free_tasks_queue.empty())
        pthread_cond_wait(&this->free_tasks_cond, &this->free_tasks_mutex);

    newtask = this->free_tasks_queue.front(); // obtain an empty task
    this->free_tasks_queue.pop(); // remove the task from queue

    this->unlock_ftq(); // unlock the resource
    /******* end critical section: free_tasks_queue *******/

    newtask->routine = routine;
    newtask->data = data;

    /******* begin critical section: tasks_queue *******/
    this->lock_tq(); // lock the resource

    this->tasks_queue.push(newtask); // add the task to the tasks queue
    // notify all worker threads that there are new jobs
    if (this->tasks_queue.size() == 1)
        pthread_cond_broadcast(&this->cond);

    this->unlock_tq(); //unlock the resource
    /******* end critical section: tasks_queue *******/

    return 0;
}

threadpool_task *threadpool::get_task_to_do() {
    task newtask = NULL;

    if (!this->stop_flag) {
        /******* begin critical section *******/
        this->lock_tq(); // lock the queue
        while (this->tasks_queue.empty() && !this->stop_flag) // wait for a task
            pthread_cond_wait(&this->cond, &this->mutex);

        if (!this->stop_flag) {
            newtask = this->tasks_queue.front();
            this->tasks_queue.pop();
        }

        this->unlock_tq(); // unlock the queue
        /******* end critical section *******/
    }

    return newtask;
}

void *threadpool::worker_routine() {
    task to_do;

    while (true) {
        to_do = this->get_task_to_do();

        if (to_do == NULL && this->stop_flag)
            break;

        //execute routine (if any)
        if (to_do->routine) {
            to_do->routine(to_do->data);

            this->init_task(to_do);

            /******* begin critical section *******/
            this->lock_ftq();

            // release the task by returning it to the free_tasks_queue
            this->free_tasks_queue.push(to_do);
            if (this->free_tasks_queue.size() == 1)
                pthread_cond_broadcast(&this->free_tasks_cond);

            this->unlock_ftq();
            /******* end critical section *******/
        }
    }

    return NULL;
}
