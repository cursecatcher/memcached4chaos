#include <iostream>
#include <pthread.h>
#include "../common/sSetter.h"
#include "../common/bClient.h"

using namespace std;

int main(int argc, char** argv)
{
    cout << "argc: " << argc << endl;
    sSetter mySetter(argc, argv);
    //mySClient.Start();
    return 0;
}
