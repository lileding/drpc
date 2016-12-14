LIB=libdrpc.a
OBJ=$(patsubst %.cc,%.o,$(wildcard src/*.cc)) $(patsubst %.c,%.o,$(wildcard src/*.c))
OBJ=$(patsubst %.c,%.o,$(wildcard src/*.c))

CC=clang
CXX=clang++
CFLAGS=-std=c11 -g -Wall -fPIC -Iinclude
CXXFLAGS=-std=c++11 -g -Wall -fPIC -Iinclude
AR=ar
ARFLAGS=rc

all: $(LIB)

.PHONY: clean
clean:
	rm -f $(LIB) $(OBJ)
	make -C test clean

rebuild: clean all

test: all
	make -C test test

$(LIB): $(OBJ)
	$(AR) $(ARFLAGS) $@ $^

%.o: %.c
	$(CC) -c $(CFLAGS) -o $@ $<

%.o: %.cc
	$(CXX) -c $(CXXFLAGS) -o $@ $<

