LIB=libdrpc.a
SRC=$(wildcard src/*.cc)
OBJ=$(patsubst %.cc,%.o,$(SRC))

CXX=clang++
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

%.o: %.cc
	$(CXX) -c $(CXXFLAGS) -o $@ $<

