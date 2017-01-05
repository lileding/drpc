CC=cc
CFLAGS=-std=c11 -g -Wall -fPIC -I../include
CXX=c++
CXXFLAGS=-std=c++11 -g -Wall -fPIC -I../include
LD=c++
LDFLAGS=-lpthread

all: server client

.PHONY: clean
clean:
	@rm -f client client.o server server.o

rebuild: clean all

server: server.o ../libdrpc.a
	@echo "LD $@"
	@$(LD) -o $@ $^ $(LDFLAGS)

client: client.o ../libdrpc.a
	@echo "LD $@"
	@$(LD) -o $@ $^ $(LDFLAGS)

%.o: %.c
	@echo "CC $<"
	@$(CC) -c $(CFLAGS) -o $@ $<

%.o: %.cc
	@echo "CXX $<"
	@$(CXX) -c $(CXXFLAGS) -o $@ $<

