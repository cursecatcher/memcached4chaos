#ifndef __CONST_TYPES_H__
#define __CONST_TYPES_H__

#include <inttypes.h> //definition of uint*_t


#define POWER_SMALLEST 1
#define POWER_LARGEST  200
#define CHUNK_ALIGN_BYTES 8
#define DONT_PREALLOC_SLABS
#define MAX_NUMBER_OF_SLAB_CLASSES (POWER_LARGEST + 1)

typedef uint32_t rel_time_t;

/* You should not try to aquire any of the item locks before calling these
 * functions. */
typedef struct _hash_item {
    struct _hash_item *next; // pointer to next item in lru
    struct _hash_item *prev; // pointer to previous item in lru
    struct _hash_item *h_next; // hash chain next
    rel_time_t time;  // least recent access

    uint32_t nbytes; // < The total size of the data (in bytes)
    uint32_t flags; // Flags associated with the item (in network byte order)
    uint16_t nkey; // The total length of the key (in bytes)
    uint16_t iflag; /**< Intermal flags. lower 8 bit is reserved for the core
                     * server, the upper 8 bits is reserved for engine
                     * implementation. */
    unsigned short refcount;
    uint8_t slabs_clsid; // which slab class we're in
} hash_item;


typedef enum {
    ENGINE_SUCCESS     = 0x00, /**< The command executed successfully */
    ENGINE_KEY_ENOENT  = 0x01, /**< The key does not exists */
    ENGINE_KEY_EEXISTS = 0x02, /**< The key already exists */
    ENGINE_ENOMEM      = 0x03, /**< Could not allocate memory */
    ENGINE_NOT_STORED  = 0x04, /**< The item was not stored */
    ENGINE_EINVAL      = 0x05, /**< Invalid arguments */
    ENGINE_ENOTSUP     = 0x06, /**< The engine does not support this */
    ENGINE_EWOULDBLOCK = 0x07, /**< This would cause the engine to block */
    ENGINE_E2BIG       = 0x08, /**< The data is too big for the engine */
    ENGINE_WANT_MORE   = 0x09, /**< The engine want more data if the frontend
                                * have more data available. */
    ENGINE_DISCONNECT  = 0x0a, /**< Tell the server to disconnect this client */
    ENGINE_EACCESS     = 0x0b, /**< Access control violations */
    ENGINE_NOT_MY_VBUCKET = 0x0c, /** < This vbucket doesn't belong to me */
    ENGINE_TMPFAIL     = 0x0d, /**< Temporary failure, please try again later */
    ENGINE_ERANGE      = 0x0e, /**< Value outside legal range */
    ENGINE_FAILED      = 0xff  /**< Generic failue. */
} ENGINE_ERROR_CODE;

typedef enum {
    OPERATION_ADD = 1, /**< Store with add semantics */
    OPERATION_SET,     /**< Store with set semantics */
    OPERATION_REPLACE, /**< Store with replace semantics */
    OPERATION_APPEND,  /**< Store with append semantics */
    OPERATION_PREPEND, /**< Store with prepend semantics */
    OPERATION_CAS      /**< Store with set semantics. */
} ENGINE_STORE_OPERATION;
#endif
