NAME = memcached
CC = g++
LIBS = -lpthread
FLAGS =

all: engine.o slabs.o assoc.o lru.o
	$(CC) -o $(NAME) $^ $(LIBS) $(FLAGS)

engine.o: engine.cpp engine.hpp
	$(CC) -c $^ $(LIBS) $(FLAGS)

slabs.o: slabs.cpp slabs.hpp
	$(CC) -c $^ $(LIBS) $(FLAGS)

assoc.o: assoc.cpp assoc.hpp
	$(CC) -c $^ $(LIBS) $(FLAGS)

lru.o: lru.cpp lru.hpp
	$(CC) -c $^ $(LIBS) $(FLAGS)

clean:
	rm -f *.gch *.o
