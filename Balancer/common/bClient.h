#ifndef BCLIENT_H
#define BCLIENT_H

#include <fcntl.h>
#include <stdlib.h>
#include <errno.h>
#include <netinet/in.h>
#include <resolv.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include "common.h"

class bClient{
private:
    Status greetingMsg;
    int host_port;
    char host_name[16];
    char *buffer; //input message from server
    int bytecount;
    int buffer_len;
    int hsock;
    int * p_int;
    int err;
    bClient *balancer;
    char balancerIp[16];
    struct sockaddr_in my_addr;

public:
bClient();
bClient(char* balancerIp, int clientType, int clientPort, char *name, char *extName);
void Send();
int getHsock();
void closeHsock();

};

#endif
