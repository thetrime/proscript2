#ARCH=c
ARCH?=js

FLAGS=
WASM=-Oz

STRICT_ISO?=no
ifeq ($(STRICT_ISO),yes)
	FLAGS+=-DSTRICT_ISO
endif


BASIC_FILESYSTEM=--embed-file src/builtin.pl
TESTING_FILESYSTEM=--embed-file src/builtin.pl --embed-file test.pl --embed-file tests

ifeq ($(ARCH),js)
CC=emcc
TARGET=proscript.js
OPT_LEVEL=-O3
MEMORY_REQUIRED=-s ALLOW_MEMORY_GROWTH=1
GMP=node_modules/gmpjs
CFLAGS=$(OPT_LEVEL) -s NO_EXIT_RUNTIME=1 $(MEMORY_REQUIRED) -I$(GMP) -s ASSERTIONS=2 -Werror -DDEBUG -DMEMTRACE $(FLAGS) $(WASM)
LDFLAGS=$(OPT_LEVEL) --llvm-lto 1 -s ASM_JS=1 $(MEMORY_REQUIRED) -s ASSERTIONS=2 $(GMP)/.libs/libgmp.a $(WASM)
BOOT=--pre-js $(BOOTFILE)
BOOTFILE=src/pre.js
CHECK=node test.js
else
CC=gcc
TARGET=proscript
CFLAGS=-g -I/opt/local/include $(FLAGS)
LDFLAGS=-L/opt/local/lib -lgmp -lm
BOOTFILE=main.o
BOOT=main.o
CHECK=./proscript
endif

OBJECTS=kernel.o parser.o constants.o ctable.o stream.o hashmap.o test.o compiler.o bihashmap.o crc.o list.o operators.o prolog_flag.o errors.o whashmap.o module.o init.o foreign.o format.o arithmetic.o options.o char_conversion.o term_writer.o record.o string_builder.o fli.o char_buffer.o global.o

$(TARGET):	$(OBJECTS) $(BOOTFILE) $(GMP)
		$(CC) $(OBJECTS) $(BOOT) $(BASIC_FILESYSTEM) $(LDFLAGS) -o $@

proscript_standalone.js: $(OBJECTS) $(BOOTFILE) $(GMP) test.pl
		$(CC) $(OBJECTS) $(BOOT) $(TESTING_FILESYSTEM) $(LDFLAGS) -o proscript.js
		NODEPATH=. node_modules/browserify/bin/cmd.js --standalone Proscript ./proscript.js > proscript_standalone.js

kernel.o:	src/kernel.c src/constants src/instructions src/builtin.h $(GMP)
		$(CC) $(CFLAGS) -c $< -o $@

src/builtin.h:	src/builtin.pl
		xxd -i $< $@

foreign.o:	src/foreign.c src/foreign_impl.c $(GMP)
		$(CC) $(CFLAGS) -c $< -o $@

%.o:		src/%.c src/constants src/instructions $(GMP)
		$(CC) $(CFLAGS) -c $< -o $@

node_modules/gmpjs:
	npm install https://github.com/thetrime/gmpjs

clean:
		rm -f *.o
		rm -f proscript.js
		rm -f proscript.wasm

check: 	$(TARGET)
	$(CHECK)

# To check in a browser, run make proscript_standalone.js then visit test.html in a browser.
