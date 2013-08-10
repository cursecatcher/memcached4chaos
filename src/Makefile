SERVER_NAME = zmqcache
CLIENT_NAME = zmqclient

CC = g++
LIBS = -lpthread -I /opt/include/ -L /opt/lib/  -lzmq
OPT = -O2
WFLAGS = -Wall -Wextra -pedantic
FLAGS = $(LIBS) $(OPT) $(WFLAGS)


all:
	make --directory=cache/
	make $(SERVER_NAME) $(CLIENT_NAME)
#	make $(CLIENT_NAME)

$(SERVER_NAME): zmqcache.cpp cache/*.o
	$(CC) -o $@ $^ $(FLAGS)

$(CLIENT_NAME): zmqclient.cpp reqrep.hpp
	$(CC) -o $@ $^ $(FLAGS)


clean:
	rm -f cache/*.o cache/*.gch

cleanall:
	make clean && rm -f $(CLIENT_NAME) $(SERVER_NAME)
