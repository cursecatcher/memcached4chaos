NAME = cache
CC = g++
LIBS = -lpthread
OPT = -O2
FLAGS = -Wall -Wextra -pedantic

zmqcache: zmqcache.cpp datacache.hpp lru.o assoc.o slabs.o hash.o const_types.h
	$(CC) -o zmqcache $^ $(OPT) $(LIBS) -lzmq $(FLAGS)

zmqclient: zmqclient.cpp reqrep.hpp
	$(CC) -o zmqclient $^ $(OPT) $(LIBS) -lzmq $(FLAGS)

#cache: main.o datacache.hpp threadpool.o lru.o assoc.o slabs.o hash.o const_types.h
#	$(CC) -o $(NAME) $^ $(OPT) $(LIBS) $(FLAGS)

main.o: main.cpp
	$(CC) -c $^ $(OPT) $(LIBS) $(FLAGS)

threadpool.o: threadpool.cpp threadpool.hpp
	$(CC) -c $^ $(OPT) $(LIBS) $(FLAGS)

slabs.o: slabs.cpp slabs.hpp
	$(CC) -c $^ $(OPT) $(LIBS) $(FLAGS)

assoc.o: assoc.cpp assoc.hpp
	$(CC) -c $^ $(OPT) $(LIBS) $(FLAGS)

lru.o: lru.cpp lru.hpp
	$(CC) -c $^ $(OPT) $(LIBS) $(FLAGS)

hash.o: hash.c hash.h
	$(CC) -c $^ $(OPT) $(LIBS) $(FLAGS)

clean:
	rm -f *.o *.gch

cleanall:
	rm -f *.o *.gch $(NAME)
