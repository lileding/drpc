APP=drpc
SRC=$(wildcard *.c)
OBJ=$(patsubst %.c,%.o,$(SRC))

CC=clang
CFLAGS=-std=c11 -g -Wall -fPIC -I../include
LD=clang
LDFLAGS=-lpthread

all: $(APP)

.PHONY: clean
clean:
	rm -f $(APP) $(OBJ)

rebuild: clean all

test: all
	./drpc

$(APP): $(OBJ) ../libdrpc.a
	$(LD) -o $@ $^ $(LDFLAGS)

%.o: %.c
	$(CC) -c $(CFLAGS) -o $@ $<

