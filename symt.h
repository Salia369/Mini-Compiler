//
//  symt.h
//

#ifndef symt_h
#define symt_h

#include <stdio.h>
#include <stdbool.h>
#include "hasht.h"
#include "tree.h"


/* Forward declaration */
struct entry;
struct type_2;
struct table;


/* An entry can be a type entry or table entry.
 * Table entry is for storing scopes.
 */
void symbol_free(struct hasht_node *n);
typedef struct entry {
    char *name;
    
    struct type_2 *entrytype;
    struct table *entrytable;
    
} * entryptr;


/* Hash table struct that holds scope name and type data */
typedef struct table {
    
    char *name;
   // entryptr entry[10000];
    entryptr *entry;
    
} * tableptr;


/* Data struct which holds type information stored in an table entry */
typedef struct type_2 {
    
    int basetype;
    
    union {
        
        struct arraytype {
            
            int size;
            struct entry *type_2;
            /* some table */
            
        } a;
        
        struct functiontype {
            
            struct entry *type_2;
            /* some table */
            
        } f;
        
        struct classtype {
            struct entry *type_2;
            /* some table */
            
        } c;
        
    };
    
} * typeptr;


struct node;
struct hasht_node;
struct typeinfo;

/* Prototype declarations */
bool get_pointer(struct node *t);
tableptr new_table( char* );
entryptr new_entry( char* );
entryptr new_scope( char* );
entryptr get_entry( char* , tableptr );
void insert( entryptr, tableptr );
void insert_entry( char* , tableptr );
tableptr get_scope(char* , tableptr );
bool lookup( char *, tableptr );

struct typeinfo *type_check(struct node *t);

int get_array(struct node *t);
char *get_class(struct node *t);
char *class_member(struct node *n);
char *get_identifier(struct node *n);
void handle_param_list(struct node *n, struct hasht *s, struct list *l);
struct tokenitem *get_category(struct node *n, int target, int before);
struct tokenitem *get_category_(struct node *n, int target, int before);
void handle_param(struct typeinfo *v, struct node *n, struct hasht *s, struct list *l);
struct node *get_production(struct node *n, enum rule r);
char *class_member(struct node *n);

void symbol_populate(struct node *t);


#endif /* symt_h */
