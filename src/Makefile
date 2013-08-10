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


$(SERVER_NAME): zmqcache.o cache/*.o
	$(CC) -o $@ $^ $(FLAGS)

$(CLIENT_NAME): zmqclient.o reqrep.hpp
	$(CC) -o $@ $^ $(FLAGS)


zmqcache.o: zmqcache.cpp zmqcache.hpp
	$(CC) -c $^ $(FLAGS)

zmqclient.o: zmqclient.cpp zmqclient.hpp
	$(CC) -c $^ $(FLAGS)


clean:
	rm -f *.o *.gch && make --directory=cache/ clean

cleanall:
	make clean && rm -f $(CLIENT_NAME) $(SERVER_NAME)
