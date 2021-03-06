LIB=libdrpc.a
OBJ=$(patsubst %.c,%.o,$(wildcard src/*.c))

CC=cc
CPPFLAGS=-D_POSIX_SOURCE -D_C99_SOURCE
CFLAGS=-std=c11 $(CPPFLAGS) -g -Wall -fPIC -Iinclude
AR=ar
ARFLAGS=rc

all: $(LIB)

.PHONY: clean
clean:
	@rm -f $(LIB) $(OBJ)
	@make -C test clean

rebuild: clean all

test: all
	@make -C test all

$(LIB): $(OBJ)
	@echo "AR $@"
	@$(AR) $(ARFLAGS) $@ $^

%.o: %.c
	@echo "CC $<"
	@$(CC) -c $(CFLAGS) -o $@ $<

