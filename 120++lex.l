/*	$Id: lexer.l,v 1.2 1997/11/19 15:13:15 sandro Exp $	*/

/*
 * Copyright (c) 1997 Sandro Sigala <ssigala@globalnet.it>.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided witxh the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * ISO C++ lexical analyzer.
 *
 * Based on the ISO C++ draft standard of December '96.
 */

%{

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <ctype.h>
#include <libgen.h>
#include <string.h>

#include "120++gram.h"
#include "tree.h"
#include "hasht.h"
#include "list.h"

#include "args.h"
#include "logger.h"
#include "libs.h"
#include "rules.h"
#include "symt.h"
#include "token.h"
#include "node.h"


#define T(name) do { prepare_token(name); return name; } while(0)
#define YYAPPENDTEXT() token_push_text(yytoken, yytext)
#define YYAPPENDCHAR(character) token_push_sval_char(yytoken, character)

void include_lib();
static int check_identifier( char *s);//, tableptr t);

void typenametable_insert(char *s, tableptr t);
int tokenize(int category);
static void prepare_token(int category);
struct node *tree_new(struct node *parent, void *data,
bool (*compare)(void *a, void *b),
void (*delete)(void *data, bool leaf));
void delete_tree(void *data, bool leaf);
static void handle_include(char *filename);
static void handle_quoted_include(const char *s);

void insert_typename(char *k, int c);
void insert_typename_tree(struct node *t, int category);

static struct tokenitem *yytoken;

/* handle #include libraries */
static void handle_c();
static void handle_fstream();
static void handle_iostream();
static void handle_string();

/* extern */
extern FILE *yyin;
FILE *saved_yyin;
extern int yylineno;
extern char *yytext;
extern int yychar;
extern YYSTYPE yylval;
extern struct hasht *yytypes;
extern struct list *yyfiles;
extern struct list *yyclibs;
extern struct hasht *yyincludes;
extern tableptr classtable;
extern nptr filestack;

/*
 intsuffix                ([uU][lL]?)|([lL][uU]?)
 fracconst                ([0-9]*\.[0-9]+)|([0-9]+\.)
 exppart                    [eE][-+]?[0-9]+
 floatsuffix                [fFlL]
 chartext                ([^'])|(\\.)
 stringtext                ([^"])|(\\.)
 */

%}

%option warn nounput noinput
%option yylineno
%option noyywrap
%option header-file="120++lex.h"
%option yylineno
%x COMMENT STR CHR CHREND INC
D        [0-9]
L        [a-zA-Z_]
H        [a-fA-F0-9]
FS       (f|F|l|L)
IS       (u|U|l|L)*

%%


[ \r\t\v\f\n]*          { /* eat whitespace */ }

"//".*$                 {/* eat C++ comments */ }

"/*"                    { BEGIN(COMMENT); /* eat multi-line comments */ }

<COMMENT>{
    [^*]*           { /* eat comment in chunks */ }
    "*"+[^*/]*      { /* eat the lone star */ }
    "*"+"/"         { BEGIN(INITIAL); }
}

        /* includes */
"#include"              { BEGIN(INC); }

<INC>{
    [ \t]*          { /* eat whitespace */ }
    "<cstdlib>"     { libs.cstdlib  = true; BEGIN(INITIAL); }
    "<cmath>"       { libs.cmath    = true; BEGIN(INITIAL); }
    "<ctime>"       { libs.ctime    = true; BEGIN(INITIAL); }
    "<cstring>"     { libs.cstring  = true; BEGIN(INITIAL); }
    "<fstream>"     { libs.fstream  = true; handle_fstream(); BEGIN(INITIAL); }
    "<iostream>"    { libs.iostream = true; handle_iostream(); BEGIN(INITIAL); } 
    "<string>"      { libs.string   = true; handle_string(); BEGIN(INITIAL); } 
    "<iomanip>"     { libs.iomanip  = true; BEGIN(INITIAL); }
        /*
    "<iostream>"               {  included_iostream = 1; }
    "<cstdlib>"                { include_lib(); included_cstdlib = 1; }
    "<ctime>"                  { include_lib(); included_ctime = 1; }
    "<string>"                 { include_lib(); included_string = 1; }
    "<string.h>"               { include_lib(); included_string = 1; }
    "<fstream>"                { include_lib(); included_fstream = 1; }
    "<stdio.h>"                { include_lib(); included_stdio = 1; }
    "<cmath>"                  { include_lib(); included_cmath = 1; }
         */
    "<ctype.h>"     |
    "<math.h>"      |
    "<stdlib.h>"    |
    "<string.h>"    |
    "<time.h>"      { list_push_back(yyclibs, strdup(yytext)); BEGIN(INITIAL); handle_c(); }
    \"[^<>\n\"]+\"  { handle_quoted_include(yytext); BEGIN(INITIAL); }
    "<"[^<>]+">"    { log_lexical("unrecognized library: %s", yytext); }
    <<EOF>>         { log_lexical("unexpected EOF"); }
    .               { log_lexical("unrecognized token: %s"); }
}

        /* only allowed namespace directive */
