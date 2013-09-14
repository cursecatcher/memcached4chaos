#include <stdint.h>
#include <cassert>
#include <cstddef>
#include <cstring>
#include <fstream>
#include <iostream>


struct stat {
    int nwriters, nreaders;
    int32_t num_opt;
    int32_t store_ok;
    int32_t store_failed;
    int32_t cache_success;
    int32_t cache_miss;
    /* tempi */
};


void add_stats(struct stat *tot_stats, struct stat stats) {
    tot_stats->nreaders = stats.nreaders;
    tot_stats->nwriters = stats.nwriters;

    tot_stats->num_opt += stats.num_opt;
    tot_stats->store_ok += stats.store_ok;
    tot_stats->store_failed += stats.store_failed;
    tot_stats->cache_success += stats.cache_success;
    tot_stats->cache_miss += stats.cache_miss;
}

void avg_stats(struct stat *tot_stats, int den) {
    tot_stats->num_opt /= den;
    tot_stats->store_ok /= den;
    tot_stats->store_failed /= den;
    tot_stats->cache_success /= den;
    tot_stats->cache_miss /= den;
}


/* argv[1]: file di output
 * argv[2...argc-1]: file di input */
int main(int argc, char *argv[]) {
    assert(argc > 2);
    int nfi = argc - 2;
    std::ofstream fo;
    std::ifstream fi[nfi];

    fo.open(argv[1], std::fstream::trunc);

    for(int i = 2; i < nfi; i++)
        fi[i-2].open(argv[i], std::fstream::binary);

    struct stat istat;
    struct stat ostat;

    fo << "nw\tnr\tnopt\n";

    while (fi[0].good()) {
        memset((void *) &ostat, 0, sizeof(struct stat));
        // legge blocco dai file di input
        for (int i = 0; i < nfi; i++) {
            fi[i].read((char *) &istat, sizeof(struct stat));
            add_stats(&ostat, istat);
        }
        // medie
        avg_stats(&ostat, nfi);
        // scrive sul file di output
        fo << ostat.nwriters << " " << ostat.nreaders << " " << ostat.num_opt << "\n";
//        fo << "nstore = " << ostat.store_ok + ostat.store_failed << "\n";
//        fo << "nfind = " << ostat.cache_miss + ostat.cache_success << "\n";
    }

    fo.close();

    for (int i = 0; i < argc - 2; i++)
        fi[i].close();

    return 0;
}
