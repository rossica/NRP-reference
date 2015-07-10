all:
	g++ -Wall -g main.cpp config.h config.cpp -o nrpd

clean:
	rm *.o nrpd
