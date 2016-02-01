CC=g++
DEBUG=-g
CXXFLAGS=-std=c++14 -Wall -fms-extensions -pipe $(DEBUG)
LFLAGS=-Wall $(DEBUG) -lpthread


all: nrpd

nrpd:	protocol.o log.o config.o server.o client.o main.o
	$(CC) $(LFLAGS) -o bin/nrpd obj/protocol.o obj/log.o obj/server.o obj/client.o obj/config.o obj/main.o

protocol.o:  protocol.cpp protocol.h
	$(CC) $(CXXFLAGS) -c protocol.cpp -o obj/protocol.o

log.o: log.cpp log.h
	$(CC) $(CXXFLAGS) -c log.cpp -o obj/log.o

config.o:  config.cpp config.h log.h
	$(CC) $(CXXFLAGS) -c config.cpp -o obj/config.o

server.o:  server.cpp server.h protocol.h log.h
	$(CC) $(CXXFLAGS) -c server.cpp -o obj/server.o

client.o:  client.cpp client.h protocol.h log.h
	$(CC) $(CXXFLAGS) -c client.cpp -o obj/client.o

main.o:  main.cpp server.h config.h client.h log.h
	$(CC) $(CXXFLAGS) -c main.cpp -o obj/main.o

test:  protocol.o log.o config.o server.o
	$(CC) $(CXXFLAGS) test/main.cpp test/functest.cpp $(LFLAGS) obj/protocol.o obj/log.o obj/config.o obj/server.o -o bin/testnrpd

clean:
	rm -f obj/*.o bin/nrpd bin/testnrpd
