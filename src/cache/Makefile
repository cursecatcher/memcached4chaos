CC = g++
LIBS = -lpthread
OPT = -O2
WFLAGS = -Wall -Wextra -pedantic
FLAGS = $(LIBS) $(OPT) $(WFLAGS)


all: slabs.o assoc.o lru.o hash.o


slabs.o: slabs.cpp slabs.hpp
	$(CC) -c $^ $(FLAGS)

assoc.o: assoc.cpp assoc.hpp
	$(CC) -c $^ $(FLAGS)

lru.o: lru.cpp lru.hpp
	$(CC) -c $^ $(FLAGS)

hash.o: hash.c hash.h
	$(CC) -c $^ $(FLAGS)


clean:
	rm -f *.o *.gch
