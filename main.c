//
//  main.c
//


#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <limits.h>
#include <stdlib.h>
#include <stddef.h>
#include <stdbool.h>
#include <unistd.h>
#include <libgen.h>
#include <time.h>
#include <argp.h>

#include "main.h"
#include "120++lex.h"
#include "tree.h"
#include "list.h"
#include "token.h"
#include "symt.h"
#include "hasht.h"
#include "node.h"
#include "codegen.h"
#include "final.h"
#include "libs.h"
#include "args.h"
#include "type.h"
#include "logger.h"
#include "rules.h"
#include "scope.h"



static struct argp_option options[] = {
    { "debug",    'd', 0,      0, "Print debug messages (scopes, mostly).\n"
        "Also disables exit on assertion failure."},
    { "tree",     't', 0,      0, "Print the syntax tree." },
    { "syntax",   't', 0,      OPTION_ALIAS },
    { "symbols",  'y', 0,      0, "Print the populated symbols." },
    { "checks",   'k', 0,      0, "Print the performed type checks." },
    { "types",    'k', 0,      OPTION_ALIAS },
    { "include",  'I', "DIR",  0, "Search path for 'system' headers." },
    { "assemble", 's', 0,      0, "Generate assembler code." },
    { "compile",  'c', 0,      0, "Generate object code." },
    { "output",   'o', "FILE", 0, "Name of generated executable." },
    { 0 }
};


static error_t parse_opt(int key, char *arg, struct argp_state *state);
static struct argp argp = { options, parse_opt, NULL, NULL };


static void parse_program(char *filename);
void free_typename(struct hasht_node *t);
bool print_tree(struct node *t, int d);
char *remove_extention(char* mystr) ;

struct node *t_unit;
struct list *yyscopes;
struct list *yyfiles;
struct list *yyclibs;
struct hasht *yyincludes;
struct hasht *yytypes;
size_t yylabels;


char *yytext = NULL;
int yyparse();
size_t offset;
enum region region;

//FILE *yyin;

int main (int argc, char **argv){

    arguments.debug = false;
    arguments.tree = false;
    arguments.symbols = false;
    arguments.checks = false;
    arguments.assemble = false;
    arguments.compile = false;
    arguments.output = "a.out";
    arguments.include = getcwd(NULL, 0);

    argp_parse(&argp, argc, argv, 0, 0, &arguments);
    
    int i;
    char *filelist[argc];
    char *objects = "";

    
    // parse each input file as a new 'program'
    for ( i = 1; i < argc; i++){
        filelist[i] = argv[i];
        name.filename = argv[i];
        if (name.filename == NULL)
            log_error("could not find input file: %s",
                      name.filename);

       
     //*/
        /* copy because Wormulon's basename modifies */
        char *copy = strdup(name.filename);
        char *base = remove_extention(copy);
        
        /* append object name to end of objects */
        char *temp = NULL;
        asprintf(&temp, "%s %s.o", objects, base);
        /* free if previously allocated */
        if (strcmp(objects, "") != 0)
            free(objects);
        objects = temp;
        parse_program(name.filename);
        free(copy);
       
        
    }
//}

    // link object files
    ///*
    if (!arguments.assemble && !arguments.compile) {

        char *command = NULL;
        asprintf(&command, "gcc -o %s %s", arguments.output, objects);
        int status = system(command);
        if (status != 0)
            log_error("command failed: %s", command);
        free(command);

        asprintf(&command, "rm -f%s", objects);
        status = system(command);
        if (status != 0)
            log_error("command failed: %s", command);
        free(command);

    }
//*/
    return EXIT_SUCCESS;
}

