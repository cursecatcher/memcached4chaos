#ifndef SSERVER_H
#define SSERVER_H

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
#include "common.h"
#include "bClient.h"
#include <msgpack.hpp>
#include <fstream>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>

using namespace std;

class sServer
{

private:
    //Variables
    int host_port; // my port
    struct sockaddr_in my_addr;
    socklen_t addr_size;
    int* csock;
    sockaddr_in sadr;
    pthread_t bClientThread_id;
    pthread_t thread_id;
    ThreadArgs clientArgs;
    int hsock;
    int * p_int ;
    int err;
    bClient *balancer;
    char* balancerIp;
    list<string> clientList;
    list<Status> setterList;
    list<Status> getterList;
    //status variables
    long bytesReceived;
    long bytesPrevious;
    long cycles;
    Status status;
    pthread_mutex_t lock;
    pthread_mutex_t setterListLock;
    pthread_mutex_t getterListLock;
    pthread_mutex_t clientListLock;
    pthread_mutex_t statusLock;
    pthread_mutex_t bwLock;
    pthread_mutex_t bufferLock;
    int i;
    pthread_mutex_t settersOnlineLock;
    map<string, int> settersOnlineMap; //sets int field to
    BufferMap buffer;
    ofstream logFile;
    time_t log_timestamp;


public:
    //Variables

    //Functions
    sServer(int serverPort, char* balIp);
    virtual ~sServer();
    void Run();
    void Stop();
    void printClientList();
    list<string> getClientList();

    void setStatus(int bytes);
    void sendStatusUpdate();
    void setClientList(char*);
    void refreshBandwidth();
    void closeClientSocket(int socket);
    // bClient thread handler
    static void* bClientHandler(void* arg)
    {
        sServer * srv = reinterpret_cast<sServer * >(arg);
        bClient bal(srv->balancerIp, SERVER, srv->host_port, "", "");
        srv->balancer= &bal;
        srv->balancer->Send();
        int balancerReq;
        int bytecount=0;
        while(1)
        {
            // wait for BALANCER messages
            if( (bytecount=recv(srv->balancer->getHsock(), &balancerReq, sizeof(int),0)) <=0)   // try to send greeting message
            {
                fprintf(stderr, "sServer: Error %d receiving Balancer data \n", errno);
                exit(1);
            }
            switch(balancerReq)
            {
            case STATUS_REQ:  // if a statuRequestUpdate arrives, get status and send it back to BALANCER
                printf("Incoming Status Update Request!\n");
                srv->sendStatusUpdate();
                // wait for BALANCER messages
                break;
            default:
                printf("Invalid message from BALANCER\n");
                srv->balancer->closeHsock();
                return 0;
                break;
            }
        }
        srv->balancer->closeHsock();
        return 0;
    }

