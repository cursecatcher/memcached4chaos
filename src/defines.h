#include <cstddef>
#include <stdint.h>

/* assoc */
typedef unsigned long   ub4; /* unsigned 4-byte quantities */
typedef unsigned char   ub1; /* unsigned 1-byte quantities */



#define MAX_NUMBER_OF_SLAB_CLASSES (POWER_LARGEST+1)
#define DEFAULT_SLAB_BULK_CHECK 1
#define DEFAULT_HASH_BULK_MOVE 8

#define LARGEST_ID POWER_LARGEST
#define POWER_LARGEST 200
#define POWER_SMALLEST 1

#define HASHPOWER_DEFAULT 16
#define CHUNK_ALIGN_BYTES 8

#define TAIL_REPAIR_TIME (3*3600)
#define ITEM_UPDATE_INTERVAL 60

#define ITEM_LINKED 1
#define ITEM_CAS 2
#define ITEM_SLABBED 4
#define ITEM_FETCHED 8

#define hashsize(n) ((ub4)1<<(n))
#define hashmask(n) (hashsize(n)-1)

/* warning: don't use these macros with a function, as it evals its arg twice */
#define ITEM_get_cas(i) (((i)->it_flags & ITEM_CAS) ? \
        (i)->data->cas : (uint64_t)0)

#define ITEM_set_cas(i,v) { \
    if ((i)->it_flags & ITEM_CAS) { \
        (i)->data->cas = v; \
    } \
}

#define ITEM_key(item)  (((char*)&((item)->data)) \
         + (((item)->it_flags & ITEM_CAS) ? sizeof(uint64_t) : 0))

#define ITEM_suffix(item)   ((char*) &((item)->data) + (item)->nkey + 1 \
         + (((item)->it_flags & ITEM_CAS) ? sizeof(uint64_t) : 0))

#define ITEM_data(item) ((char*) &((item)->data) + (item)->nkey + 1 \
         + (item)->nsuffix \
         + (((item)->it_flags & ITEM_CAS) ? sizeof(uint64_t) : 0))

#define ITEM_ntotal(item)   (sizeof(struct _stritem) + (item)->nkey + 1 \
         + (item)->nsuffix + (item)->nbytes \
         + (((item)->it_flags & ITEM_CAS) ? sizeof(uint64_t) : 0))

typedef unsigned int rel_time_t;

typedef struct _stritem {
    struct _stritem *next;
    struct _stritem *prev;
    struct _stritem *h_next;    /* hash chain next */
    rel_time_t      time;       /* least recent access */
    rel_time_t      exptime;    /* expire time */
    int             nbytes;     /* size of data */
    unsigned short  refcount;
    uint8_t         nsuffix;    /* length of flags-and-length string */
    uint8_t         it_flags;   /* ITEM_* above */
    uint8_t         slabs_clsid;/* which slab class we're in */
    uint8_t         nkey;       /* key length, w/terminating null and padding */
    /* this odd type prevents type-punning issues when we do
     * the little shuffle to save space when not using CAS. */
    union {
        uint64_t cas;
        char end;
    } data[];
    /* if it_flags & ITEM_CAS we have 8 bytes CAS */
    /* then null-terminated key */
    /* then " flags length\r\n" (no terminating null) */
    /* then data with terminating \r\n (no terminating null; it's binary!) */
} item;


struct settings {
    size_t maxbytes;
    int maxconns;
//    int port;
//    int udpport;
    char *inter;
    int verbose;
    rel_time_t oldest_live; /* ignore existing items older than this */
    int evict_to_free;
//    char *socketpath;   /* path to unix socket if using local socket */
//    int access;  /* access mask (a la chmod) for unix domain socket */
    double factor;          /* chunk size growth factor */
    int chunk_size;
    int num_threads;        /* number of worker (without dispatcher) libevent threads to run */
//    int num_threads_per_udp; /* number of worker threads serving each udp socket */
//    char prefix_delimiter;  /* character that marks a key prefix (for stats) */
//    int detail_enabled;     /* nonzero if we're collecting detailed stats */
//    int reqs_per_event;     /* Maximum number of io to process on each io-event. */
    bool use_cas;
//    enum protocol binding_protocol;
//    int backlog;
    int item_size_max;        /* Maximum item size, and upper end for slabs */
//    bool sasl;              /* SASL on/off */
//    bool maxconns_fast;     /* Whether or not to early close connections */
    bool slab_reassign;     /* Whether or not slab reassignment is allowed */
    int slab_automove;     /* Whether or not to automatically move slabs */
    int hashpower_init;     /* Starting hash power level */
//    bool shutdown_command; /* allow shutdown command */
};

/*
extern volatile rel_time_t current_time;

//extern pthread_mutex_t cache_lock;
extern struct settings settings;
*/
