YACC=yacc
LEX=flex
CC=cc -D_GNU_SOURCE
CFLAGS= -g -Wall

all: 120++

.c.o:
	$(CC) -c $(CFLAGS) $<

120++:  120++lex.o 120++gram.o tree.o symt.o token.o hasht.o main.o codegen.o list.o logger.o type.o scope.o rules.o node.o final.o
	cc -o 120++ 120++lex.o 120++gram.o tree.o symt.o token.o hasht.o main.o codegen.o list.o logger.o type.o scope.o rules.o node.o final.o


120++gram.c 120++gram.h: 120++gram.y
	$(YACC) -dt --verbose 120++gram.y
	mv -f y.tab.c 120++gram.c
	mv -f y.tab.h 120++gram.h

120++lex.c: 120++lex.l
	$(LEX) -t 120++lex.l >120++lex.c

120++lex.o: 120++gram.h

main.o: main.h

final.o: final.h

codegen.o: codegen.h

hasht.o: hasht.h

symt.o: symt.h

token.o: token.h

tree.o: tree.h

list.o: list.h

logger.o: logger.h

type.o: type.h

scope.o: scope.h

rules.o: rules.h

node.o: node.h

clean:
	rm -f 120++ *.o
	rm -f 120++lex.c 120++gram.c 120++gram.h