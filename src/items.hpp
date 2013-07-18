

/* You should not try to aquire any of the item locks before calling these
 * functions. */
typedef struct _hash_item {
    struct _hash_item *next;
    struct _hash_item *prev;
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


class Items {
private:
    Engine* engine;

    hash_item *heads[POWER_LARGEST];
    hash_item *tails[POWER_LARGEST];
    unsigned int sizes[POWER_LARGEST];


    void item_link_q(hash_item *it);
    void item_unlink_q(hash_item *it);
    hash_item *do_item_alloc(const void *key, const size_t nkey,
                              const int flags,/*, const rel_time_t exptime,*/
                              const int nbytes);
    hash_item *do_item_get(const char *key, const size_t nkey);
    int do_item_link(hash_item *it);
    void do_item_unlink(hash_item *it);
    void do_item_release(hash_item *it);
    void do_item_update(hash_item *it);
    int do_item_replace(hash_item *it, hash_item *new_it);
    void item_free(hash_item *it);

public:
    Items(Engine *engine);

    /** Allocate and initialize a new item structure
     * @param engine handle to the storage engine
     * @param key the key for the new item
     * @param nkey the number of bytes in the key
     * @param flags the flags in the new item
     * @param exptime when the object should expire ***REMOVED***
     * @param nbytes the number of bytes in the body for the item
     * @return a pointer to an item on success NULL otherwise */
    hash_item *item_alloc(const void *key, size_t nkey,
                           int flags,/*rel_time_t exptime, */
                           int nbytes);

    /** Get an item from the cache
     *
     * @param engine handle to the storage engine
     * @param key the key for the item to get
     * @param nkey the number of bytes in the key
     * @return pointer to the item if it exists or NULL otherwise */
    hash_item *item_get(const void *key, const size_t nkey);

    /** Release our reference to the current item
     * @param engine handle to the storage engine
     * @param it the item to release */
    void item_release(hash_item *it);

    /** Unlink the item from the hash table (make it inaccessible)
     * @param engine handle to the storage engine
     * @param it the item to unlink */
    void item_unlink(hash_item *it);

    /** Store an item in the cache
     * @param engine handle to the storage engine
     * @param item the item to store
     * @param cas the cas value (OUT)
     * @param operation what kind of store operation is this (ADD/SET etc)
     * @return ENGINE_SUCCESS on success
     *
     * @todo should we refactor this into hash_item ** and remove the cas
     *       there so that we can get it from the item instead? */
    ENGINE_ERROR_CODE store_item(hash_item *it,
                                 uint64_t cas,
                                 ENGINE_STORE_OPERATION operation,
                                 const void *cookie);
};
