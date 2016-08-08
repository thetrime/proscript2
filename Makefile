ARCH=c
#ARCH=js

FILESYSTEM=--embed-file test.pl --embed-file src/builtin.pl --embed-file tests

ifeq ($(ARCH),js)
CC=emcc
TARGET=build/proscript.js
#CFLAGS=-O2
OPT_LEVEL=-O3
MEMORY_REQUIRED=-s TOTAL_MEMORY=134217738
#MEMORY_REQUIRED=-s ALLOW_MEMORY_GROWTH=1
CFLAGS=$(OPT_LEVEL) -s NO_EXIT_RUNTIME=1 $(MEMORY_REQUIRED) -Ilib/gmp.js -s ASSERTIONS=2 -Werror
LDFLAGS=$(OPT_LEVEL) -Llib/gmp.js/.libs --llvm-lto 1 -s ASM_JS=1 $(MEMORY_REQUIRED) -s ASSERTIONS=2
BOOT=--pre-js $(BOOTFILE)
BOOTFILE=src/pre.js
BOOT_FS=$(FILESYSTEM)
else
CC=gcc
TARGET=build/proscript
CFLAGS=-g -I/opt/local/include
LDFLAGS=-L/opt/local/lib -lgmp
BOOTFILE=main.o
BOOT=main.o
endif

OBJECTS=kernel.o parser.o constants.o ctable.o stream.o hashmap.o test.o compiler.o bihashmap.o crc.o list.o operators.o prolog_flag.o errors.o whashmap.o module.o init.o foreign.o arithmetic.o options.o char_conversion.o term_writer.o record.o string_builder.o

$(TARGET):	$(OBJECTS) $(BOOTFILE)
		$(CC) $(OBJECTS) $(BOOT) $(BOOT_FS) $(LDFLAGS) -o $@

foreign.o:	src/foreign.c src/foreign_impl.c
		$(CC) $(CFLAGS) -DDEBUG -c $< -o $@

%.o:		src/%.c src/constants src/instructions
		$(CC) $(CFLAGS) -DDEBUG -c $< -o $@

clean:
		rm -f *.o
