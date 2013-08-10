-------------------------------------------------
|	      FRANCESCO IESU 45595		|
|	       "Balancer" Dev Log		|
-------------------------------------------------


TO DO
=================================================
GLOBAL:
Change current "CLIENT" to "SETTER" and make a separate sClient clone for handling GETTER instances from a separate component
Changed all fullIpAddress arrays to std::vector to prevent a bug indicating a "free error"... let's see if it does the trick

BALANCER:
[ ] Use Messagepack for serialization of messages ( int data[] + long int timestamp[] )
[ ] Change the message sent to CLIENTs when a SERVER reconnection occurs: it has to be a timestamp telling each CLIENT the exact time of MIGRATION back to its primary SERVER
[ ] Create getStatusUpdate(int socket) function to request a status update from a SPECIFIC SERVER
[ ] Separate the "wait for new Client/Server connection" loop and the "wait for user command" into 2 threads
    At the moment "wait for command" will be a listener that simply waits for the user to trigger a statusUpdateRequest from a Server in the serverList. In the final state of the project, the request will be automatically sent when a topological change occurs.
[ ] Evaluate the possibility of creating a thread for each connection, to be able to listen for arbitrary messages from bClients (arbitrary serverStatus updates, intentional disconnection messages etc.. )

SERVER:
[ ] Modify the package with 
[ ] Send to CLIENT the full balancedList converted to a Vector of struct fullIpAddress
	Handshake: 
	1) BALANCER sends "int balancedServerSize" to CLIENT
	2) CLIENT sends ACK
	3) BALANCER sends balancedList (Vector)

CLIENT
[x] Create Setter Object
[ ] Create Getter Object
[ ] Differentiate between GETTER and SETTER, (GETTER must receive packages from SERVER)





DEV LOG
=================================================

2012 11 01
-------------------------------------------------
GLOBAL:
converted "struct fullIpAddress" to "class fullIpAddress", generating 2 files (fullIpAddress.h and .cpp).
This was needed for Messagepack class seralization (not possibile with struct).

BALANCER:
corrected all the code to work with new class

SERVER:
Up to date

SETTER:
Tried to implement the use of fullIpAddress class and its serialization: problems with its deserialization.


2012 10 29
-------------------------------------------------
GLOBAL:
Overwritten Client with Setter

BALANCER:

SERVER:

SETTER



2012 08 23
-------------------------------------------------
BALANCER:
[x] BUG fix: after a SERVER reconnection getStatusUpdate must receive correct SERVER data, instead keeps giving the old avgLoad and bandwidth values
[x] optimised SERVER disconnection/reconnection managing its OFFLINE status and re-acquiring status updates on reconnection
[x] Handled SERVER disconnection and reconnection with MIGRATION of all CLIENTS that have it as primary SERVER

SERVER:
[x] BUG fix: SERVER entered an infinite loop if BALANCER disconnected abruptly
[x] BUG fix: SERVER entered an infinite loop if CLIENT disconnected abruptly
*NOTE: cause of both infinite loops was the absence of MSG_NOSIGNAL flag in the recv() call, causing it to return a SIGNAL instead of -1 when no message from CLIENT was received. Also after each error (TCP_TIMEOUT to do) the relative socket is definitely closed, to permit the other threads to keep listening and promting the output.

CLIENT:
[x] Handled MIGRATION to primary SERVER after its disconnection and reconnection

2012 08 22
-------------------------------------------------
BALANCER:
[x] Create a Thread that  periodically calls for a SERVER status update, in order to know when a SERVER has gone offline.
[x] Create a Thread that listens for user commands:
	'r' -> get statusUpdate and print all lists: serverList (sorted) ; clientList (name@ip) ; CLIENT->balancedList[]
	's' -> get statusUpdate and print just serverList
[x] Implement the use of struct bClientStatus.type field also for SERVER: it takes the values OFFLINE or ONLINE
[x] Manage the disconnection and reconnection of a SERVER:
 	Disconnection: mark the server as offline closing its socket and assigning value -1 to bCLientStatus.csock field in serverList
	Reconnection: 	instead of pushing the new SERVER into serverList, search for a previous identical "ip:port" entry
			if we get a match, simply edit csock field in serverList item.
2012 08 21
-------------------------------------------------
BALANCER:
[x] Balance lists according to Bandwidth and avgLoad


2012 08 18
-------------------------------------------------
BALANCER:
[x] Implemented BalanceServerList with PIVOTING on first server (preferred server) of every CLIENT
    Rebalancing resizes (enlarges) each bClientStatus.list in case of a new SERVER connection


2012 08 16
-------------------------------------------------
BALANCER:

[x] Send a balancedList (array of struct fullIpAddress) to CLIENTs
[x] balancedList is a dynamically allocated array of "struct fullIpAddress { char ip[16]; int port; }"
    This array is sent to every CLIENT in bClientList, wich will receive it as follows:
    sClient has a bClient object that establishes a connection to BALANCER and then runs a thread that listens for balancedList updates. 
    This thread is in recv() waiting for a sequence of 2 messages ( a manual ACK is sent between the two messages for further security):
    1) CLIENT: recv(balancerSck, &balancedListSize, sizeof(int),0) 
	it's the size needed for malloc the balancedList array. 
	balancedList = (struct fullIpAddress*)malloc(sizeof(struct fullIpAddress)*balancedListSize);
    2) CLIENT: sends ACK
    3) CLIENT: recv(balancerSck, balancedList, sizeof(struct fullIpAddress)*balancedListSize,0)
	Receive balancedList.

