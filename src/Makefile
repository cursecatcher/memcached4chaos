NAME = memcached
CC = g++
LIBS = -lpthread
FLAGS = -Wall

#all: engine.o slabs.o assoc.o lru.o hash.o items_op.o const_types.h
#	$(CC) -o $(NAME) $^ $(LIBS) $(FLAGS)

engine.o: engine.cpp engine.hpp
	$(CC) -c $^ $(LIBS) $(FLAGS)

slabs.o: slabs.cpp slabs.hpp
	$(CC) -c $^ $(LIBS) $(FLAGS)

assoc.o: assoc.cpp assoc.hpp
	$(CC) -c $^ $(LIBS) $(FLAGS)

lru.o: lru.cpp lru.hpp
	$(CC) -c $^ $(LIBS) $(FLAGS)

hash.o: hash.c hash.h
	$(CC) -c $^ $(LIBS) $(FLAGS)

items_op.o: items_op.cpp items_op.hpp
	$(CC) -c $^ $(LIBS) $(FLAGS)

clean:
	rm -f *.gch *.o