void parse_program(char *filename)
{

    printf("parsing file: %s\n", filename);
    yyfiles = list_new(NULL, &free);
    log_assert(yyfiles);
    list_push_back(yyfiles, filename);
    yyclibs = list_new(NULL, &free);
    log_assert(yyclibs);

    yyincludes = hasht_new(8, true, NULL, NULL, NULL);
    log_assert(yyincludes);

    // open file for lexer
    yyin = fopen(filename, "r");
    if (yyin == NULL)
        log_error("could not open input file: %s", filename);
   
    yypush_buffer_state(yy_create_buffer(yyin, YY_BUF_SIZE));

    /* setup yytypes table for lexer */
    yytypes = hasht_new(8, true, NULL, NULL, &free_typename);
    log_assert(yytypes);

    /* reset library flags */
    libs.usingstd    = false;
    libs.cstdlib    = false;
    libs.cmath    = false;
    libs.ctime    = false;
    libs.cstring    = false;
    libs.fstream    = false;
    libs.iostream    = false;
    libs.string    = false;
    libs.iomanip    = false;

    log_debug("invoking Bison");
    int result;

    while(!feof(yyin)) {
        result = yyparse();
    }


    /* print syntax tree */
    if (arguments.tree)
        tree_traverse(t_unit, 0, &print_tree, NULL, NULL);

    /* initialize scope stack */
    log_debug("setting up for semantic analysis");
    yyscopes = list_new(NULL, NULL);
    log_assert(yyscopes);
    struct hasht *global = hasht_new(32, true, NULL, NULL, &symbol_free);
    log_assert(global);
    list_push_back(yyscopes, global);

    /* build the symbol tables */
    log_debug("populating symbol tables");
    region = GLOBE_R;
    offset = 0;
    symbol_populate(t_unit);
    log_debug("global scope had %zu symbols", hasht_used(global));

    /* constant symbol table put in front of stack for known location */
    struct hasht *constant = hasht_new(32, true, NULL, NULL, &symbol_free);
    log_assert(constant);
    list_push_front(yyscopes, constant);
    region = CONST_R;
    offset = 0;
    log_debug("type checking");
    type_check(t_unit);
    /* generating intermediate code */
    log_debug("generating intermediate code");
    yylabels = 0; /* reset label counter */
    codegen(t_unit);
    
    //struct node_2 *n = t_unit->data;
   // n->code = list_new(NULL, NULL);
    struct node_2 * temp = t_unit->data;
    struct list *code = temp->code;
    
    
    /* iterate to get correct size of constant region*/
    size_t i, string_size = 0;
    for (i = 0; i < constant->size; ++i) {
        struct hasht_node *slot = constant->table[i];
        if (slot && !hasht_node_deleted(slot)) {
            struct typeinfo *v = slot->value;
            if (v->base == FLOAT_T)
                string_size += 8;
            else if (v->base == CHAR_T && v->pointer)
                string_size += v->token->ssize;
        }
    }
    
    /* print intermediate code file if debugging */
    if (arguments.debug) {
    
    char * newname = remove_extention (filename);
        char *output_file = NULL;
        asprintf(&output_file, "%s.ic", newname);

        FILE *ic = fopen(output_file, "w");
        if (ic == NULL)
            log_error("could not save to output file: %s",
                      output_file);
        fprintf(ic, ".file \"%s\"\n", newname);
        /* print .string region */
        fprintf(ic, ".string %zu\n", string_size);
        /* iterate to print everything but ints, chars, and bools */
        size_t j;
        for (j = 0; j < constant->size; ++j) {
            struct hasht_node *slot = constant->table[j];
            if (slot && !hasht_node_deleted(slot)) {
                struct typeinfo *v = slot->value;
                if (v->base == FLOAT_T
                    || (v->base == CHAR_T && v->pointer)) {
                    fprintf(ic, "    ");
                    print_typeinfo(ic, slot->key, v);
                    fprintf(ic, "\n");
                }
            }
        }
        /* print .data region */
        fprintf(ic, ".data\n");
        size_t k;
        for (k = 0; k < global->size; ++k) {
            struct hasht_node *slot = global->table[k];
            if (slot && !hasht_node_deleted(slot)) {
                struct typeinfo *value = slot->value;
                if (value->base != FUNCTION_T) {
                    fprintf(ic, "    ");
                    print_typeinfo(ic, slot->key, value);
                    fprintf(ic, "\n");
                }
            }
        }
        fprintf(ic, ".code\n");
        print_code(ic, code);

        fclose(ic);
        free(output_file);
    }
    
    
    log_debug("generating final code");
    /* copy because Wormulon */
    //char *copy = strdup(filename);
    char *base = remove_extention(filename);
   // free(copy);
    char *out_file = NULL;
    asprintf(&out_file, "%s.c", base);
    
    FILE *fc = fopen(out_file, "w");
    if (fc == NULL)
        log_error("could not save to output file: %s", out_file);
    
    fprintf(fc, "/*\n");
    fprintf(fc, "/* Required includes */\n");
    fprintf(fc, "#include <stdlib.h>\n");
    fprintf(fc, "#include <stdbool.h>\n");
    fprintf(fc, "#include <string.h>\n");
    if (libs.usingstd && libs.iostream)
    fprintf(fc, "#include <stdio.h>\n");
    fprintf(fc, "\n");
    
    /* include passed-through C headers */
    struct list_node *iter = list_head(yyclibs);
    while (!list_end(iter)) {
        fprintf(fc, "#include %s\n", (char *)iter->data);
        iter = iter->next;
    }
    fprintf(fc, "\n");
    
    /* get maximum param size for mock stack */
    size_t max_param_size = 0;
    iter = list_head(code);
    while (!list_end(iter)) {
        struct op *op = iter->data;
        if (op->code == PROC_O) {
            size_t param_size = op->address[0].offset;
            if (param_size > max_param_size)
            max_param_size = param_size;
        }
        iter = iter->next;
    }
    
    fprintf(fc, "/* Memory regions */\n");
    fprintf(fc, "char constant[%zu];\n", string_size);
    fprintf(fc, "char global[%zu];\n", global->size);
    fprintf(fc, "char stack[%zu];\n", max_param_size);
    fprintf(fc, "\n");
    
    fprintf(fc, "/* Final Three-Address C Generated Code */\n");
    final_code(fc, code);
    fclose(fc);
    ///*
    // remove TAC-C "assembler" code
    if (!arguments.assemble) {
        // compile object files
        char *command;
        asprintf(&command, "gcc -c %s", out_file);
        int status = system(command);
        if (status != 0)
        log_error("command failed: %s", command);
        
       // remove(out_file);
    }
  //   */
    
    free(out_file);

    // clean up
    log_debug("cleaning up");
    tree_free(t_unit);
    yylex_destroy();
    hasht_free(yytypes);
    free(yyincludes); // values all referenced elsewhere
    list_free(yyfiles);
    list_free(yyclibs);
    list_free(yyscopes);

}

