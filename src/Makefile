NAME = test
CC = g++
LIBS = -lpthread
FLAGS = -Wall
OPTI = # -O2

#all: memslab.o mutex.o hash.o defines.h
#	$(CC) -o $(NAME) $^ $(LIBS) $(FLAGS) $(OPTI)

memslab.o: memslab.cpp memslab.hpp
	$(CC) -c $^ $(LIBS) $(FLAGS) $(OPTI)

mutex.o: mutex.cpp mutex.hpp
	$(CC) -c $^ $(LIBS) $(FLAGS) $(OPTI)

hash.o: hash.cpp hash.hpp
	$(CC) -c $^ $(FLAGS) $(OPTI)


clean:
	rm -rf *.o *.gch

