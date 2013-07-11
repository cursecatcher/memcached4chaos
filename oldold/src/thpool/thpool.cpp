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
    pthread_t tid;

    if (nthreads < 1)
        nthreads = 1;

    this->tids = new pthread_t[this->nthreads = nthreads];
    pthread_mutex_init(&this->iq_lock, NULL);
    pthread_cond_init(&this->insert_cond, NULL);
    pthread_mutex_init(&this->cout_lock, NULL);

    cout << "Create thread...done";
    pthread_create(&tid, NULL, setter, this);
    cout << "Init queue... ";
    pthread_join(tid, NULL);
    cout << "done" << endl;

    cout << "Create worker threads..." << endl;

    for (int i = 0; i < nthreads; i++) {
        pthread_create(&this->tids[i], NULL, worker, this);
        cout << "Worker thread #" << i+1 << "created" << endl;
    }

/*    for (int i = 0; i < nthreads; i++)
        pthread_join(this->tids[i], NULL);

    cout << "done" << endl; */

}


void thpool::_worker() {
    req_t t;
    string s;

    while (true) {
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

    while (cont < 10) {
        pthread_mutex_lock(&this->iq_lock);
        this->global_iq.push(temp);
        if (this->global_iq.size() == 1)
            pthread_cond_signal(&this->insert_cond);
        pthread_mutex_unlock(&this->iq_lock);

        temp.op_select = (req_type) (((int)temp.op_select + 1) % 3);
        cont++;
        usleep(10);
    }

    _exit(0);


}

void thpool::wait_end() {
    for (int i = 0; i < this->nthreads; i++)
        pthread_join(this->tids[i], NULL);
}
