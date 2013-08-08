SERVER_NAME = zmqcache
CLIENT_NAME = zmqclient


CC = g++
LIBS = -lpthread -lzmq
OPT = -O2
WFLAGS = -Wall -Wextra -pedantic
FLAGS = $(OPT) $(WFLAGS)

all:
	make $(SERVER_NAME) && make $(CLIENT_NAME)

$(SERVER_NAME): zmqcache.cpp cache/datacache.hpp lru.o assoc.o slabs.o hash.o cache/const_types.h
	$(CC) -o $(SERVER_NAME) $^ $(LIBS) $(FLAGS)

$(CLIENT_NAME): zmqclient.cpp reqrep.hpp
	$(CC) -o $(CLIENT_NAME) $^ $(LIBS) $(FLAGS)


slabs.o: cache/slabs.cpp cache/slabs.hpp
	$(CC) -c $^ $(LIBS) $(FLAGS)

assoc.o: cache/assoc.cpp cache/assoc.hpp
	$(CC) -c $^ $(LIBS) $(FLAGS)

lru.o: cache/lru.cpp cache/lru.hpp
	$(CC) -c $^ $(LIBS) $(FLAGS)

hash.o: cache/hash.c cache/hash.h
	$(CC) -c $^ $(LIBS) $(FLAGS)


clean:
	rm -f *.o cache/*.gch

cleanall:
	make clean && rm -f $(CLIENT_NAME) $(SERVER_NAME)
