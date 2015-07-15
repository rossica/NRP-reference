CC=g++
DEBUG=-g
CXXFLAGS=-std=c++11 -Wall -pedantic $(DEBUG)
LFLAGS=-Wall $(DEBUG)

all:
	$(CC) $(CXXFLAGS) main.cpp config.h config.cpp server.h server.cpp client.h client.cpp -o nrpd

clean:
	rm *.o nrpd
