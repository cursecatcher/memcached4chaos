NAME = datacache
CC = g++
LIBS = -lpthread
FLAGS = -O2 -Wall -Wextra -pedantic

all: main.cpp datacache.o slabs.o assoc.o lru.o hash.o items_op.hpp const_types.h
	$(CC) -o $(NAME) $^ $(LIBS) $(FLAGS)

datacache.o: datacache.cpp datacache.hpp
	$(CC) -c $^ $(LIBS) $(FLAGS)

slabs.o: slabs.cpp slabs.hpp
	$(CC) -c $^ $(LIBS) $(FLAGS)

assoc.o: assoc.cpp assoc.hpp
	$(CC) -c $^ $(LIBS) $(FLAGS)

lru.o: lru.cpp lru.hpp
	$(CC) -c $^ $(LIBS) $(FLAGS)

hash.o: hash.c hash.h
	$(CC) -c $^ $(LIBS) $(FLAGS)

clean:
	rm -f *.gch *.o