"using namespace std;"  { libs.usingstd = true; }

        /* keywords */
"bool"                  { T(BOOL); }
"break"                 { T(BREAK); }
"case"                  { T(CASE); }
"char"                  { T(CHAR); }
"class"                 { T(CLASS); }
"continue"              { T(CONTINUE); }
"default"               { T(DEFAULT); }
"delete"                { T(DELETE); }
"do"                    { T(DO); }
"double"                { T(DOUBLE); }
"else"                  { T(ELSE); }
"false"                 { T(FALSE); }
"float"                 { T(FLOAT); }
"for"                   { T(FOR); }
"if"                    { T(IF); }
"int"                   { T(INT); }
"long"                  { T(LONG); }
"new"                   { T(NEW); }
"private"               { T(PRIVATE); }
"protected"             { T(PROTECTED); }
"public"                { T(PUBLIC); }
"return"                { T(RETURN); }
"short"                 { T(SHORT); }
"signed"                { T(SIGNED); }
"sizeof"                { T(SIZEOF); }
"struct"                { T(STRUCT); }
"switch"                { T(SWITCH); }
"true"                  { T(TRUE); }
"unsigned"              { T(UNSIGNED); }
"void"                  { T(VOID); }
"while"                 { T(WHILE); }

        /* unsupported keywords */
"const"                 { /* const qualifier is discarded */ }
"auto"                  |
"..."                   |
"enum"                  |
"extern"                |
"friend"                |
"goto"                  |
"namespace"             |
"register"              |
"static"                |
"this"                  |
"typedef"               |
"union"                 |
"using"                 |
"virtual"               |
"volatile"              { log_unsupported(); }

        /* integer and floating constants */
{D}+{IS}?               { T(INTEGER); }
{D}+{FS}?               { T(FLOATING); }
{D}*"."{D}+{FS}?        { T(FLOATING); }
{D}+"."{D}*{FS}?        { T(FLOATING); }

        /* character literal */
\'                      { prepare_token(CHARACTER); BEGIN(CHR); }

<CHR>{
    \'              { log_lexical("empty char literal"); }
    "\\'"           { yytoken->ival = '\''; YYAPPENDTEXT(); BEGIN(CHREND); }
    "\\\""          { yytoken->ival = '"';  YYAPPENDTEXT(); BEGIN(CHREND); }
    "\\?"           { yytoken->ival = '\?'; YYAPPENDTEXT(); BEGIN(CHREND); }
    "\\a"           { yytoken->ival = '\a'; YYAPPENDTEXT(); BEGIN(CHREND); }
    "\\b"           { yytoken->ival = '\b'; YYAPPENDTEXT(); BEGIN(CHREND); }
    "\\f"           { yytoken->ival = '\f'; YYAPPENDTEXT(); BEGIN(CHREND); }
    "\\n"           { yytoken->ival = '\n'; YYAPPENDTEXT(); BEGIN(CHREND); }
    "\\r"           { yytoken->ival = '\r'; YYAPPENDTEXT(); BEGIN(CHREND); }
    "\\t"           { yytoken->ival = '\t'; YYAPPENDTEXT(); BEGIN(CHREND); }
    "\\v"           { yytoken->ival = '\v'; YYAPPENDTEXT(); BEGIN(CHREND); }
    "\\0"           { yytoken->ival = '\0'; YYAPPENDTEXT(); BEGIN(CHREND); }
    "\\\\"          { yytoken->ival = '\\'; YYAPPENDTEXT(); BEGIN(CHREND); }
    [^\\"'"]        { yytoken->ival = *yytext;
        YYAPPENDTEXT();
        BEGIN(CHREND); }
    .               { log_lexical("in char literal, unrecognized token: %s", yytext); }
}

<CHREND>{
    \'              { YYAPPENDTEXT(); BEGIN(INITIAL); return CHARACTER; }
    \n              { log_lexical("in char literal: unexpected newline"); }
    .               { log_lexical("in char literal: too many symbols"); }
}

        /* string literal */
\"                      { prepare_token(STRING); BEGIN(STR); }

<STR>{
    \"              { token_finish_sval(yytoken);
    YYAPPENDTEXT();
    BEGIN(INITIAL);
    return STRING; }
