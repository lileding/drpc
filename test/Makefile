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
	@rm -f $(APP) $(OBJ)

rebuild: clean all

$(APP): $(OBJ) ../libdrpc.a
	@echo "LD $@"
	@$(LD) -o $@ $^ $(LDFLAGS)

%.o: %.cc
	@echo "CXX $<"
	@$(CXX) -c $(CXXFLAGS) -o $@ $<

