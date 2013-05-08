NAME = test
CC = g++
LIBS = -lpthread

all: test.o assoc.o
	$(CC) -o $(NAME) $^ $(LIBS)

test.o: test.cpp
	$(CC) -c $^ $(LIBS)

assoc.o: assoc.cpp assoc.hpp
	$(CC) -c $^ $(LIBS)

clean: 
	rm -rf *.o *.gch 
	
