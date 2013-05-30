#include <pthread.h>

class mutex {
public:
    mutex();
    int lock();
    int lock_trylock();
    int trylock();
    int unlock();
    int cond_wait(pthread_cond_t *cond);

private:
    pthread_mutex_t *l;
};