    // Server Main Thread
    static void* SocketHandler(void* arg)
    {
        //sServer * srv = reinterpret_cast<sServer * >(arg);
        ThreadArgs *myArgs = reinterpret_cast<ThreadArgs *>(arg);
        sServer * srv = reinterpret_cast<sServer *>(myArgs->server);
        //int *threadCsock = srv->csockSafeCopy; //old: referred to external common csockSafeCopy, curruptable
        int threadCsock = myArgs->socket;
        int bytecount;
        Status sClientInfo;
        //char * msg;
        Message msg;
        int msgSize;
        Timer timer;
        time_t timestamp=time(NULL);;
        printf("#Debug: %s : Server thread handler launched\n", asctime( localtime(&timestamp)));
        // Receive Greeting Message
//        if((bytecount = recv(threadCsock, &sClientInfo, sizeof(Status), MSG_NOSIGNAL))<=0){
        if((bytecount = recv(threadCsock, &sClientInfo, sizeof(Status), MSG_NOSIGNAL))< sizeof(Status))
        {
            fprintf(stderr, "!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n");
            fprintf(stderr, "Error %d receiving Greeting Message\n", errno);
            fprintf(stderr, "Received %d of %d\n", bytecount, sizeof(Status));
            fprintf(stderr, "!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n");
            srv->closeClientSocket(threadCsock);
            //free(threadCsock);
            return 0;
        }

        printf("#Debug: %s Greeting Message received\n", sClientInfo.name);
        //msgSize=sizeof(char)*sClientInfo.avgLoad;
        msgSize=sizeof(msg);
        //msg = (char*)malloc(sizeof(char)*sClientInfo.avgLoad +1);
        printf("#Debug: msg size initialised to %d Bytes\n", msgSize);
        int ack=ACK;
        if((bytecount = send(threadCsock, &ack, sizeof(int), 0)) <=0)
        {
            fprintf(stderr, "Error %d sending Greeting Message ACK\n", errno);
            srv->closeClientSocket(threadCsock);
            //free(threadCsock);
            return 0;
        }
        // sServer Main Loop
        printf("#Debug: %s ACK sent\n", sClientInfo.name);
        if(sClientInfo.type==SETTER)
        {

            uint32_t delay;
            pthread_mutex_lock(&(srv->setterListLock));
            srv->setterList.push_back(sClientInfo);
            pthread_mutex_unlock(&(srv->setterListLock));

            string setterName(sClientInfo.name);
            pthread_mutex_lock(&(srv->bufferLock));
            srv->buffer.insert(BufferMap::value_type(setterName, MessageVector(BUF_SIZE))); //create a new vector instance
            pthread_mutex_unlock(&(srv->bufferLock));
            int i=0;
            int signalFirstAccess=1;
            while(1)
            {
                // Receive Messages from sSetter
                bytecount = recv(threadCsock, &msg, msgSize, MSG_NOSIGNAL);
                pthread_mutex_lock(&(srv->bufferLock));
                srv->buffer[setterName].at(i%BUF_SIZE)=msg; // store message inside the buffer to send it
                i++;
                pthread_mutex_unlock(&(srv->bufferLock));
                if(signalFirstAccess)  // uses flag to avoid locking, setting and unlocking after the first main loop iteration
                {
                    pthread_mutex_lock(&(srv->settersOnlineLock));
                    srv->settersOnlineMap[sClientInfo.name]=1;
                    pthread_mutex_unlock(&(srv->settersOnlineLock));
                    signalFirstAccess=0;
                }
                if(bytecount <= 0 )
                {
                    fprintf(stderr, "sServer: Error %d receiving Setter data\n", errno);
                    srv->closeClientSocket(threadCsock);
                    return 0;
                }

                srv->setStatus( msgSize ); //lock inside function
                timestamp= time(NULL);
                //delay = timestamp - msg.timestamp;
                delay = timer.usecs() - msg.usecs;
                printf("< %s : %d [%d B] - %d us\n", sClientInfo.name, /*srv->host_port,*/ msg.seq, msgSize , delay); //display received message

            }//main loop
        }
        else if(sClientInfo.type==GETTER)
        {
            // find out if required SETTER is connected to this server
            // in case of migration GETTERS can arrive before their SETTERS, so they have to wait NUM_TRIES times
            list<Status>::iterator findIter;
            int setterFound=0;
            while(setterFound==0)
            {
                //printf("#Debug: Getter %s arrived before its setter\n", sClientInfo.name);
                /*pthread_mutex_lock(&(srv->lock));
                  for (findIter = srv->setterList.begin(); findIter != srv->setterList.end(); findIter++){
                    if( strcmp(findIter->name , sClientInfo.extName)==0 ) setterFound=1;
                  }
                pthread_mutex_unlock(&(srv->lock));
                */
                //pthread_mutex_lock(&(srv->settersOnlineLock));
                if(srv->settersOnlineMap[sClientInfo.extName]==1) setterFound=1;
                //pthread_mutex_unlock(&(srv->settersOnlineLock));
                usleep(800);
                printf("#Debug: %s waiting for setter\n", sClientInfo.name);
            }
            printf("#Debug: setter for %s Found!\n", sClientInfo.name);

            pthread_mutex_lock(&(srv->getterListLock));
            srv->getterList.push_back(sClientInfo);
            pthread_mutex_unlock(&(srv->getterListLock));

            Message outMsg;
            int j=0;
            string setterName(sClientInfo.extName);
            //pthread_mutex_lock(&(srv->lock));
            /* if(srv->i<(BUF_SIZE)) {
                     j=0;
                 }
                else {
                    j= ((srv->i)+1)%BUF_SIZE;
                    }  */
            //pthread_mutex_unlock(&(srv->lock));

            //sleep(2); // substitute with a READERS-WRITERS buffer+semaphore+lock system
            while(1)
            {
                // Send Messages to sGetter
                pthread_mutex_lock(&(srv->bufferLock));
                outMsg = srv->buffer[setterName][j];
                if(outMsg.seq<1)
                {
                    pthread_mutex_unlock(&(srv->bufferLock));    //if we read a corrupted message (maybe went ahead of writer)
                    usleep(800);
                    continue;
                }
                else
                {
                    j=(++j % BUF_SIZE);
                    pthread_mutex_unlock(&(srv->bufferLock));
                }

                timestamp= time(NULL);
                outMsg.timestamp= time(NULL); // set send timestamp
                outMsg.usecs=timer.usecs(); // set send usecs
                bytecount = send(threadCsock, &outMsg, msgSize, MSG_NOSIGNAL);
                if(bytecount <= 0 )
                {
                    fprintf(stderr, "Error %d sending data to Getter\n", errno);
                    srv->closeClientSocket(threadCsock);
                    return 0;
                }

                srv->setStatus( bytecount ); // locked inside function

                printf("> %s : %d [%d B] - %s", sClientInfo.name, outMsg.seq, bytecount , asctime( localtime(&timestamp))); //display received message
                sleep(1); // temporary fixed at 1 sec
            }//main loop
        }
    }

    // Bandwidth Calculator
    static void* BandwidthCalculator(void* arg)
    {
        sServer * srv = reinterpret_cast<sServer * >(arg);
        while(1)
        {
            srv->refreshBandwidth(); //locked inside function
            printf("==== AvgLoad: %.1f || Bandwidth: %d ====\n", srv->status.avgLoad, srv->status.bandwidth);
            sleep(1);
        }
        return 0;
    }


};


#endif
