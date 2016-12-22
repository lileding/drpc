LIB=libdrpc.a
OBJ=$(patsubst %.c,%.o,$(wildcard src/*.c))

CC=clang
#CPPFLAGS=-DDRPC_DEBUG
CFLAGS=-std=c11 $(CPPFLAGS) -g -Wall -fPIC -Iinclude
AR=ar
ARFLAGS=rc

all: $(LIB)

.PHONY: clean
clean:
	rm -f $(LIB) $(OBJ)
	make -C test clean

rebuild: clean all

test: all
	make -C test all

$(LIB): $(OBJ)
	$(AR) $(ARFLAGS) $@ $^

%.o: %.c
	$(CC) -c $(CFLAGS) -o $@ $<

