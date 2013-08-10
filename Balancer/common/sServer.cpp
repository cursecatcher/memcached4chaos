#include "sServer.h"
#include <string>
#include <iostream>
#include <stdlib.h>

sServer::sServer(int port, char* balIp)
{
    // Connect to BALANCER to annouce our presence
    balancerIp= balIp;
    i=0; // counter for circular buffer of messages
    pthread_create(&bClientThread_id,0,&bClientHandler, this ); //create the new Thread
    pthread_detach(bClientThread_id);


    //  Server Setup
    if(port<1101)
    {
        printf("sServer Port Error: must be >1100\n");
        exit(1);
    }
    //init socket params
    host_port= port;
    addr_size = 0;
    thread_id=0;
    hsock = socket(AF_INET, SOCK_STREAM, 0);
    if(hsock == -1)
    {
        printf("Error initializing socket %d\n", errno);
        exit(1);
    }
    p_int = (int*)malloc(sizeof(int));
    *p_int = 1;

    if( (setsockopt(hsock, SOL_SOCKET, SO_REUSEADDR, (char*)p_int, sizeof(int)) == -1 )
            || (setsockopt(hsock, SOL_SOCKET, SO_KEEPALIVE, (char*)p_int, sizeof(int)) == -1 ) )
    {
        printf("Error setting options %d\n", errno);
        free(p_int);
        exit(1);
    }
    free(p_int);

    my_addr.sin_family = AF_INET ;
    my_addr.sin_port = htons(host_port);

    memset(&(my_addr.sin_zero), 0, 8);
    my_addr.sin_addr.s_addr = INADDR_ANY ;

    if( bind( hsock, (sockaddr*)&my_addr, sizeof(my_addr)) == -1 )
    {
        fprintf(stderr,"Error binding to socket, make sure nothing else is listening on this port %d\n",errno);
        exit(1);
    }
    if(listen( hsock, 10) == -1 )
    {
        fprintf(stderr, "Error listening %d\n",errno);
        exit(1);
    }

    // init status fields
    status.port=port;
    status.bandwidth=0;
    status.avgLoad=0;
    bytesReceived=0;
    bytesPrevious=0;
    cycles=0;
    // init mutex
    if (pthread_mutex_init(&lock, NULL) != 0)
    {
        printf("\n mutex init failed\n");
        exit(1);
    }
    if (pthread_mutex_init(&setterListLock, NULL) != 0)
    {
        printf("\n mutex init failed\n");
        exit(1);
    }
    if (pthread_mutex_init(&getterListLock, NULL) != 0)
    {
        printf("\n mutex init failed\n");
        exit(1);
    }
    if (pthread_mutex_init(&clientListLock, NULL) != 0)
    {
        printf("\n mutex init failed\n");
        exit(1);
    }
    if (pthread_mutex_init(&statusLock, NULL) != 0)
    {
        printf("\n mutex init failed\n");
        exit(1);
    }
    if (pthread_mutex_init(&bwLock, NULL) != 0)
    {
        printf("\n mutex init failed\n");
        exit(1);
    }
    if (pthread_mutex_init(&bufferLock, NULL) != 0)
    {
        printf("\n mutex init failed\n");
        exit(1);
    }
    if (pthread_mutex_init(&settersOnlineLock, NULL) != 0)
    {
        printf("\n mutex init failed\n");
        exit(1);
    }
}

sServer::~sServer()
{
    //dtor
}

void sServer::Run()
{
    // Server Interface
    addr_size = sizeof(sockaddr_in);
    // Start BandwidthCalculator Thread
    pthread_create(&thread_id,0,&BandwidthCalculator, this ); //create the new Thread
    pthread_detach(thread_id);
    char logFileName[32];
    sprintf(logFileName, "logs/log-SERVER-%d", status.port);
    logFile.open(logFileName); // init log file
    log_timestamp = time(NULL);
    logFile << "sServer " << status.port << "created at " << asctime(localtime(&log_timestamp)) << endl;
    logFile <<  "---------------------------" << endl;

    while(1)
    {
        printf("waiting for a connection\n");
        csock = (int*)malloc(sizeof(int));
        if((*csock = accept( hsock, (sockaddr*)&sadr, &addr_size))!= -1) 	// new connection
        {
            printf("---------------------\nReceived connection from %s\n",inet_ntoa(sadr.sin_addr));
            log_timestamp=time(NULL);
            logFile << asctime(localtime(&log_timestamp)) << "::---------------------\nReceived connection from " << inet_ntoa(sadr.sin_addr) << endl ;
            pthread_mutex_lock(&clientListLock);
            clientList.push_back(inet_ntoa(sadr.sin_addr));
            pthread_mutex_unlock(&clientListLock);
            printClientList();
            clientArgs.server=this;
            clientArgs.socket=*csock;
            pthread_create(&thread_id,0,&SocketHandler, &clientArgs); //create the new Thread
            //pthread_detach(thread_id);
        }
        else
        {
            log_timestamp=time(NULL);
            fprintf(stderr, "Error accepting %d\n", errno);
            logFile << asctime(localtime(&log_timestamp)) << ":: Error "<< errno << " accepting " << endl;
        }
    }
}

void sServer::Stop()
{
    printf("killing thread\n");
    // TO DO
}

//  SET Functions ----------------------------------------------------------------------
void sServer::setStatus( int bytes)
{
    pthread_mutex_lock(&statusLock);
    bytesReceived+=bytes;
    status.avgLoad=(float)bytesReceived/(++cycles);
    //printf("avgLoad = [%.1f B] | ", status.avgLoad);
    pthread_mutex_unlock(&statusLock);
}

void sServer::refreshBandwidth()
{
    pthread_mutex_lock(&bwLock);
    pthread_mutex_lock(&statusLock);
    status.bandwidth = bytesReceived - bytesPrevious;
    pthread_mutex_unlock(&statusLock);
    bytesPrevious = bytesReceived;
    pthread_mutex_unlock(&bwLock);
}

//  GET Functions ----------------------------------------------------------------------
list<string> sServer::getClientList()
{
    pthread_mutex_lock(&clientListLock);
    return clientList;
    pthread_mutex_unlock(&clientListLock);
}

void sServer::sendStatusUpdate()
{
//    pthread_mutex_lock(&lock);
//    return status;
//    pthread_mutex_unlock(&lock);
    int bytecount=0;
    pthread_mutex_lock(&statusLock);
    if( (bytecount=send(balancer->getHsock(), &status, sizeof(status),0))<=0)   // try to send greeting message
    {
        fprintf(stderr, "sServer: Error %d sending status update\n", errno);
        logFile << "sServer: Error " << errno << "sending status update" << endl;
        exit(1);
    }
    pthread_mutex_unlock(&statusLock);
}

// -------------------------------------------------------------------------------------
void sServer::printClientList()
{
    printf("=================\nCLIENT LIST:\n");
    list<string>::iterator iterList;
    pthread_mutex_lock(&clientListLock);
    if(!clientList.empty())
    {
        printf("SIZE = %d\n=================\n", clientList.size());
        for (iterList = clientList.begin(); iterList != clientList.end(); iterList++)
        {
            cout << (*iterList) << endl;
        }
    }
    pthread_mutex_unlock(&clientListLock);

    printf("=================\n");
}



void sServer::closeClientSocket(int socket)
{
    close(socket);
}
