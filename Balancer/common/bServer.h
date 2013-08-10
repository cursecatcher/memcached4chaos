#ifndef BSERVER_H
#define BSERVER_H

#include <fcntl.h>
#include <string.h>
#include <string>
#include <stdlib.h>
#include <errno.h>
#include <stdio.h>
#include <netinet/in.h>
#include <resolv.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <pthread.h>
#include <signal.h>
#include <list>
#include <vector>
#include "common.h"
#include <sys/time.h>
#include <time.h>
#include <unistd.h>

using namespace std;

class bServer {

private:
    int host_port; // my port
    struct sockaddr_in my_addr;
    socklen_t addr_size;
    int* csock;
    sockaddr_in sadr;
    pthread_t thread_id;
    pthread_mutex_t lock;
    int hsock;
    int * p_int ;
    int err;
    list<struct bClientStatus> serverList;// BALANCER uses list to sort them efficiently
    list<struct bClientStatus> getterList;
    list<struct bClientStatus> setterList;
    vector<struct fullIpAddress> balancedList; //array of struct fullIpAddress dinamically allocated, more efficient for CLIENT-side direct access to elements
    int balancedListSize;

public:
    bServer();
    virtual ~bServer();
    // Variables
    list<struct bClientStatus> getSetterList();
    list<struct bClientStatus> getGetterList();
    list<struct bClientStatus> getServerList();
    // Core Functions
    void Run();
    void Stop();
    void BalanceServerList(int cause); //the cause of a list rebalancing is a new connection from a SERVER, SETTER or GETTER (int flag)
    void sendBalancedList();
    void getStatusUpdate();
    bool checkForReconnection(char *ip, int port, int socket);
    void communicateReconnection(int clientSocket, long int timestamp); // informs the client conected on clientSocket that its balancedList[0] SERVER is back ONLINE sending a timestamp that indicates the time of migration back on it
   // UI Functions
    void printSetterList();
    void printGetterList();
    void printServerList();
    void printList(bClientStatus& cli);
    void printBalancedList();
    void sendSetterDataAddress(string setterName, int getterSocket);
    //Thread Handlers
    static void* pollServerStatus(void* arg){
        bServer * bal = reinterpret_cast<bServer * >(arg);
        while(1){
            bal->getStatusUpdate();
            sleep(2);
        }
    }

    static void* UserCommandListener(void* arg){
        bServer * bal = reinterpret_cast<bServer * >(arg);
        char cmd = '_';
        list<struct bClientStatus>::iterator iterList;

        while(cmd !='q'){
            cmd=getchar();
            switch(cmd){
                case 's':
                    bal->getStatusUpdate();
                    bal->BalanceServerList(NULL);
                    bal->printServerList();
                break;

                case 'r':
                    bal->getStatusUpdate();
                    bal->BalanceServerList(NULL);
                    bal->printServerList();
                    bal->printSetterList();
                    bal->printGetterList();
                    for ( iterList = bal->setterList.begin(); iterList != bal->setterList.end(); iterList++){
                        bal->printList(*iterList);
                    }
                break;
                default:
                break;
            }

        }
    }
};


#endif
