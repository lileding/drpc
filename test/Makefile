APP=serpc
SRC=$(wildcard *.cc)
OBJ=$(patsubst %.cc,%.o,$(SRC))

CXX=clang++
CXXFLAGS=-std=c++11 -g -Wall -fPIC -I../include
LD=clang++
LDFLAGS=

all: $(APP)

.PHONY: clean
clean:
	rm -f $(APP) $(OBJ)

rebuild: clean all

test: all
	./serpc

$(APP): $(OBJ) ../libserpc.a
	$(LD) -o $@ $^ $(LDFLAGS)

%.o: %.cc
	$(CXX) -c $(CXXFLAGS) -o $@ $<

