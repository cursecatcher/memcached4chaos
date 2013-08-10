#include "bClient.h"


using namespace std;

bClient::bClient(){
    //void ctor
}
bClient::bClient(char* balancerIp, int clientType, int clientPort, char *name, char* extName){

    // Client Setup
    host_port= 1100;  // BALANCER PORT
	strcpy(host_name, balancerIp); // BALANCER IP ADDRESS

    buffer_len=0;
    // Init Greeting Message
    greetingMsg.type=clientType;
    greetingMsg.port=clientPort;
    strcpy(greetingMsg.name, name);
    strcpy(greetingMsg.extName, extName);
    greetingMsg.bandwidth=0;
    greetingMsg.avgLoad=0;    // Init Socket
    hsock = socket(AF_INET, SOCK_STREAM, 0);
    if(hsock == -1){
        printf("bClient: Error initializing socket %d\n",errno);
         exit(1);
    }

    p_int = (int*)malloc(sizeof(int));
    *p_int = 1;

    if( (setsockopt(hsock, SOL_SOCKET, SO_REUSEADDR, (char*)p_int, sizeof(int)) == -1 )||
        (setsockopt(hsock, SOL_SOCKET, SO_KEEPALIVE, (char*)p_int, sizeof(int)) == -1 ) ){
        printf("bClient: Error setting options %d\n",errno);
        free(p_int);
         exit(1);
    }
    free(p_int);

    my_addr.sin_family = AF_INET ;
    my_addr.sin_port = htons(host_port); //set port

    memset(&(my_addr.sin_zero), 0, 8);
    my_addr.sin_addr.s_addr = inet_addr((char*) host_name); //set ip address
    if( connect( hsock, (struct sockaddr*)&my_addr, sizeof(my_addr)) == -1 ){  // establish connection with server
        if((err = errno) != EINPROGRESS){
            fprintf(stderr, "bClient: Error connecting socket %d\n", errno);
             exit(1);
        }
    }
} // end ctor

void bClient::Send(){
    // Client Interface

    if( (bytecount=send(hsock, &greetingMsg, sizeof(Status),0))== -1){  // try to send greeting message
        fprintf(stderr, "bClient: Error sending data %d\n", errno);
        exit(1);
    }

    printf("## Greeting Message Sent\n");

    //close(hsock);
}

int bClient::getHsock(){
    return hsock;
}

void bClient::closeHsock(){
    close(hsock);
}
