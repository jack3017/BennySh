OPS = -g -Wall -Wshadow -Wunreachable-code -Wredundant-decls -Wmissing-declarations -Wold-style-definition -Wmissing-prototypes -Wdeclaration-after-statement -Wno-return-local-addr -Wuninitialized -Wextra -Wunused

all: BennySh 

BennySh: BennySh.o
	gcc $(OPS) -o BennySh BennySh.o

BennySh.o: BennySh.c
	gcc $(OPS) -c BennySh.c

make clean:
	rm -f reclookup bigFiles BennySh *.o *~ \#*
