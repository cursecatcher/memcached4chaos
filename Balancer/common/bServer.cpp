#include "bServer.h"
#include <iostream>
#include <stdlib.h>
#include <msgpack.hpp>

bServer::bServer(){
    // ctor
    // balancedList init
    balancedListSize=1;
    //balancedList = (fullIpAddress*)malloc(sizeof(fullIpAddress)*balancedListSize); // not used at the moment, but if not allocated we get an error at the first call of BalanceList() due to the free()

    //  Server Setup
    host_port= 1100; //bServer port
    addr_size = 0;
    thread_id=0;
    hsock = socket(AF_INET, SOCK_STREAM, 0);
    if(hsock == -1){
        printf("Error initializing socket %d\n", errno);
	exit(1);
    }
    p_int = (int*)malloc(sizeof(int));
    *p_int = 1;

    if( (setsockopt(hsock, SOL_SOCKET, SO_REUSEADDR, (char*)p_int, sizeof(int)) == -1 )||
        (setsockopt(hsock, SOL_SOCKET, SO_KEEPALIVE, (char*)p_int, sizeof(int)) == -1 ) ){
        printf("Error setting options %d\n", errno);
        free(p_int);
        exit(1);
    }
    free(p_int);

    my_addr.sin_family = AF_INET ;
    my_addr.sin_port = htons(host_port);

    memset(&(my_addr.sin_zero), 0, 8);
    my_addr.sin_addr.s_addr = INADDR_ANY ;

    if( bind( hsock, (sockaddr*)&my_addr, sizeof(my_addr)) == -1 ){
        fprintf(stderr,"Error binding to socket, make sure nothing else is listening on this port %d\n",errno);
	exit(1);
    }
    if(listen( hsock, 10) == -1 ){
        fprintf(stderr, "Error listening %d\n",errno);
        exit(1);
    }

    // init mutex
    if (pthread_mutex_init(&lock, NULL) != 0)
    {
        printf("\n mutex init failed\n");
        exit(1);
    }

    // Create Thread to Refresh the status of every SERVER
    pthread_create(&thread_id,0,&pollServerStatus, this );
    pthread_detach(thread_id);
    // Create Thread to cet USER COMMANDS
    pthread_create(&thread_id,0,&UserCommandListener, this );
    pthread_detach(thread_id);

    time_t timeVar = time(NULL);
    //long int t = (long int) timeVar;
    printf("time_t -> \t%s\n", asctime(localtime(&timeVar))) ;
}

bServer::~bServer()
{
    //dtor
}

void bServer::Run(){
    // Server Interface

    addr_size = sizeof(sockaddr_in);
    int bytecount;
    Status greetingMsg; //received struct

    while(true){
        printf("waiting for a connection\n");
        printf("cmd > ");
        // 1) Wait for a new connection from a Client/Server
        csock = (int*)malloc(sizeof(int));
        if((*csock = accept( hsock, (sockaddr*)&sadr, &addr_size))!= -1){	// new connection
            printf("\n********************************************************\n");
            printf("\nReceived connection from %s \n",inet_ntoa(sadr.sin_addr));
        // 2) Receive Greeting message
         if((bytecount = recv(*csock, &greetingMsg, sizeof(Status), 0)) <=0){
            fprintf(stderr, "\nError receiving 1st message %d\n", errno);
            free(csock);
            //return 0;
          }
          // Update bClientStatus
          //printf("Received Greeting Message\n\n");
          bClientStatus tempStatus;
          tempStatus.csock=*csock;
          strcpy(tempStatus.ip, inet_ntoa(sadr.sin_addr));
          strcpy(tempStatus.name, greetingMsg.name);
          strcpy(tempStatus.extName, greetingMsg.extName); // ????????? why doesn't write the correct name ???????
          tempStatus.port=greetingMsg.port;
          tempStatus.bandwidth=greetingMsg.bandwidth;
          tempStatus.avgLoad=greetingMsg.avgLoad;
          tempStatus.type=ONLINE;
          //tempStatus.list=(fullIpAddress*)malloc(1); //dummy malloc to avoid error due to the free on the first BalanceServerList() call
          switch(greetingMsg.type){
            case SERVER:
                printf("type = SERVER\n");
                if (!(checkForReconnection(tempStatus.ip, tempStatus.port, tempStatus.csock)) ){
                    serverList.push_front(tempStatus); // if it's a brand new SERVER connection and not a reconnection from a previously listed SERVER
                    getStatusUpdate();
                    BalanceServerList(SERVER);
                    sendBalancedList();

                } else{
                    printf("#Debug: SERVER %s:%d reconnection on socket: %d\n", tempStatus.ip, tempStatus.port, tempStatus.csock);
                    getStatusUpdate();
                    BalanceServerList(RECON);
                    sendBalancedList();
                }
                printServerList();
                //printBalancedList();
                break;
            case GETTER:
                getterList.push_front(tempStatus);
                printf("name = %s | type = GETTER\n", greetingMsg.name);
                printf("greetingMsg.extName = %s \n", greetingMsg.extName);
                printf("tempStatus.extName = %s \n", tempStatus.extName);
                getStatusUpdate();
                BalanceServerList(GETTER);
                // send the GETTER the list of SERVERS of the requested SETTER. the list has to be the same because every GETTER switches to the same servers of the setter it's monitoring
                sendBalancedList();
                printServerList();
                printGetterList();
                printList(*getterList.begin());
                break;
            case SETTER:
                setterList.push_front(tempStatus);
                printf("name = %s | type = SETTER\n", greetingMsg.name);
                getStatusUpdate();
                BalanceServerList(SETTER);
                sendBalancedList();
                printServerList();
                printSetterList();
//                printBalancedList();
                break;
            }

        }
        else{
            fprintf(stderr, "Error accepting %d\n", errno);
        }
    }
  }

