# Host-side tests for the MemCard PRO / FlipperMCE code. Needs a native gcc.
CC	:= gcc
CFLAGS	:= -g -std=gnu99 -Wall -Wextra -Wno-unused-parameter -Wno-unused-value \
	   -fsanitize=address,undefined -fno-sanitize-recover=undefined \
	   -Imock -I../include

all: test_memcardpro
	./test_memcardpro

test_memcardpro: test_memcardpro.c ../source/MemCardPro.c ../include/MemCardPro.h
	$(CC) $(CFLAGS) -o $@ test_memcardpro.c ../source/MemCardPro.c

clean:
	rm -f test_memcardpro

.PHONY: all clean
