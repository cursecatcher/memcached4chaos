#include "sSetter.h"
#include <ctime>
#include <time.h>
#include <iostream>
#include <string>
#include <map>
#include <msgpack.hpp>

using namespace std;

sSetter::sSetter(int argc, char** argv){
    sClientInfo.type = SETTER; // init type
    ack=ACK; // init constant ack
    ParseParameters(argc, argv); // Init parameters from command line
    // Init to NULL all SERVER connection parameters
    // CONNECT TO BALANCER
    balancedListSize=0;
    InitBalancerConnection(); // get best SERVER by greeting the BALANCER
    PrintServerList();
    char logFileName[32];
    sprintf(logFileName, "logs/log-SETTER-%s", sClientInfo.name);
    //logFile.open(logFileName); // init log file
    // init sSetter
    //msgSize = sizeof(char)*sClientInfo.avgLoad;
    msgSize = sizeof(msg);
    msg.seq=0;
    //msg = (char*)malloc(msgSize);

	printf("SETTER created\n");
    // Display Client Specs
    printf("pkgSize: %d || bandwidth: %d\n", (int)sClientInfo.avgLoad, sClientInfo.bandwidth);
    printf("UI: %d ms || UF: %.1f Hz || MPS: %.1f \n", getUploadInterval(), getUploadFrequency(), getMessagesPerSecond());


    int connectResult=-1;
    // Establish CLIENT -> SERVER connection
    for(srvNumber=-1;(connectResult==-1 && srvNumber<balancedListSize);){
        connectResult=ConnectToServer(); // will return -1 if connection attempt fails
    }

    /*srvNumber=-1; // ConnectToServer() starts increasing srvNumber value, so the first connection will be to balancedList[0];
    int connectResult=-1;
    // Establish CLIENT -> SERVER connection
    for(;connectResult==-1;srvNumber<balancedListSize){
        connectResult=ConnectToServer(); // will return -1 if connection attempt fails
    }
    */
    // init mutex
    if (pthread_mutex_init(&balancedListLock, NULL) != 0)
    {
        printf("\n balancedListLock mutex init failed\n");
        exit(1);
    }

    if (pthread_mutex_init(&lock, NULL) != 0)
    {
        printf("\n lock mutex init failed\n");
        exit(1);
    }
    migrating=0;
    Start();
}

