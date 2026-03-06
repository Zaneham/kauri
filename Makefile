CC      = gcc
CFLAGS  = -std=c99 -pedantic -Werror -Wall -Wextra -Wshadow \
          -Wstrict-prototypes -Wmissing-prototypes -Wformat=2 \
          -Wconversion -Wold-style-definition -DKAURI_DEBUG=1

EXAMPLES = examples/mini_lex examples/mini_pool examples/mini_ast

.PHONY: test examples clean

test: tests/test_kauri
	./tests/test_kauri

examples: $(EXAMPLES)

examples/mini_lex: examples/mini_lex.c kauri.h
	$(CC) $(CFLAGS) -o $@ $<

examples/mini_pool: examples/mini_pool.c kauri.h
	$(CC) $(CFLAGS) -o $@ $<

examples/mini_ast: examples/mini_ast.c kauri.h
	$(CC) $(CFLAGS) -o $@ $<

tests/test_kauri: tests/test_kauri.c kauri.h
	$(CC) $(CFLAGS) -o $@ $<

clean:
	rm -f tests/test_kauri tests/test_kauri.exe $(EXAMPLES) $(addsuffix .exe,$(EXAMPLES))
