#include "thpool.hpp"

void* worker(void* arg) {
    thpool *t = (thpool *) arg;
    t->_worker();
    pthread_exit(NULL);
}

void* setter(void* arg) {
    thpool *t = (thpool *) arg;
    t->_setter();
    pthread_exit(NULL);
}


thpool::thpool(int nthreads) {
    if (nthreads < 1)
        nthreads = 1;

    this->nthreads = nthreads;
    this->tids = new pthread_t[nthreads+1];
//    sem_init(&this->sem_iq, 0, 0);
    pthread_mutex_init(&this->iq_lock, NULL);
    pthread_cond_init(&this->insert_cond, NULL);
    pthread_mutex_init(&this->cout_lock, NULL);

    for (int i = 1; i <= nthreads; i++)
        pthread_create(&this->tids[i], NULL, worker, this);

    pthread_create(&this->tids[0], NULL, setter, this);
}


void thpool::_worker() {
    static int v = 1;
    int id_t;
    req_t t;
    string s;

    pthread_mutex_lock(&this->iq_lock);
    id_t = v++;
    pthread_mutex_unlock(&this->iq_lock);

    while (true) {
        /*
        sem_wait(&this->sem_iq);
        pthread_mutex_lock(&this->iq_lock);
        t = this->global_iq.front();
        this->global_iq.pop();
        pthread_mutex_unlock(&this->iq_lock); */
        pthread_mutex_lock(&this->iq_lock);
        if (this->global_iq.size() == 0) {
            pthread_cond_wait(&this->insert_cond, &this->iq_lock);
        }
        else {
            t = this->global_iq.front();
            this->global_iq.pop();
        }
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
/*
        pthread_mutex_lock(&this->cout_lock);
        cout << s << endl;
        pthread_mutex_unlock(&this->cout_lock); */
        /* elaborazione s */
    }
}

void thpool::_setter() {
    int cont = 0;
    req_t temp;

    temp.key = "prova";
    temp.value = "try";
    temp.op_select = SET_REQ;

    while (cont < 20000) {
        pthread_mutex_lock(&this->iq_lock);
        this->global_iq.push(temp);
        pthread_cond_signal(&this->insert_cond);
        pthread_mutex_unlock(&this->iq_lock);
//        sem_post(&this->sem_iq);

        temp.op_select = (req_type) (((int)temp.op_select + 1) % 3);
        cont++;
        usleep(1);
    }

    _exit(0);


}

void thpool::wait_end() {
    for (int i = 0; i < this->nthreads; i++)
        pthread_join(this->tids[i], NULL);
}