void bServer::Stop(){
    printf("killing thread\n");
}

// Functions GET and PRINT lists
list<struct bClientStatus> bServer::getSetterList(){
    return setterList;
}
list<struct bClientStatus> bServer::getGetterList(){
    return getterList;
}
list<struct bClientStatus> bServer::getServerList(){
    return serverList;
}

void bServer::printSetterList()
{
    printf("\n====================\n");
    printf("SETTER List:\n");
    printf("====================\n");
    list<struct bClientStatus>::iterator iterList;
    if(!setterList.empty()){
        for (iterList = setterList.begin(); iterList != setterList.end(); iterList++)
               {
                   std::cout << iterList->name << "@" << iterList->ip << endl;
               }
    }
    printf("\n");
    printf("cmd > ");
}

void bServer::printGetterList()
{
    printf("\n====================\n");
    printf("GETTER List:\n");
    printf("====================\n");
    list<struct bClientStatus>::iterator iterList;
    if(!setterList.empty()){
        for (iterList = getterList.begin(); iterList != getterList.end(); iterList++)
               {
                   std::cout << iterList->name << "@" << iterList->ip << endl;
               }
    }
    printf("\n");
    printf("cmd > ");
}

void bServer::printServerList()
{
    printf("\n====================\n");
    printf("SERVER List:\n");
    printf("====================\n");
    list<struct bClientStatus>::iterator iterList;
    string sck;
    if(!serverList.empty()){
        for (iterList = serverList.begin(); iterList != serverList.end(); iterList++)
               {
                   if(iterList->type == OFFLINE){
                       cout << iterList->ip << ":" << iterList->port << " OFFLINE " << " avgLoad: " << (int)(iterList->avgLoad) << " B" << "\tbandwidth: " << iterList->bandwidth << " B/s" << endl;
                   }
                   else{
                    cout << iterList->ip << ":" << iterList->port << " sck:  "<< iterList->csock <<  " avgLoad: " << (int)(iterList->avgLoad) << " B" << "\tbandwidth: " << iterList->bandwidth << " B/s" << endl;
                   }
               }
    }
    printf("\n");

}


void bServer::printBalancedList()
{
    printf("\n====================\n");
    printf("BALANCED List:\n");
    printf("====================\n");
    for (int i=0; i<balancedList.size() ; i++)
       {
           cout << balancedList.at(i).ip << ":" << balancedList.at(i).port << endl;
       }
    printf("\n");
}