void free_typename(struct hasht_node *t)
{
    free(t->key);
    free(t->value);
}


static error_t parse_opt(int key, char *arg, struct argp_state *state)
{
    struct arguments *arguments = state->input;
    
    switch (key) {
        case 'd':
        arguments->debug = true;
        break;
        case 't':
        arguments->tree = true;
        break;
        case 'y':
        arguments->symbols = true;
        break;
        case 'k':
        arguments->checks = true;
        break;
        case 'I':
        arguments->include = arg;
        break;
        case 's':
        arguments->assemble = true;
        break;
        case 'c':
        arguments->compile = true;
        break;
        case 'o':
        arguments->output = arg;
        break;
        
        case ARGP_KEY_NO_ARGS:
        argp_usage(state);
        
        case ARGP_KEY_ARGS:
        arguments->input_files = &state->argv[state->next];
        state->next = state->argc;
        break;
        
        default:
        return ARGP_ERR_UNKNOWN;
    }
    return 0;
}


char *remove_extention(char* mystr) {
    char *retstr;
    char *lastdot;
    if (mystr == NULL)
    return NULL;
    if ((retstr = malloc (strlen (mystr) + 1)) == NULL)
    return NULL;
    strcpy (retstr, mystr);
    lastdot = strrchr (retstr, '.');
    if (lastdot != NULL)
    *lastdot = '\0';
    return retstr;
}


