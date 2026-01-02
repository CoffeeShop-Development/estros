PROGS=xm_cc

all:

build: $(PROGS)

test: build
	./xm_cc samples/j000.c
	./xm_cc samples/j001.c

clean:
	-rm *.o xm_cc

.PHONY: all build test clean

xm_cc: main.o lexer.o parser.o ssa.o asm.o ast.o util.o
	$(CC) $(CFLAGS) $^ -o $@

.o: .c
	$(CC) $(CFLAGS) -c $< -o $@
