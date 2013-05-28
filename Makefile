NAME = test
CC = g++
LIBS = -lpthread
FLAGS = -Wall

all: test.o hash.o assoc.o slabs.o 
	$(CC) -o $(NAME) $^ $(LIBS) $(FLAGS)

test.o: test.cpp
	$(CC) -c $^ $(LIBS) $(FLAGS)

assoc.o: assoc.cpp assoc.hpp
	$(CC) -c $^ $(LIBS) $(FLAGS)

slabs.o: slabs.cpp slabs.hpp
	$(CC) -c $^ $(LIBS) $(FLAGS)

items.o: items.cpp items.hpp
	$(CC) -c $^ $(LIBS) $(FLAGS)

hash.o: hash.cpp hash.hpp
	$(CC) -c $^

clean: 
	rm -rf *.o *.gch 
	
