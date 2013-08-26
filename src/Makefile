CC = g++
LIBS = -lpthread -I /opt/include/ -L /opt/lib/  -lzmq
OPT = -O2
WFLAGS = -Wall -Wextra -pedantic
FLAGS = $(LIBS) $(OPT) $(WFLAGS)


all:
	make --directory=local/
	make --directory=cache/
	make --directory=client-server/

clean:
	rm -f *.o *.gch 
	make --directory=cache/ clean 
	make --directory=local/ clean 
	make --directory=client-server clean
