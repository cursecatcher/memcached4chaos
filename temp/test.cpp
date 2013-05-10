#include <iostream>
#include "assoc.hpp"
#include "defines.h"

using namespace std;

int main() {
    item i;

    i.data = calloc(10, sizeof(uint64_t));

    cout << ITEM_key(i) << endl;
//    assoc_array *aa;
//    aa = new assoc_array(0);
//    cout << "It works!" << endl;

    return 0;
}
