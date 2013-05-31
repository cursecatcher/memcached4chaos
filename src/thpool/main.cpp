#include <cstdio>
#include "thpool.hpp"

int main(int argc, char *argv[]) {
    thpool *tp;
    int nthreads;

    sscanf(argv[1], "%d", &nthreads);

    tp = new thpool(nthreads);
    tp->wait_end();

    return 0;
}
