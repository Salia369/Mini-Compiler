//
//  main.h
//  
//
//  Created by Hanna Salian on 11/8/17.
//

#ifndef main_h
#define main_h

#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <limits.h>
#include <stdlib.h>


#include "120++lex.h"
#include "tree.h"
#include "list.h"
#include "token.h"
#include "symt.h"
#include "hasht.h"
#include "codegen.h"
//#include "120++gram.h"

///*
extern FILE *yyin;
extern int yylineno;
extern char *yytext;
//extern int yyparse();
//extern int yychar;
//*/

nptr filestack;


tableptr classtable;
tableptr classfunctable;
tableptr functable;
tableptr globaltable;

#endif /* main_h */
