CC      = gcc
CFLAGS  = -std=c99 -pedantic -Werror -Wall -Wextra -Wshadow \
          -Wstrict-prototypes -Wmissing-prototypes -Wformat=2 \
          -Wconversion -Wold-style-definition -DKAURI_DEBUG=1

.PHONY: test clean

test: tests/test_kauri
	./tests/test_kauri

tests/test_kauri: tests/test_kauri.c kauri.h
	$(CC) $(CFLAGS) -o $@ $<

clean:
	rm -f tests/test_kauri tests/test_kauri.exe
