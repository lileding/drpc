APP=drpc
SRC=$(wildcard *.cc)
OBJ=$(patsubst %.cc,%.o,$(SRC))

CC=clang
CFLAGS=-std=c11 -g -Wall -fPIC -I../include
CXX=clang++
CXXFLAGS=-std=c++11 -g -Wall -fPIC -I../include
LD=clang++
LDFLAGS=-lpthread

all: $(APP)

.PHONY: clean
clean:
	rm -f $(APP) $(OBJ)

rebuild: clean all

$(APP): $(OBJ) ../libdrpc.a
	$(LD) -o $@ $^ $(LDFLAGS)

%.o: %.c
	$(CC) -c $(CFLAGS) -o $@ $<

%.o: %.cc
	$(CXX) -c $(CXXFLAGS) -o $@ $<

