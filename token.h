//
//  token.h
//  

#ifndef token_h
#define token_h

#include <stdio.h>
#include <stddef.h>


typedef struct tokenitem {
    size_t ssize;
    int category;   /* the integer code returned by yylex */
    char *text;     /* the actual string (lexeme) matched */
    int lineno;     /* the line number on which the token occurs */
    char *filename; /* the source file in which the token occurs */
    int ival;       /* if you had an integer constant, store its value here */
    char *sval;      /* if you had a string constant, malloc space and store */
    double fval;
} *tok_1;           /*    the string (less quotes and after escapes) here */

struct tokenitem name;

struct tokenitem *token_new(int category, int lineno,
                        const char *text, const char* filename);

void token_free(struct tokenitem *t);
void token_print(struct tokenitem *t);
void token_push_sval_char(struct tokenitem *t, char c);
void token_push_sval_string(struct tokenitem *t, const char *s);
void token_push_text(struct tokenitem *t, const char* s);
void token_finish_sval();

//tok_1 create_token(int, char*, int, char*);
//void print_token(tok_1);

#endif /* token_h */
