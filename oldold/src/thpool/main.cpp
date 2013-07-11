#include <cstdio>
#include <iostream>
#include "thpool.hpp"

using namespace std;

/*
 * 1. inserire 100_000 elementi nella worker queue
 * 2. rilasciare i worker threads
 * 3. misurare le prestazioni */

int main(int argc, char *argv[]) {
    thpool *tp;
    int nthreads;

//    sscanf(argv[1], "%d", &nthreads);
    stringstream(new string(argv[1])) >> nthreads;

    cout << nthreads << " worker threads" << endl;

    tp = new thpool(nthreads);
    tp->wait_end();


    return 0;
}
