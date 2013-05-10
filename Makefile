NAME = test
CC = g++
LIBS = -lpthread

all: test.o hash.o assoc.o
	$(CC) -o $(NAME) $^ $(LIBS)

test.o: test.cpp
	$(CC) -c $^ $(LIBS)

assoc.o: assoc.cpp assoc.hpp
	$(CC) -c $^ $(LIBS)

hash.o: hash.cpp hash.hpp
	$(CC) -c $^

clean: 
	rm -rf *.o *.gch 
	
