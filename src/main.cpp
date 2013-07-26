#include <iostream>
#include "datacache.hpp"

using namespace std;


int main() {
    DataCache *cache;
    void *buf;
    int bufflen;

    string keys[10];
    string values[10];

    cache = new DataCache();
    buf = malloc(64);

    keys[0] = "mem";  values[0] = "cached";
    keys[1] = "cas";  values[1] = "compare and set";
    keys[2] = "slabbing";   values[2] = "broodal";
    keys[3] = "hash";

    for (int i = 0; i < 3; i++)
        if (cache->store_item(keys[i].c_str(), (void*) values[i].c_str(), values[i].size()))
            cout << "store('" << keys[i] << "', '" << values[i] << "') OK" << endl;

    cout << endl;

    for (int i = 2; i >= 0; i--)
        if (cache->get_item(keys[i].c_str(), bufflen, &buf)) {
            char *p = (char*) buf;
            p[bufflen] = '\0';
            cout << "get('" << keys[i] << "') -> '" << p << "'" << endl;
        }
        else {
            cout << "key '" << keys[i] << "' is not present" << endl;
        }

    cout << endl;

    int i = 2;
    values[i] = "list of pointers";

    if (cache->store_item(keys[i].c_str(), (void*) values[i].c_str(), values[i].size()))
        cout << "store('" << keys[i] << "', '" << values[i] << "') OK" << endl;
    else
        cout << "smerdo" << endl;

    cout << endl;

    for (int j = 0; j < 2; j++) {
        if (cache->get_item(keys[i].c_str(), bufflen, &buf)) {
            char *p = (char*) buf;
            p[bufflen] = '\0';
            cout << "get('" << keys[i] << "') -> '" << p << "'" << endl;
        }
        else {
            cout << "key '" << keys[i] << "' is not present" << endl;
        }

        if (cache->delete_item(keys[i].c_str()))
            cout << "key " << keys[i] << " deleted" << endl;
    }

    free(buf);

    return 0;
}
