CC=g++
DEBUG=-g
CXXFLAGS=-std=c++11 -Wall -pedantic -fms-extensions $(DEBUG)
LFLAGS=-Wall $(DEBUG)


default: nrpd

nrpd:	protocol.o config.o server.o client.o main.o
	$(CC) $(LFLAGS) -o bin/nrpd obj/protocol.o obj/server.o obj/client.o obj/config.o obj/main.o

protocol.o:  protocol.cpp protocol.h
	$(CC) $(CXXFLAGS) -c protocol.cpp -o obj/protocol.o

config.o:  config.cpp config.h
	$(CC) $(CXXFLAGS) -c config.cpp -o obj/config.o

server.o:  server.cpp server.h protocol.h
	$(CC) $(CXXFLAGS) -c server.cpp -o obj/server.o

client.o:  client.cpp client.h protocol.h
	$(CC) $(CXXFLAGS) -c client.cpp -o obj/client.o

main.o:  main.cpp server.h config.h
	$(CC) $(CXXFLAGS) -c main.cpp -o obj/main.o

test:  protocol.o 
	$(CC) $(CXXFLAGS) test/main.cpp test/functest.cpp obj/protocol.o -o bin/testnrpd

clean:
	rm -f obj/*.o bin/nrpd bin/testnrpd
