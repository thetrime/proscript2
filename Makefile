CC=emcc
TARGET=build/proscript.js
#CFLAGS=-O2
CFLAGS=-g -s NO_EXIT_RUNTIME=1
BOOT=--pre-js $(BOOTFILE)
BOOTFILE=src/pre.js
OBJECTS=kernel.o parser.o constants.o ctable.o stream.o hashmap.o main.o compiler.o bihashmap.o crc.o list.o operators.o prolog_flag.o errors.o whashmap.o module.o init.o

$(TARGET):	$(OBJECTS) $(BOOTFILE)
		$(CC) $(CFLAGS) $(OBJECTS) $(BOOT) -o $@

%.o:		src/%.c src/constants src/instructions
		$(CC) $(CFLAGS) -DDEBUG -c $< -o $@

clean:
		rm -f *.o
