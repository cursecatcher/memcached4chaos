NAME = cachetest

CC = g++
LIBS = -lpthread
#OPT = -O2
#WFLAGS = -Wall -Wextra -pedantic
FLAGS = $(LIBS) $(OPT) $(WFLAGS)
CACHE_DIR = ../cache/


all:
	make --directory=$(CACHE_DIR)
	make $(NAME)


$(NAME): testing.o ../cache/*.o
	$(CC) -o $@ $^ $(FLAGS)

testing.o: testing.cpp
	$(CC) -c $^ $(FLAGS)


clean:
	rm -f *.o *.gch

cleanall:
	make clean && rm -f $(NAME)
