//
//  tree.h
//  


#ifndef tree_h
#define tree_h

#include <stdio.h>
#include <stdarg.h>
#include <stddef.h>

#include "token.h"
#include "type.h"
#include "list.h"
#include "rules.h"




bool TREE_DEBUG;


typedef struct node {

    struct list *children;
    struct node *parent;
    bool (*compare)(void *a, void *b);

    
    void* data;
    char* text;
    
    struct instr *next;
    struct addr *address;
    
    void (*delete)(void *data, bool leaf);
    
} * node ;

bool treeprint(node , int );
node alctree(char *, int , int , ...);
struct node *tree_new(struct node *parent, void *data,
                      bool (*compare)(void *a, void *b),
                      void (*delete)(void *data, bool leaf));
struct node *tree_new_group(struct node *parent, void *data,
                            bool (*compare)(void *a, void *b),
                            void (*delete)(void *data, bool leaf),
                            int count, ...);

struct node *tree_push_front(struct node *self, void *data);
struct node *tree_push_back(struct node *self, void *data);
struct node *tree_push_child(struct node *self, struct node *child);

struct node *tree_index(struct node *self, int pos);

void tree_traverse(struct node *self, int d,
                   bool (*pre) (struct node *t, int d),
                   void (*in)  (struct node *t, int d),
                   void (*post)(struct node *t, int d));

size_t tree_size(struct node *self);
void tree_free(struct node *self);


#endif /* tree_h */