void bServer::printList(bClientStatus& cli)
{
    printf("\n====================\n");
    printf("CLIENT %s serverList \n", cli.name);
    printf("====================\n");
    for (int i=0; i<balancedList.size() ; i++)
       {
           cout << cli.list.at(i).ip << ":" << cli.list.at(i).port << endl;
       }
    printf("\n");
}
bool bServer::checkForReconnection(char *ip, int port, int socket){

    list<struct bClientStatus>::iterator iterServerList;
    list<struct bClientStatus>::iterator iterSetterList;
    list<struct bClientStatus>::iterator iterGetterList;

    if(!serverList.empty()){
        //pthread_mutex_lock(&lock);
        for (iterServerList = serverList.begin(); iterServerList != serverList.end(); iterServerList++){
            if(strcmp(iterServerList->ip, ip)==0 && iterServerList->port==port){ // if new connected SERVER was already listed
                iterServerList->csock=socket;
                iterServerList->type=ONLINE; // set it back ONLINE
                pthread_mutex_unlock(&lock);
                // communicate to every SETTER that has this as primary SERVER, to migrate back
                for (iterSetterList = setterList.begin(); iterSetterList != setterList.end(); iterSetterList++){
                    if((iterSetterList->list.at(0).ip == ip) && iterSetterList->list.at(0).port==port){
                        printf("#Debug: Sending MIGRATE signal to SETTER %s\n",iterSetterList->name );
                        communicateReconnection(iterSetterList->csock, (long int)(time(NULL)+1));
                        printf(" OK!\n");
                    }
                }
                //sleep(1);
                // communicate to every GETTER that has this as primary SERVER, to migrate back
                for (iterGetterList = getterList.begin(); iterGetterList != getterList.end(); iterGetterList++){
                    if((iterGetterList->list.at(0).ip == ip) && iterGetterList->list.at(0).port==port){
                        printf("#Debug: Sending MIGRATE signal to GETTER %s ...",iterGetterList->name );
                        communicateReconnection(iterGetterList->csock, (long int)(time(NULL)+1));
                        printf(" OK!\n");
                    }
                }
                return true;
            }
        }
        //pthread_mutex_unlock(&lock);
    }
    return false;
}

void bServer::communicateReconnection(int clientSocket, long int timestamp){
    int msg=MIGRATE;
    int bytecount=0;
    int ack=0;
    // 0 A) Send Message and Handshake
    if( (bytecount=send(clientSocket, &msg, sizeof(int),MSG_NOSIGNAL)) <=0){
        fprintf(stderr, "bServer: Error %d sending MIGRATE Message to clientSck %d\n", errno, clientSocket);
        //exit(1);
    }
// 0 B) Receive ACK from CLIENT

    if( (bytecount=send(clientSocket, &timestamp, sizeof(long int),MSG_NOSIGNAL)) <=0){
        fprintf(stderr, "bServer: Error %d sending TIMESTAMP to clientSck %d\n", errno, clientSocket);
        //exit(1);
    }
if( (bytecount=recv(clientSocket, &ack, sizeof(int),0)) <=0){  // try to send greeting message
            fprintf(stderr, "bServer: Error %d receiving TIMESTAMP ack from clientSck %d\n", errno, clientSocket);
//            exit(1);
}
    // 1) send timestamp for reconnection
//    if( (bytecount=send(clientSocket, &timestamp, sizeof(long int),MSG_NOSIGNAL))== -1){
//        perror("bServer: Error communicating primary server reconnection\n");
//        exit(1);
//    }
}

void bServer::getStatusUpdate(){
    int bytecount;
    int request=STATUS_REQ;
    Status tmpUpdate;
    list<struct bClientStatus>::iterator iterList;
    if(!serverList.empty()){
        for (iterList = serverList.begin(); iterList != serverList.end(); iterList++)
               {
                    if(iterList->type==OFFLINE) continue; // skip to next server if the current one is listed as offline

                    pthread_mutex_lock(&lock);
                    // 1) Send Update Request to SERVER
                    if( (bytecount=send(iterList->csock, &request, sizeof(int),MSG_NOSIGNAL)) <=0){  // try to send greeting message
                        fprintf(stderr, "bServer: Error %d sending update request to sck %d\n", errno, iterList->csock);
                        iterList->type=OFFLINE; // mark as OFFLINE to avoid further update request attempt until it come back ONLINE
                        close(iterList->csock);
                        //iterList->csock = -1;
                        pthread_mutex_unlock(&lock); //release lock
                        printServerList();
                        continue; // skip to next SERVER
                        //exit(1);
                    }
                    // 2) Receive Update from SERVER
                    if((bytecount = recv(iterList->csock, &tmpUpdate, sizeof(Status), 0)) <=0){
                        fprintf(stderr, "bServer: Error %d receiving update request ffrom sck %d\n", errno, iterList->csock);
                        iterList->type=OFFLINE; // mark as OFFLINE to avoid further update request attempt until it come back ONLINE
                        close(iterList->csock);
                        //iterList->csock = -1;
                        pthread_mutex_unlock(&lock); //release lock
                        continue; // skip to next SERVER
                    }
                    iterList->avgLoad = tmpUpdate.avgLoad;
                    iterList->bandwidth = tmpUpdate.bandwidth;
                    pthread_mutex_unlock(&lock);
               }
    }
    return;
}

