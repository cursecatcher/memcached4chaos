class Assoc {
private:
    unsigned int hashpower; // how many powers of 2's worth of buckets we use
    unsigned int hash_items; // Number of items in the hash table.

    // Main hash table. This is where we look except during expansion.
    hash_item** primary_hashtable;

    /* Previous hash table. During expansion, we look here for keys that haven't
     * been moved over to the primary yet. */
    hash_item** old_hashtable;

    bool expanding; // Flag: Are we in the middle of expanding now?

    /* During expansion we migrate values with bucket granularity;
     * this is how far we've gotten so far.
     * Ranges from 0 .. hashsize(hashpower - 1) - 1. */
    unsigned int expand_bucket;


    void assoc_expand(struct default_engine *engine);
    hash_item** hashitem_before(struct default_engine *engine, uint32_t hash,
                                const char *key, const size_t nkey);

public:
    Assoc(Engine* engine);
    hash_item *assoc_find(struct default_engine *engine, uint32_t hash,
                          const char *key, const size_t nkey);
    int assoc_insert(struct default_engine *engine, uint32_t hash,
                     hash_item *it);
    void assoc_delete(struct default_engine *engine, uint32_t hash,
                      const char *key, const size_t nkey);
    void assoc_maintenance_thread(struct default_engine *engine);
//    int start_assoc_maintenance_thread(struct default_engine *engine);
//    void stop_assoc_maintenance_thread(struct default_engine *engine);


};