CLIENT:
[x] Get balancedList from Balancer and connect to first Server. iT's an array because it will give a better performance since the CLIENT just iterates through serverList
[x] FAILOVER - first implementation: 
    1) During CLIENT->SERVER Connection:
       If a SERVER from balancedList is actually gone offline, attempt connection to the next one, and so on until we reach the end of balancedList.
    2) During send():
       If the SERVER we're connected to is not in recv() anymore, we retry for "maxSendAttempts" times, then we connect to next server.
NOTE: To get the send() error (send returns -1), send is used with the MSG_NOSIGNAL flag. In this case we can handle a blank send ( send() returns 0) with a retry, without considering it a real error. Further considerations can be done, i.e. counting the "blank sends" and considering a failover when their number reaches a given maxPkgLoss value.


2012 08 15
-------------------------------------------------

SERVER:
[x] Create thread to calculate bandwidth: Infinite loop that calls sServer::calculateBandwidth() every second
[x] Create public function getStatus(), which will be called by Balancer to get SERVER's avgLoad and bandwidth values


2012 08 14
-------------------------------------------------

SERVER: 
[x] Receive Greeting Message from CLIENT with info about its pkgSize and required bandwidth

CLIENT:
[x] Pass pkgSize in 3 arbitrary units:
	-uf (Upload Frequency, in Hz)
	-ui ( Upload Interval in msec = 1000/pkgFreq)
	-mps (Messages Per Second = 1/pkgFreq )
[x] Implemented  Greeting section with SERVER to communicate Client Type (GETTER or SETTER) MessageSize and message frequency --> bandwidth
[x] Command line parameters can be given in any order via prefix ( "-n" <name> -t <type> etc... )
[x] Added command line parameters package_size and upload_interval to parser --> every client can initialise its own message size and frequency
[x] Message upload rate can be set in 3 different ways:
    "-ui"  = upload_interval_par; // message upload interval in milliseconds
    "-uf"  = upload_frequency_par; // message upload frequency in Hz
    "-mps" = upload_mps_par; // messages per second


2012 08 13
-------------------------------------------------

BALANCER:

[x] Greeting section with "swicth case" for handling the 2 bClient macro categories (SERVER - GETTER/SETTER)
    1) If a SERVER greetingMsg arrives, it's added to serverList
    2) If a GETTER/SETTER greetingMsg arrives, send an acknowloedgement message containing the last SERVER connected

CLIENT:
[x] Implemented  Greeting section with Balancer to get SERVER optimal parameters, at the moment receiving only 1 SERVER ip (the last online).
[x] Created parser to acquire command line parameters in any order with tag-style "-n <name>"


2012 08 12
-------------------------------------------------

BALANCER:
[x] Create a struct to carry the bClient's Greeting Message (client port, avgLoad and bandwidth)
    It's the same struct for all bClient instances (CLIENT and SERVER).

[x] Create a struct to save each bClientStatus, it contains the fields sent by bClients, plus 3 fields:
    int csock; // holds bServer socket number fot this connection
    char ip[16]; // holds the ip address of bClient machine

[x] Create getStatusUpdate() function to request a status update from all SERVERs



2012 08 08
-------------------------------------------------

SERVER:
[x] Changed Client structure from:
	main.cpp
	|_ bClient --> connect to BALANCER
	|_ sClient --> send msg to SERVER
    To:
	main.cpp
	|_ sClient --> after connection to BALANCER, send msg to SERVER
	   |_ bClient --> connect to BALANCER
    Now bClient object (running in a thread) is inside sClient object. Before they were 2 independent objects, making impossible for bClient to access sClient attributes.



2012 08 06
-------------------------------------------------

SERVER:
[x] Changed Server structure from:
	main.cpp
	|_ bClient --> connect to BALANCER
	|_ bServer --> receive msg from CLIENTs
    To:
	main.cpp
	|_ bServer --> after connection to BALANCER, receive msg from CLIENTs
	   |_ bClient --> connect to BALANCER
    Now bClient object (running in a thread) is inside bServer object. Before they were 2 independent objects, making impossible for bServer to pass bClient the clientList for sending it to BALANCER when a statusUpdate request arrived.



2012 08 04
-------------------------------------------------

BALANCER:
[x] BUG: when a new ip is added to a List (both Server and Client) looks all the list members get the value of the last ip
	 the issue was due to the use of list<char*> clientList: before pushing the new item, we simply chenged the value pointed by 

SERVER:
[x] Bug Corrected: when a new ip is added to a List (both Server and Client) looks all the list members get the value of the last ip
	 the issue was due to the use of list<char*> clientList: before pushing the new item, we simply chenged the value pointed by 



2012 08 03
-------------------------------------------------

SERVER:
[x] added command line balancer_ip, needed to make possibile launching servers in remote machines
[x] modified static function SocketHandler:
the old implementation used "pthread_create(&thread_id,0,&SocketHandler, (void*)csock );" , but it was necessary to access every attribute of the object from within the thread, so the thread creation has been changed to "pthread_create(&thread_id,0,&SocketHandler, this );", passing a pointer to the object itself.
An int* csockSafeCopy; has been added to allow the thread to receive the csock value before it's reallocated in the next cycle.
Now we can access all the necessary attributes, so the next step will be the implementation of 2 functions to calculate avgLoad and bandwidth.
[x] Create functions to calculate avgLoad
[x] protect access to avgLoad and bandwidth with mutexes and refresh their values at every cycle



2012 08 02
-------------------------------------------------

CLIENT: 
[x] Loop of messages (struct balancerPkg) with msg field of random length up to MSG_LEN.
[x] At the moment the size of struct balancerPkg.msg is fixed to a char[MSG_LEN], the randomization affects only the string length, extracting a random position for the '\0' character.
[x] Displays a sent message status "[%d B] - timestamp" at every cycle.

SERVER:
[x] Each thread displays the size of incoming message ( as said before the size is only virtual at the moment )

