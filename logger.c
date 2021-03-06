

#include <errno.h>
#include <stdarg.h>
#include <stdio.h>

#include "logger.h"
#include "args.h"
#include "token.h"
#include "120++gram.h"
#include "120++lex.h"
#include "type.h"
#include "symt.h"

#include "list.h"
#include "tree.h"
#include "libs.h"
#include "rules.h"
#include "scope.h"
#include "hasht.h"
#include "node.h"

/* from main */
extern struct list *yyfiles;
extern int yylineno;
extern char* yytext;

/*
 * Print message to stderr. If debug, call perror. Exit failure.
 */
void log_error(const char *format, ...)
{
	va_list ap;
	va_start(ap, format);

	fprintf(stderr, "error: ");
	vfprintf(stderr, format, ap);
	fprintf(stderr, "\n");

	va_end(ap);

	if (arguments.debug && errno)
		perror("");

	exit(EXIT_FAILURE);
}

/*
 * If debug, print message to stderr. Continue.
 */
void log_debug(const char *format, ...)
{
	if (!arguments.debug)
		return;
	va_list ap;
	va_start(ap, format);
	fprintf(stderr, "debug: ");
	vfprintf(stderr, format, ap);
	fprintf(stderr, "\n");
	va_end(ap);
}

/*
 * Acts like assert.
 *
 * If errno is set, print it.
 */
void log_assert(bool p)
{
	if (p)
		return;

	fprintf(stderr, "assertion failed: probably received unexpected null\n");
	if (errno != 0)
		perror("");

	/* debug builds should allow the segfault for stack tracing */
	if (!arguments.debug)
		exit(EXIT_FAILURE);
}

/*
 * Prints error message and exits per assignment requirements.
 */
void log_unsupported()
{
	fprintf(stderr, "error: operation unsupported\n"
	        "file: %s\n" "line: %d\n" "token: %s\n",
                (const char *)list_back(yyfiles), yylineno, yytext);
	exit(3);
}

/*
 * Prints relevant information for lexical errors and exits returning 1
 * per assignment requirements.
 */
void log_lexical(const char *format, ...)
{
	va_list ap;
	va_start(ap, format);

	fprintf(stderr, "lexical error: ");
	vfprintf(stderr, format, ap);

	va_end(ap);

	fprintf(stderr, "\n" "file: %s\n" "line: %d\n" "token: %s\n",
                (const char *)list_back(yyfiles), yylineno, yytext);

	exit(1);
}

/*
 * Follow a node to a token, emit error, exit with 3.
 */
void log_semantic(struct node *t, const char *format, ...)
{
	va_list ap;
	va_start(ap, format);

	fprintf(stderr, "semantic error: ");
	vfprintf(stderr, format, ap);

	va_end(ap);

	fprintf(stderr, "\n");
	if (t) {
		while (tree_size(t) > 1)
			t = list_front(t->children);
		struct node_2 *node = t->data;
		struct tokenitem *token = node->token;
		fprintf(stderr, "file: %s\n" "line: %d\n" "token: %s\n",
		        token->filename, token->lineno, token->text);
	}

	exit(3);
}

void log_check(const char *format, ...)
{
	if (!arguments.checks)
		return;

	va_list ap;
	va_start(ap, format);

	fprintf(stderr, "type check: ");
	vfprintf(stderr, format, ap);
	fprintf(stderr, "\n");

	va_end(ap);
}

void log_symbol(const char *k, struct typeinfo *v)
{
	if (!arguments.symbols)
		return;

	fprintf(stderr, "Inserting symbol into %s/%zu: ",
	        print_region(region), offset);
	print_typeinfo(stderr, k, v);
	fprintf(stderr, "\n");
}
