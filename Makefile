#ARCH=c
ARCH?=js

FLAGS=

STRICT_ISO?=no
ifeq ($(STRICT_ISO),yes)
	FLAGS+=-DSTRICT_ISO
endif


FILESYSTEM=--embed-file src/builtin.pl
#FILESYSTEM=--embed-file src/builtin.pl --embed-file test.pl --embed-file tests

ifeq ($(ARCH),js)
CC=emcc
TARGET=proscript.js
OPT_LEVEL=-O3
MEMORY_REQUIRED=-s TOTAL_MEMORY=33554432
GMP=node_modules/gmpjs
CFLAGS=$(OPT_LEVEL) -s NO_EXIT_RUNTIME=1 $(MEMORY_REQUIRED) -I$(GMP) -s ASSERTIONS=2 -Werror -DDEBUG -DMEMTRACE $(FLAGS)
LDFLAGS=$(OPT_LEVEL) --llvm-lto 1 -s ASM_JS=1 $(MEMORY_REQUIRED) -s ASSERTIONS=2 $(GMP)/.libs/libgmp.a
BOOT=--pre-js $(BOOTFILE)
BOOTFILE=src/pre.js
BOOT_FS=$(FILESYSTEM)
else
CC=gcc
TARGET=proscript
CFLAGS=-g -I/opt/local/include $(FLAGS)
LDFLAGS=-L/opt/local/lib -lgmp
BOOTFILE=main.o
BOOT=main.o
endif

OBJECTS=kernel.o parser.o constants.o ctable.o stream.o hashmap.o test.o compiler.o bihashmap.o crc.o list.o operators.o prolog_flag.o errors.o whashmap.o module.o init.o foreign.o format.o arithmetic.o options.o char_conversion.o term_writer.o record.o string_builder.o fli.o char_buffer.o global.o

$(TARGET):	$(OBJECTS) $(BOOTFILE)
		$(CC) $(OBJECTS) $(BOOT) $(BOOT_FS) $(LDFLAGS) -o $@

foreign.o:	src/foreign.c src/foreign_impl.c
		$(CC) $(CFLAGS) -c $< -o $@

%.o:		src/%.c src/constants src/instructions
		$(CC) $(CFLAGS) -c $< -o $@

clean:
		rm -f *.o
