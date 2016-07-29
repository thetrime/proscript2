proscript:	kernel.o parser.o constants.o ctable.o stream.o hashmap.o main.o compiler.o bihashmap.o crc.o list.o operators.o prolog_flag.o errors.o whashmap.o
		gcc -g $^ -o $@

%.o:		src/%.c src/constants src/instructions
		gcc -g -DDEBUG -c $< -o $@

clean:
		rm -f *.o
