#ifndef SGETTER_H
#define SGETTER_H

#include <fcntl.h>
#include <stdlib.h>
#include <errno.h>
#include <netinet/in.h>
#include <resolv.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include "common.h"
#include "bClient.h"
#include <fstream>


class sGetter{
private:
    // socket variables
    int ack;
    int host_port;
    //char host_name[16]; //host ip
    char host_name[16]; //host ip
    char buffer[1024]; //input message from server
    int bytecount;
    int buffer_len;
    int hsock;
    int * p_int;
    int err;
    long int migrate_timestamp;
    int migrating;
    // CLIENT variables
    //char * msg;
    Message msg;
    int msgSize;
    Status sClientInfo; // greetingMsg to send sServer info about our pkgSize (stored in status.avgLoad) and bandwidth (from our updload_interval_param)
    //int type; // can be SETTER or GETTER
    int balancerSck;
    pthread_t bClientThread_id;
    bClient *balancer;
    pthread_mutex_t lock;
    pthread_mutex_t balancedListLock;
    char balancerIp[16];
    struct sockaddr_in my_addr;
    int balancedListSize;
    vector<fullIpAddress> balancedList; // will be an array of struct fullIpAddress dynamically allocated
    int srvNumber; // stores the balancedList position of the active connection
    ofstream logFile;
    Timer timer;

public:
sGetter(int , char**);
void ParseParameters(int argc, char** argv); // parse command line parameterss
void InitBalancerConnection();  // establish connection with framework to get balancedList and updates
int ConnectToServer();
void Start();
// UI functions
void PrintServerList();
void ReceiveBalancerMessage();
void GetBalancedListUpdate();
// bClient thread Handler
static void* GetBalancerMessages_Thread(void* arg){
    sGetter * cli = reinterpret_cast<sGetter * >(arg);
    while(1){
        printf("#Debug: Thread waiting for balancedList Message...\n");
        cli->ReceiveBalancerMessage();
    }
}

// Getter Functions
float getUploadFrequency();  // Hertz
float getMessagesPerSecond(); // mps
int getUploadInterval(); // milliseconds
// Setter Functions
void setMaxSendAttempts(int n);
};

#endif
