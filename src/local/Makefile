NAME = localcache

CC = g++
LIBS = -lpthread
OPT = -O2
WFLAGS = -Wall -Wextra -pedantic
FLAGS = $(LIBS) $(OPT) $(WFLAGS)
CACHE_DIR = ../cache/


all:
	make --directory=$(CACHE_DIR)
	make $(NAME)


$(NAME): main.o threadpool.o ../cache/*.o
	$(CC) -o $@ $^ $(FLAGS)

main.o: main.cpp
	$(CC) -c $^ $(FLAGS)

threadpool.o: threadpool.cpp threadpool.hpp
	$(CC) -c $^ $(FLAGS)


clean:
	rm -f *.o *.gch

cleanall:
	make clean && rm -f $(NAME)