// Function used to sort serverLlist
bool by_bandidthw_then_avgLoad(bClientStatus const &a, struct bClientStatus const &b) {
	if (a.bandwidth != b.bandwidth){
		return (a.bandwidth < b.bandwidth);
	}
        else return (a.avgLoad < b.avgLoad);
}

void bServer::BalanceServerList(int cause){
 // the flag tells the cause of rebalancing: SERVER, SETTER or GETTER
 // if a new SETTER connection occurs, we should pass it (and all the GETTERS monitoring it) the full new balancedList
 // if a new GETTER connection occurs, it should receive the correct list during the setterList iteration, when the correct setter is reached
 // and send the others the PIVOTED version, with their balancedList[0] fixed and the others according to the new list

 balancedListSize=serverList.size();
 //free(balancedList); // free old balancedList
 //balancedList = (fullIpAddress*)malloc(sizeof(fullIpAddress)*balancedListSize); // reallocate balancedList
 balancedList.resize(balancedListSize);

  list<struct bClientStatus>::iterator iterList;
  list<struct bClientStatus>::iterator iterGetterList;
  int i = 0; // for straight list

serverList.sort(by_bandidthw_then_avgLoad); // Balance serverList

for (iterList = serverList.begin(); iterList != serverList.end(); iterList++) //start from bottom, since last server to go online probably is the less loaded
 {
    //strcpy(balancedList.at(i).getIp, iterList->ip);
    balancedList.at(i).ip=(iterList->ip);
    balancedList.at(i).port=(iterList->port);
    i++; // for straight list
 }
 printf("#Debug: Unsorted List:\n");

 // Copy optimal List in each CLIENT's list field
 fullIpAddress pivot; // to temporary store list[0] of every client
 iterList = setterList.begin();
 if (cause==SETTER) { // if we're balancing because a new SETTER has connceted
    iterList->list.resize(balancedListSize);
    for(int j=0;j<balancedListSize;j++) iterList->list.at(j)=balancedList.at(j); // we copy the full balancedList into its "list" field
    printList(*iterList);
    iterList++; // and we go on to the next pre-existent CLIENT
}

 for ( ;iterList != setterList.end(); iterList++){
        //if (cause==SERVER) { // if we need to enlarge list **** NOTE **** we could use a vector and make an array copy inside SendBalancedList for send purposes, maybe it will increase performace
        //backup "pivot" server
        pivot.ip=( iterList->list.at(0).ip );
        pivot.port=( iterList->list.at(0).port) ;
        // reallocate list memmory
        iterList->list.resize(balancedListSize);
        // restore "pivot" server
        iterList->list.at(0).ip=(pivot.ip);
        iterList->list.at(0).port=(pivot.port);
    //}
    int k=0; // iterates through the SETTER list, skipping the first element, that remains fixed
    for(int j=1;j<balancedListSize;j++) {
//        if((balancedList.at(k).ip == iterList->list.at(0).getIp())==0 && balancedList.at(k).port == iterList->list.at(0).port ){ k++; printf("#Debug: duplicate found!\n");}  // if we didn't meet the "pivot" server duplicate
        if((balancedList.at(k).ip == iterList->list.at(0).ip) && balancedList.at(k).port == iterList->list.at(0).port ){ k++; printf("#Debug: duplicate found!\n");}  // if we didn't meet the "pivot" server duplicate
            // fill the list as intended
           //strcpy(iterList->list[j].ip,balancedList.at(k).ip);
            iterList->list.at(j).ip=(balancedList.at(k).ip);
            iterList->list.at(j).port=(balancedList.at(k).port);
            k++;
        }
    printList(*iterList);
    }
}

