#include <iostream>
#include "../common/sServer.h"
#include "../common/bClient.h"

using namespace std;

int main(int argc, char** argv)
{
    if(argc<3){
        cout << "Usage: "<< argv[0] << " server_port balancer_ip\n(*) port number must be 1101 or higher" << endl;
        exit(1);
    }
    int serverPort=atoi(argv[1]);
    if(serverPort<1101){
        cout << "Usage: "<< argv[0] << " server_port balancer_ip\n(*) port number must be 1101 or higher" << endl;
        exit(1);
    }
    cout << "sServer running on port " << serverPort << endl;

    sServer server(serverPort, argv[2]);
//    bClient balancer(argv[2], SERVER);
//    balancer.Send("bClient connection test");
    server.Run();

    return 0;
}
