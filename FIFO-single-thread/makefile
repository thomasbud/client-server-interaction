# makefile

all: dataserver client

common.o: common.h common.cpp
	g++ -g -w -std=c++11 -c common.cpp

FIFORequestChannel.o: FIFORequestChannel.h FIFORequestChannel.cpp
	g++ -g -w -std=c++11 -c FIFORequestChannel.cpp

client: client.cpp FIFORequestChannel.o common.o
	g++ -g -w -std=c++11 -o client client.cpp FIFORequestChannel.o common.o -lpthread -lrt

dataserver: dataserver.cpp  FIFORequestChannel.o common.o
	g++ -g -w -std=c++11 -o dataserver dataserver.cpp FIFORequestChannel.o common.o -lpthread -lrt

clean:
	rm -rf *.o *.csv fifo* dataserver client data*_*
