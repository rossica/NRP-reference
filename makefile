CC=g++
DEBUG=-g
CXXFLAGS=-std=c++11 -Wall -pedantic -fms-extensions $(DEBUG)
LFLAGS=-Wall $(DEBUG)

all:
	$(CC) $(CXXFLAGS) main.cpp protocol.cpp config.cpp server.cpp client.cpp -o nrpd

clean:
	rm *.o nrpd