void sSetter::Start(){
   // Client Upload process, with basic output interface
   int sleepTime = getUploadInterval();
   time_t timestamp;
   pthread_create(&bClientThread_id,0,&GetBalancerMessages_Thread, this ); //create bClient Handler Thread to get updated balancedList
   //pthread_detach(bClientThread_id);

    int checkMigration=0;
//************************************************
//                 MAIN LOOP
//************************************************
   while(1){
        // Send sSetter Message
        //fprintf(stderr, "#Debug: %s Msg pre lock\n", sClientInfo.name);

        pthread_mutex_lock(&lock);
        checkMigration = migrating;
        pthread_mutex_unlock(&lock);
        if(checkMigration==0){
                msg.timestamp = (long int)time(NULL);
                msg.usecs=timer.usecs();
                msg.seq++;
                bytecount=send(hsock, &msg, msgSize,MSG_NOSIGNAL); // try to send message

            //fprintf(stderr, "#Debug: %s Msg post lock\n", sClientInfo.name);
            if(bytecount<=0){ // error
                cout << "sSetter: Error "<< errno <<".\nServer "<<balancedList.at(srvNumber).ip <<":"<< balancedList.at(srvNumber).port <<"crashed.\nSwitching to next listed SERVER\n";
                close(hsock);
                ConnectToServer();

            }else{ // message sent correctly
                //timestamp = time(NULL);
                // output interface
                //printf("#Debug: bytecount: %d\n", bytecount);
                printf("%d [%d B] - %s > %s:%d\n",msg.seq,bytecount,sClientInfo.name, host_name, host_port );
            }
            usleep(1000*sleepTime);
        }
        else{
            srvNumber=-1;
            //sleep(1);
            close(hsock);
            printf("#Debug: primary SERVER is back online\n");

            while(time(NULL)<migrate_timestamp){ /* wait for migrate timestamp*/ };
            ConnectToServer();
        }
   }
}
//************************************************
//              CONNECT TO SERVER
//************************************************
int sSetter::ConnectToServer(){
    // CLIENT->SERVER Connection Setup

    fprintf(stderr, "#Debug: %s enter ConnectToServer\n", sClientInfo.name);
    srvNumber++;
    pthread_mutex_lock(&balancedListLock);
    if(srvNumber==balancedList.size()){
        fprintf(stderr, "sSetter: Error %d BALANCED LIST limit reached and no connection established\n", errno);
        exit(1);
    }
    // Get SERVER Ip Address ad Port
    host_port=balancedList.at(srvNumber).port;
    //strcpy(host_name, balancedList[srvNumber].ip);
    strcpy(host_name, balancedList.at(srvNumber).ip.c_str());
    pthread_mutex_unlock(&balancedListLock);

    // Init Socket
    hsock = socket(AF_INET, SOCK_STREAM, 0);
    if(hsock == -1){
        printf("sSetter: Error initializing socket %d\n",errno);
         return -1;
    }

    p_int = (int*)malloc(sizeof(int));
    *p_int = 1;

    if( (setsockopt(hsock, SOL_SOCKET, SO_REUSEADDR, (char*)p_int, sizeof(int)) == -1 )||
        (setsockopt(hsock, SOL_SOCKET, SO_KEEPALIVE, (char*)p_int, sizeof(int)) == -1 ) ){
        printf("sSetter: Error setting options %d\n",errno);
        free(p_int);
       return -1;
    }
    free(p_int);

    my_addr.sin_family = AF_INET ;
    my_addr.sin_port = htons(host_port); //set port

    memset(&(my_addr.sin_zero), 0, 8);
    my_addr.sin_addr.s_addr = inet_addr((char*) host_name); //set ip address
    if( connect( hsock, (struct sockaddr*)&my_addr, sizeof(my_addr)) == -1 ){  // establish connection with server
	if((err = errno) != EINPROGRESS){
	    fprintf(stderr, "sSetter: Error connecting socket %d\n", errno);
	    return -1;
	}
    }
    time_t timer = time(NULL);
    printf("#Debug: attempting to send greetingMessage\n");
    // Greeting Message
    if( (bytecount=send(hsock, &sClientInfo, sizeof(Status),0)) <=0 ){  // try to send greeting message
        fprintf(stderr, "sSetter: Error %d sending sSetter Greeting Message after %ld ms\n", errno, (time(NULL)-timer) );
        return -1;
    }
    printf("#Debug: sent %s greeting message after %ld ms\n", sClientInfo.name, (time(NULL)-timer) );
    //Receive Ack
    int ack;
    if( (bytecount=recv(hsock, &ack, sizeof(int),0))<=0){  // try to send greeting message
        fprintf(stderr, "sSetter: Error %d receiving sServer Greeting Message after %ld ms\n", errno, (time(NULL)-timer) );
        return -1;
    }

    pthread_mutex_lock(&lock);
    printf("#Debug: %s entered lock\n", sClientInfo.name);
    if(migrating==1) migrating=0;
    pthread_mutex_unlock(&lock);
    fprintf(stderr, "#Debug: %s exit ConnectToServer\n", sClientInfo.name);
    return 1;
}
//************************************************
//            RECV BALANCER MESSAGES
//************************************************
void sSetter::ReceiveBalancerMessage(){
    int msg=-1;
    int bytecount=0;
    // fist handshake
    if( (bytecount=recv(balancerSck, &msg, sizeof(int),0)) <=0){
        fprintf(stderr, "sSetter %s: Error %d receiving Message from BALANCER\n", sClientInfo.name, errno);
        exit(1);
    }
    printf("#Debug: receiving SERVER list update\n");
    if( (bytecount=send(balancerSck, &ack, sizeof(int),0)) <=0){
    fprintf(stderr, "sSetter %s: Error %d sending Message ACK to BALANCER\n", sClientInfo.name, errno);
    exit(1);
    }
    switch(msg){
        case LIST_UPDATE:
            printf("#Debug: receiving SERVER list update\n");
            GetBalancedListUpdate();
        break;
        case MIGRATE:
        pthread_mutex_lock(&lock);
        printf("#Debug: %s receiving MIGRATE TIMESTAMP\n", sClientInfo.name);
        if( (bytecount=recv(balancerSck, &migrate_timestamp, sizeof(long int),0)) <=0){  // try to send greeting message
            fprintf(stderr, "bServer: Error %d receiving ack from clientSck %d\n", errno, balancerSck);
//            exit(1);
        }
        migrating=1;
        printf("#Debug: %s sending  MIGRATE TIMESTAMP ACK\nnow: %d - migration:%d\n", sClientInfo.name, (int)(time(NULL)), (int)migrate_timestamp);
        if( (bytecount=send(balancerSck, &ack, sizeof(int),0)) <=0){
        fprintf(stderr, "sSetter %s: Error %d sending Message ACK to BALANCER\n", sClientInfo.name, errno);
        exit(1);
        }

        pthread_mutex_unlock(&lock);
        break;
        default:
            perror("#Debug: Unknown BALANCER Message\n");
        break;
    }
}

