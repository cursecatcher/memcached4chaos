CC = g++
NAME = poool
LIBS = -lpthread

all: thpool.o main.o
	$(CC) -o $(NAME) $^ $(LIBS)

thpool.o: thpool.cpp thpool.hpp
	$(CC) -c $^ $(LIBS)

main.o: main.cpp
	$(CC) -c $^ $(LIBS)

clean:
	rm -rf *.o *.gch
