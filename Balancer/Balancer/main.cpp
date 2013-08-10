#include <iostream>
#include "../common/bServer.h"

using namespace std;

int main()
{
    cout << "bServer running" << endl;
    bServer server;
    server.Run();

    return 0;
}