void sSetter::InitBalancerConnection(){
        bClient bal(balancerIp, sClientInfo.type, 0, sClientInfo.name, sClientInfo.extName);
        balancer= &bal;
        balancer->Send();
        int bytecount=0;
        balancerSck = balancer->getHsock();
        printf("#Debug: Receiving balancedList on socket %d\n", balancerSck);

        //free(balancedList);

            // fist handshake
    if( (bytecount=recv(balancerSck, &msg, sizeof(int),0)) <=0){
        fprintf(stderr, "sSetter %s: Error %d receiving Init Message from BALANCER\n", sClientInfo.name, errno);
        exit(1);
    }
    printf("#Debug: receiving SERVER list update\n");
    if( (bytecount=send(balancerSck, &ack, sizeof(int),0)) <=0){
    fprintf(stderr, "sSetter %s: Error %d sending Init Message ACK to BALANCER\n", sClientInfo.name, errno);
    exit(1);
    }

        // 1) Receive balancedListBytes from BALANCER
        long int balancedListBytes;
        if( (bytecount=recv(balancerSck, &balancedListBytes, sizeof(long int),0)) <=0){  // try to send greeting message
            fprintf(stderr, "sSetter: Error %d receiving balancedListSize from BALANCER\n", errno);
            exit(1);
        }
        //balancedList = (struct fullIpAddress*)malloc(sizeof(struct fullIpAddress)*balancedListSize); // allocate balancedList Array
        //printf("#Debug: Received and allocated balancedList ( %d servers )\n", balancedListSize);
        printf("#Debug: Received and allocated balancedList ( %ld bytes )\n", balancedListBytes);

        //if (balancedListSize<=0) {
        if (balancedListBytes<=0) {
            fprintf(stderr, "sSetter: No Servers Available to read LiveData\n");
            exit(1);
        }
        // 2) Send ACK to BALANCER
        if( (bytecount=send(balancerSck, &ack, sizeof(int),0))== -1){  // try to send greeting message
            fprintf(stderr, "sSetter: Error %d sneding ack to BALANCER\n", errno);
            exit(1);
        }

        // 3) get balancedList from BALANCER
        //msgpack::sbuffer sbuf;
        char sbuf[balancedListBytes];
        // deserializes these objects using msgpack::unpacker.

        //if( (bytecount=recv(balancerSck, balancedList, sizeof(struct fullIpAddress)*balancedListSize,0))== -1){  // try to send greeting message
            if( (bytecount=recv(balancerSck, &sbuf, balancedListBytes,0)) <=0){  // try to send greeting message
                fprintf(stderr, "Client: Error %d receiving balancedList from BALANCER\n", errno);
            exit(1);
        }
        printf("Debug: bytecount = %d \n", balancedListBytes);
        msgpack::unpacked msg;
        msgpack::unpack(&msg, sbuf, balancedListBytes );
        msgpack::object obj = msg.get();

        // you can convert object to myclass directly
        std::vector<fullIpAddress> rvec;
        printf("Debug: Converting...\n");
        obj.convert(&rvec);
        printf("Debug: converted!\n");
        pthread_mutex_lock(&balancedListLock);
        printf("#Debug %s: balancedlist locked\n", sClientInfo.name);
        balancedList = rvec;
        pthread_mutex_unlock(&balancedListLock);
        printf("#Debug %s: balancedlist unlocked\n", sClientInfo.name);
        //balancer->closeHsock();

}


void sSetter::GetBalancedListUpdate(){
    int bytecount=0;
    long int balancedListBytes;
    // Wait for a n update from BALANCER
    // 1) balancedListSize from BALANCER (in bytes)
    if( (bytecount=recv(balancerSck, &balancedListBytes, sizeof(long int),0)) <=0){  // try to send greeting message
        fprintf(stderr, "sSetter: Error %d receiving Updated from BALANCER on socket %d\n", errno, balancerSck);
        exit(1);
    }
    //pthread_mutex_lock(&balancedListLock);
    //free(balancedList);
    //balancedList = (struct fullIpAddress*)malloc(sizeof(struct fullIpAddress)*balancedListSize); // allocate balancedList Array
    //balancedList.resize(balancedListSize/sizeof(fullIpAddress));
    printf("#Debug: Received and allocated Updated balancedList ( %ld bytes)\n", balancedListBytes);

    if (balancedListBytes<=0) {
        fprintf(stderr, "sSetter: No Servers Available to read LiveData\n");
        exit(1);
    }
    // 2) Send ACK to BALANCER
    if( (bytecount=send(balancerSck, &ack, sizeof(int),0))== -1){  // try to send greeting message
        fprintf(stderr, "sSetter: Error %d sneding ack to BALANCER\n", errno);
        exit(1);
    }
// 3) get balancedList from BALANCER

        //msgpack::sbuffer sbuf;
        char sbuf[balancedListBytes];
        // deserializes these objects using msgpack::unpacker.

        //if( (bytecount=recv(balancerSck, balancedList, sizeof(struct fullIpAddress)*balancedListSize,0))== -1){  // try to send greeting message
            if( (bytecount=recv(balancerSck, &sbuf, balancedListBytes,0)) <=0){  // try to send greeting message
                fprintf(stderr, "Client: Error %d receiving balancedList from BALANCER\n", errno);
            exit(1);
        }
        printf("Debug: bytecount = %d \n", balancedListBytes);
        msgpack::unpacked msg;
        msgpack::unpack(&msg, sbuf, balancedListBytes );
        msgpack::object obj = msg.get();

        // you can convert object to myclass directly
        std::vector<fullIpAddress> rvec;
        obj.convert(&rvec);
        pthread_mutex_lock(&balancedListLock);
        printf("#Debug %s: balancedlist locked\n", sClientInfo.name);
        balancedList = rvec;
        pthread_mutex_unlock(&balancedListLock);
        printf("#Debug %s: balancedlist unlocked\n", sClientInfo.name);
        //balancer->closeHsock();
    //pthread_mutex_unlock(&balancedListLock);
}


