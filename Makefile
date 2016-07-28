proscript:	kernel.o parser.o constants.o ctable.o stream.o hashmap.o main.o compiler.o
		gcc $^ -o $@

%.o:		src/%.c
		gcc -c $< -o $@