"\\'"           { YYAPPENDCHAR('\''); YYAPPENDTEXT(); }
"\\\""          { YYAPPENDCHAR('"');  YYAPPENDTEXT(); }
"\\?"           { YYAPPENDCHAR('\?'); YYAPPENDTEXT(); }
"\\a"           { YYAPPENDCHAR('\a'); YYAPPENDTEXT(); }
"\\b"           { YYAPPENDCHAR('\b'); YYAPPENDTEXT(); }
"\\f"           { YYAPPENDCHAR('\f'); YYAPPENDTEXT(); }
"\\n"           { YYAPPENDCHAR('\n'); YYAPPENDTEXT(); }
"\\r"           { YYAPPENDCHAR('\r'); YYAPPENDTEXT(); }
"\\t"           { YYAPPENDCHAR('\t'); YYAPPENDTEXT(); }
"\\v"           { YYAPPENDCHAR('\v'); YYAPPENDTEXT(); }
"\\0"           { YYAPPENDCHAR('\0'); YYAPPENDTEXT(); }
"\\\\"          { YYAPPENDCHAR('\\'); YYAPPENDTEXT(); }
[^\\\"\n]+      { token_push_sval_string(yytoken, yytext);
YYAPPENDTEXT(); }
\n              { log_lexical("in string literal: unexpected newline"); }
.               { log_lexical("in string literal: unrecognized token"); }
<<EOF>>         { log_lexical("in string literal: unterminated"); }
}

        /* operators */
">>="                   { T(SREQ); }
"<<="                   { T(SLEQ); }
"+="                    { T(ADDEQ); }
"-="                    { T(SUBEQ); }
"*="                    { T(MULEQ); }
"/="                    { T(DIVEQ); }
"%="                    { T(MODEQ); }
"&="                    { T(ANDEQ); }
"^="                    { T(XOREQ); }
"|="                    { T(OREQ); }
">>"                    { T(SR); }
"<<"                    { T(SL); }
"++"                    { T(PLUSPLUS); }
"--"                    { T(MINUSMINUS); }
"->"                    { T(ARROW); }
"->*"                   { T(ARROWSTAR); }
"&&"                    { T(ANDAND); }
"||"                    { T(OROR); }
"<="                    { T(LTEQ); }
">="                    { T(GTEQ); }
"=="                    { T(EQ); }
"!="                    { T(NOTEQ); }
";"                     { T(';'); }
"{"                     { T('{'); }
"}"                     { T('}'); }
","                     { T(','); }
"::"                    { T(COLONCOLON); }
":"                     { T(':'); }
"="                     { T('='); }
"("                     { T('('); }
")"                     { T(')'); }
"["                     { T('['); }
"]"                     { T(']'); }
"."                     { T('.'); }
"&"                     { T('&'); }
"!"                     { T('!'); }
"~"                     { T('~'); }
"-"                     { T('-'); }
"+"                     { T('+'); }
"*"                     { T('*'); }
"/"                     { T('/'); }
"%"                     { T('%'); }
"<"                     { T('<'); }
">"                     { T('>'); }
"^"                     { T('^'); }
"|"                     { T('|'); }
"?"                     { T('?'); }

        /* identifer */
{L}({L}|{D})*           { return check_identifier(yytext); }

<*>.                    { log_lexical("unrecognized token: %s", yytext); }

<<EOF>>                 { /* restart line numbering count */
    yylineno = 1;
    /* pop the current buffer and filename */
    yypop_buffer_state();
    list_pop_back(yyfiles);
    
    /* if buffer stack is empty, stop */
    if (!YY_CURRENT_BUFFER)
    yyterminate();
}
%%


void prepare_token(int category)
{
    yytoken = token_new(category, yylineno, yytext,
             (const char *)list_back(yyfiles));
    struct node_2 *n = node_new(TOKEN);
    n->token = yytoken;
    yylval.nodeptr = tree_new(NULL, n, NULL,
    (void (*)(void *, bool))&delete_tree);
}

static void handle_quoted_include(const char *s)
{
    /* size without surrounding quotes */
    size_t len = strlen(s) - 2;
    
    char *include = calloc(len + 1, sizeof(char));
    log_assert(include);
    
    /* copy substring */
    strncpy(include, s + 1, len);
    include[len] = '\0';
    
    /* path = realpath(dirname(current) + "/" + include) */
    char *current = list_back(yyfiles);
    
    /* copy because Wormulon's dirname modifies */
    char *copy = strdup(current);
    const char *dir = dirname(copy); /* do not pass to free */
    log_debug("dirname: %s", dir);
    
    /* append POSIX directory separator and include path */
    char *resolved;
    asprintf(&resolved, "%s/%s", dir, include);
    log_debug("resolved: %s", resolved);
    
    /* resolve abosolute path name */
    handle_include(resolved);
    
    free(copy);
    free(resolved);
    free(include);
}


