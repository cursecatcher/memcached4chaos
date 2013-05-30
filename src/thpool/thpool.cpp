#include "thpool.hpp"

void* worker(void* arg) {
    thpool *t = (thpool *) arg;
    t->_worker();
    pthread_exit(NULL);
}


thpool::thpool(int nthreads) {
    req_t temp;

    if (nthreads < 1)
        nthreads = 1;

    this->nthreads = nthreads;
    this->tids = new pthread_t[nthreads];
    sem_init(&this->sem_iq, 0, 4);
    pthread_mutex_init(&this->iq_lock, NULL);
    pthread_mutex_init(&this->cout_lock, NULL);

    temp.key = "ciao";
    temp.value = "mondo";
    temp.op_select = SET_REQ;
    this->global_iq.push(temp);

    temp.key = "dio";
    temp.op_select = GET_REQ;
    this->global_iq.push(temp);

    temp.key = "gesu";
    temp.op_select = DELETE_REQ;
    this->global_iq.push(temp);

    temp.key = "nico";
    temp.value = "ci";
    temp.op_select = SET_REQ;
    this->global_iq.push(temp);

    for (int i = 0; i < nthreads; i++)
        pthread_create(&this->tids[i], NULL, worker, this);

}

void thpool::_worker() {
    req_t t;
    string s;

    while (true) {
        sem_wait(&this->sem_iq);
        pthread_mutex_lock(&this->iq_lock);
        t = this->global_iq.front();
        this->global_iq.pop();
        pthread_mutex_unlock(&this->iq_lock);

        switch (t.op_select) {
            case GET_REQ:
                s = "get(" + t.key + ")";
                break;

            case SET_REQ:
                s = "set(" + t.key + ", " + t.value + ")";
                break;

            case DELETE_REQ:
                s = "delete(" + t.key + ")";
                break;

        }

        pthread_mutex_lock(&this->cout_lock);
        cout << s << endl;
        pthread_mutex_unlock(&this->cout_lock);
        /* elaborazione s */
    }
}


void thpool::wait_end() {
    for (int i = 0; i < this->nthreads; i++)
        pthread_join(this->tids[i], NULL);
}
