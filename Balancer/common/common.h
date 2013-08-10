#ifndef CONS_H
#define CONS_H

#include <ctime>
#include <string.h>
#include <vector>
#include <msgpack.hpp>
#include <sys/time.h>
//CLIENT flags
#define RECON -1
#define SERVER 0
#define GETTER 1
#define SETTER 2
//SERVER flags
#define OFFLINE 3
#define ONLINE 4
//misc
#define ERR -1
#define ACK 1
#define MIGRATE 2
#define STATUS_REQ 3
#define LIST_UPDATE 4
#define BUF_SIZE 1024 // number of messages stored for

using namespace std;
// struct status is also used as bClient -> bServer and sClient -> sServer Greeting Message
// containing all the info about send/recv process: avgLoad (aka sClient pkgSize) and bandwidth ( 1000 / sClient.update_interval_param )
//

class Status{
	public:
	int type; // can be SERVER , SETTER or GETTER
	int port; // set by SERVER
	char name[32];  // for debug purpose
	// Data fields
	char extName[32]; // used by GETTER to comunicate the SETTER he wants to monitor
	int bandwidth; // Bytes/sec
	float avgLoad; // Bytes
};

class fullIpAddress {
   public:
	std::string ip;
	int port;
	MSGPACK_DEFINE(ip, port);
};

struct bClientStatus{ // used by sServer
// very similar to the status structure used by bClient and bServer, with the addition of the sending CLIENT's ip
// a bClientStatus wil be created for every incoming bClient connection, and appended to the respective list ( serverList orclientList )
	// status fields
	int type; // for CLIENT it can be GETTER or SETTER ; for SERVER it can be ONLINE(default) or OFFLINE(waiting to be up again)
	int port; // set by SERVER
	int bandwidth; // Bytes/sec
	float avgLoad; // Bytes
	char name[32];  //
	char extName[32]; // used by GETTER to comunicate the SETTER he wants to monitor
	// added fields
	int csock; // socket connection number to the bClient
	char ip[16]; // its ip
	std::vector<fullIpAddress> list; 	//  case SERVER -> list of connected CLIENTs to restore in case of disconnection/reconnection
					//  case CLIENT -> balancedList of servers, with list[0] default SERVER to which migrate back in case of disconnection/reconnection
};

class Message{
	public:
		long int data;
		int seq;
		long int timestamp;
		uint32_t usecs;
		MSGPACK_DEFINE(data, seq, timestamp, usecs);
};


class ThreadArgs {
    public:
	void* server;
	int 	 socket;
};

typedef std::vector<Message>  MessageVector;
//typedef Message[BUF_SIZE]  MessageVector;
typedef std::map<string , MessageVector> BufferMap;

class Timer{
    public:
    static long int timestamp(){
        return time(NULL);
    }

    static uint32_t usecs(){
        struct timeval  tv;
        struct timezone tz;
        struct tm      *tm;
        uint32_t         usecs;

        gettimeofday(&tv, &tz);
        tm = localtime(&tv.tv_sec);

        usecs = tm->tm_hour * 3600 * 1000 * 1000 + tm->tm_min * 60 * 1000 * 1000+
            tm->tm_sec * 1000 * 1000 + tv.tv_usec;
        return usecs;
    }

    static uint32_t msecs(){
            struct timeval  tv;
        struct timezone tz;
        struct tm      *tm;
        uint32_t         usecs;

        gettimeofday(&tv, &tz);
        tm = localtime(&tv.tv_sec);

        usecs = tm->tm_hour * 3600 * 1000 + tm->tm_min * 60 * 1000 +
            tm->tm_sec * 1000 + tv.tv_usec / 1000;
        return usecs/1000;
    }
};

#endif