static void handle_include(char *s)
{
    /* restart line numbering count */
    yylineno = 1;
    
    char *current = list_back(yyfiles);
    
    char *filename = realpath(s, NULL);
    if (filename == NULL)
    log_error("could not find included file: %s\n"
    "included from: %s", s, current);
    
    if (hasht_search(yyincludes, filename)) {
        log_debug("Flex: already included %s", filename);
        return;
    }
    
    /* open file */
    yyin = fopen(filename, "r");
    if (yyin == NULL)
    log_error("could not open included file: %s\n"
    "included from: %s", filename, current);
    
    /* push filename and buffer state */
    list_push_back(yyfiles, filename);
    yypush_buffer_state(yy_create_buffer(yyin, YY_BUF_SIZE));
    
    /* record filename in yyincludes set */
    hasht_insert(yyincludes, filename, filename);
    log_debug("filename: %s", filename);
}

/*
 * Insert C prototypes.
 */
static void handle_c()
{
    char *path;
    asprintf(&path, "%s/include_c.h", arguments.include);
    handle_include(path);
    free(path);
}

/*
 * Insert "ifstream" and "ofstream" types.
 */
static void handle_fstream()
{
    char *path;
    asprintf(&path, "%s/include_fstream.h", arguments.include);
    handle_include(path);
    free(path);
}

/*
 * Insert "cin" and "cout" symbols.
 */
static void handle_iostream()
{
    char *path;
    asprintf(&path, "%s/include_iostream.h", arguments.include);
    handle_include(path);
    free(path);
}

/*
 * Insert "string" type.
 */
static void handle_string()
{
    char *path;
    asprintf(&path, "%s/include_string.h", arguments.include);
    handle_include(path);
    free(path);
}
/*
 int tokenize(int category)
{
   // yytoken =
   prepare_token(category);
  // yytoken = create_token(category, yytext, yylineno, filestack->filename);
    //yylval.nodeptr = tree_new(NULL, n, NULL,(void (*)(void *, bool))&delete_tree);
    //yylval.nodeptr = alctree(yytext, category, 0);
	//yylval.nodeptr->tok_text = yytoken;
   	return category;
}
 */
/*
void handle_include()
{
	
    char *fname = malloc(strlen(yytext)+1-strlen("#include \"\""));
    fname = strchr(yytext, '\"')+1;
    fname[strlen(fname)-1] = '\0';
    printf("%s\n",fname);
    yyin = fopen(fname,"r");
    push_node(&filestack, fname);
    yypush_buffer_state(yy_create_buffer(yyin, YY_BUF_SIZE));

}
*/
void insert_typename_tree(struct node *t, int category)
{
    struct node_2 *node = t->data;
    log_assert(node);
    struct tokenitem *token = node->token;
    log_assert(token);
    char *key = token->text;
    insert_typename(key, category);
}

void typenametable_insert(char *s, tableptr t){ //, int c){//
    
    char *key = strdup(s);
    int *i = malloc(sizeof(*i));
    
    if (hasht_search(yytypes, key))
    log_lexical("typename %s previously declared", s);
    
    if (hasht_insert(yytypes, key, i) == NULL)
    log_error("failed to insert %s into yytypes table", s);
    
}


void insert_typename(char *k, int c)
{
    log_debug("inserting typename %s", k);
    char *key = strdup(k);
    int *i = malloc(sizeof(*i));
    log_assert(key && i);
    
    *i = c;
    
    if (hasht_search(yytypes, key))
    log_lexical("typename %s previously declared", k);
    
    if (hasht_insert(yytypes, key, i) == NULL)
    log_error("failed to insert %s into yytypes table", k);
}


static int check_identifier( char *s) {//, tableptr t ) {
    int *c = hasht_search(yytypes, (void *)s);
    if (c) {
      //  printf( "\"%s\" is in class table\n", s );
      T(*c);
        //return CLASS_NAME;
        
    } else {
      ///  printf( "\"%s\" is a not in class table\n", s );
      T(IDENTIFIER);
       // return IDENTIFIER;
        
    }
    
    /*
    int *c = hasht_search(yytypes, (void *)s);
    if (c)
        T(*c);
    else
        T(IDENTIFIER);
     */
};

#undef T
#undef YYAPPENDTEXT
#undef YYAPPENDCHAR