void sSetter::PrintServerList(){
    printf("\n====================\n");
    printf("BALANCED List:\n");


    for(int i=0; i< balancedList.size();i++)
        printf("%s:%d\n", balancedList.at(i).ip.c_str(), balancedList.at(i).port);
    printf("====================\n\n");
}

void sSetter::ParseParameters(int argc, char** argv){
    //connect to BALANCER
     unsigned int parametersNumber=9;
     printf("#Debug: number of params: %d\n",argc );
     printf("--- sSetter ---\n");
     // Check if all needed parameters are entered
     if(argc < parametersNumber ){
        printf("Usage: %s -n <clientName>\n -b <balancer_ip> -s <package size> -ui/uf/mps <update interval> \n type = 1 for SETTER ; 2 for GETTER\n", argv[0]) ;
        exit(1);
    }

    enum parameters {name_par, /*type_par,*/ balancerIp_par, pkg_size_par, upload_interval_par, upload_frequency_par, upload_mps_par};
    map<string, int> parametersMap;
    parametersMap["-n"] = name_par;
    //parametersMap["-t"] = type_par;
    parametersMap["-b"] = balancerIp_par;
    parametersMap["-s"] = pkg_size_par;
    // 3 ways to set upload frequency
    parametersMap["-ui"] = upload_interval_par; // message upload interval in milliseconds
    parametersMap["-uf"] = upload_frequency_par; // message upload frequency in Hz
    parametersMap["-mps"] = upload_mps_par; // messages per second
    int ui;
    float uf, mps; // store temp parameters to calculate bandwidth once we have the pkgSize
    // Parsing line parameters
    for(unsigned int i=1; i<parametersNumber; i++){
        switch(parametersMap[ argv[i] ] )
        {
        /*
        case type_par:
            sClientInfo.type=atoi(argv[++i]); // define CLIENT type (GETTER or SETTER) from command line parameter
            break;
        */
        case name_par:
            strcpy(sClientInfo.name, argv[++i]);
            break;
        case balancerIp_par:
            strcpy(balancerIp, argv[++i]);
            break;
        case pkg_size_par:
            sClientInfo.avgLoad = atoi(argv[++i]);  // set package size to communicate to sServer
            if ( sClientInfo.avgLoad < 0 ) {
               perror("Error, package size must be a positive value.\n");
               exit(1);
            }
            break;
        case upload_interval_par:
            ui = atoi(argv[++i]);
            if ( ui < 0 ) {
               perror("Error, update interval must be a positive value.\n");
               exit(1);
            }
            uf = (float) 1000/ui;
            mps = 1.0/uf;
            break;
        case upload_frequency_par:
            uf = (float) atof(argv[++i]);
            if ( ui < 0 ) {
               perror("Error, update interval must be a positive value.\n");
               exit(1);
            }
            ui = (float) 1000/uf;
            mps = 1.0/uf;
            break;
        case upload_mps_par:
            mps = (float) atof(argv[++i]);
            if ( ui < 0 ) {
               perror("Error, update interval must be a positive value.\n");
               exit(1);
            }
            uf = 1.0/mps;
            ui = (float) 1000/uf;
            break;
        default:
            printf("Unrecognised Parameter %s\n",argv[i]);
            exit(1);
        }

    }

    sClientInfo.bandwidth = sClientInfo.avgLoad * uf ; // set bandwidth to communicate to sServer

}

// Getter Functions
float sSetter::getUploadFrequency(){
    return (float) sClientInfo.bandwidth / sClientInfo.avgLoad;
}

float sSetter::getMessagesPerSecond(){
    return   (float) sClientInfo.avgLoad / sClientInfo.bandwidth ;
}

int sSetter::getUploadInterval(){
    return  1000 * ( (float) sClientInfo.avgLoad / sClientInfo.bandwidth );
}