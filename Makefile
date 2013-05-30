NAME = test
CC = g++
LIBS = -lpthread
FLAGS = -Wall
OPTI = # -O2

memslab.o: memslab.cpp memslab.hpp
	$(CC) -c $^ $(LIBS) $(FLAGS) $(OPTI)

mutex.o: mutex.cpp mutex.h
	$(CC) -c $^ $(LIBS) $(FLAGS) $(OPTI)

hash.o: hash.cpp hash.hpp
	$(CC) -c $^ $(FLAGS) $(OPTI)

#all: test.o hash.o assoc.o slabs.o
#	$(CC) -o $(NAME) $^ $(LIBS) $(FLAGS) $(OPTI)

#test.o: test.cpp
#	$(CC) -c $^ $(LIBS) $(FLAGS)

#assoc.o: assoc.cpp assoc.hpp
#	$(CC) -c $^ $(LIBS) $(FLAGS) $(OPTI)

#slabs.o: slabs.cpp slabs.hpp
#	$(CC) -c $^ $(LIBS) $(FLAGS) $(OPTI)

#items.o: items.cpp items.hpp
#	$(CC) -c $^ $(LIBS) $(FLAGS) $(OPTI)

#hash.o: hash.cpp hash.hpp
#	$(CC) -c $^ $(FLAGS) $(OPTI)

#mutex.o: mutex.cpp mutex.h
#	$(CC) -c $^ $(LIBS) $(FLAGS) $(OPTI)


clean:
	rm -rf *.o *.gch

