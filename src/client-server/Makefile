SERVER_NAME = zmqserver
CLIENT_NAME = zmqclient

CC = g++
LIBS = -lpthread -I /opt/include/ -L /opt/lib/  -lzmq
OPT = -O2
WFLAGS = -Wall -Wextra -pedantic
FLAGS = $(LIBS) $(OPT) $(WFLAGS)


all:
	make --directory=../cache/
	make $(SERVER_NAME) $(CLIENT_NAME)


$(SERVER_NAME): $(SERVER_NAME).o ../cache/*.o
	$(CC) -o $@ $^ $(FLAGS)

$(CLIENT_NAME): $(CLIENT_NAME).o reqrep.hpp
	$(CC) -o $@ $^ $(FLAGS)


$(SERVER_NAME).o: $(SERVER_NAME).cpp $(SERVER_NAME).hpp
	$(CC) -c $^ $(FLAGS)

$(CLIENT_NAME).o: $(CLIENT_NAME).cpp $(CLIENT_NAME).hpp
	$(CC) -c $^ $(FLAGS)


clean:
	rm -f *.o *.gch

cleanall:
	make clean && rm -f $(CLIENT_NAME) $(SERVER_NAME)
