APP=drpc
SRC=$(wildcard *.cc)
OBJ=$(patsubst %.cc,%.o,$(SRC))

CXX=c++
CXXFLAGS=-std=c++11 -g -Wall -fPIC -I../include
LD=c++
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

