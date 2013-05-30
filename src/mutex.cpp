#include "mutex.hpp"


mutex::mutex() {
    pthread_mutex_init(&this->l, NULL);
}

int mutex::lock() {
    return pthread_mutex_lock(&this->l);
}

int mutex::lock_trylock() {
    while (this->trylock())
        ;
    return 0;
}

int mutex::trylock() {
    return pthread_mutex_trylock(&this->l);
}

int mutex::unlock() {
    return pthread_mutex_unlock(&this->l);
}

int mutex::cond_wait(pthread_cond_t *cond) {
    return pthread_cond_wait(cond, &this->l);
}

