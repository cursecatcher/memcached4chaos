#include <iostream>
#include <pthread.h>
#include "../common/sGetter.h"
#include "../common/bClient.h"

using namespace std;

int main(int argc, char** argv)
{
    cout << "argc: " << argc << endl;
    sGetter myGetter(argc, argv);
    //mySClient.Start();
    return 0;
}