void bServer::sendBalancedList(){
    int bytecount;
    int ack=-1;
    int msg=LIST_UPDATE;
    list<struct bClientStatus>::iterator iterList;
    for (iterList = setterList.begin(); iterList != setterList.end(); iterList++){ // send update to every client in clientList

        // 0 A) Send Message and Handshake
        if( (bytecount=send(iterList->csock, &msg, sizeof(int),MSG_NOSIGNAL)) <=0){
            fprintf(stderr, "bServer: Error %d sending LIST_UPDATE Message to clientSck %d\n", errno, iterList->csock);
            //exit(1);
        }
            // 0 B) Receive ACK from CLIENT
            if( (bytecount=recv(iterList->csock, &ack, sizeof(int),0))<=0){  // try to send greeting message
                fprintf(stderr, "bServer: Error %d receiving ack from clientSck %d\n", errno, iterList->csock);
    //            exit(1);
            }
            // 1) Send balancedListSize to CLIENT
            // serialise the list
            msgpack::sbuffer sbuf;
            msgpack::pack(sbuf, iterList->list);
            long int  balancedListBytes = sbuf.size();
            if( (bytecount=send(iterList->csock, &balancedListBytes, sizeof(long int),MSG_NOSIGNAL))<=0){  // try to send greeting message
                fprintf(stderr, "bServer: Error %d sending balancedListSize to clientSck %d\n", errno, iterList->csock);
                //exit(1);
            }
            // 2) Receive ACK from CLIENT
            if( (bytecount=recv(iterList->csock, &ack, sizeof(int),0))<=0){  // try to send greeting message
                fprintf(stderr, "bServer: Error %d receiving ack from clientSck %d\n", errno, iterList->csock);
    //            exit(1);
            }
            if (ack!=ACK){
                fprintf(stderr, "bServer: Error %d receiving ack from clientSck %d\n", errno, iterList->csock);
    //            exit(1);
            }
            // 3) send balancedList to CLIENT

                if( (bytecount=send(iterList->csock, sbuf.data(), sbuf.size(),MSG_NOSIGNAL)) <=0){
                    fprintf(stderr, "bServer: Error %d sending balanced List to sck %d\n", errno, iterList->csock);
    //            exit(1);
            }

            // ********************************************************************************
            // SEND a copy of current BalancedList to every GETTER reading this SETTER's output
            // ********************************************************************************
        list<struct bClientStatus>::iterator iterListGetter;
        for (iterListGetter = getterList.begin(); iterListGetter!= getterList.end(); iterListGetter++){ // send update to every client in clientList

        if( strcmp(iterListGetter->extName, iterList->name)==0 ) {//if it's monitoring the current SETTER

            // copy SETTER list into GETTER list
            iterListGetter->list = iterList->list;
            // 0 A) Send Message and Handshake
            if( (bytecount=send(iterListGetter->csock, &msg, sizeof(int),MSG_NOSIGNAL)) <=0){
                fprintf(stderr, "bServer: Error %d sending LIST_UPDATE Message to clientSck %d\n", errno, iterListGetter->csock);
                //exit(1);
            }
                // 0 B) Receive ACK from CLIENT
                if( (bytecount=recv(iterListGetter->csock, &ack, sizeof(int),0)) <=0){  // try to send greeting message
                    fprintf(stderr, "bServer: Error %d receiving ack from clientSck %d\n", errno, iterListGetter->csock);
        //            exit(1);
                }
                // 1) Send balancedListSize to CLIENT
                // serialise the list
                msgpack::sbuffer sbuf;
                msgpack::pack(sbuf, iterList->list);  // Obviously it's the external loop list, SETTER's list
                long int  balancedListBytes = sbuf.size();
                printf("Debug: sbuf.size() = %ld \nbalancedListBytes = %ld ", sbuf.size(), balancedListBytes);
                if( (bytecount=send(iterListGetter->csock, &balancedListBytes, sizeof(long int),MSG_NOSIGNAL)) <=0){  // try to send greeting message
                    fprintf(stderr, "bServer: Error %d sending balancedListSize to getterSck %d\n", errno, iterListGetter->csock);
                    //exit(1);
                }
                // 2) Receive ACK from CLIENT
                if( (bytecount=recv(iterListGetter->csock, &ack, sizeof(int),0)) <=0){  // try to send greeting message
                    fprintf(stderr, "bServer: Error %d receiving ack from clientSck %d\n", errno, iterListGetter->csock);
        //            exit(1);
                }
                if (ack!=ACK){
                    fprintf(stderr, "bServer: Error %d receiving ack from clientSck %d\n", errno, iterListGetter->csock);
        //            exit(1);
                }
                // 3) send balancedList to CLIENT
                //if( (bytecount=send(iterListGetter->csock, iterListGetter->list, sizeof(fullIpAddress)*balancedListSize,MSG_NOSIGNAL))== -1){  // try to send greeting message
                    if( (bytecount=send(iterListGetter->csock, sbuf.data(), sbuf.size(),MSG_NOSIGNAL)) <=0){
                        fprintf(stderr, "bServer: Error %d sending balanced List to sck %d\n", errno, iterListGetter->csock);
        //            exit(1);
                }

            }
        }
   }
}


