

#ifndef NODE_H
#define NODE_H

#include <stddef.h>
#include <stdio.h>

#include "type.h"
#include "rules.h"

struct tree;
struct list;
struct tokenitem;

struct node_2 {
	enum rule rule; 
	struct address place;
	struct list *code;
	struct tokenitem *token;
};

struct node_2 *node_new(enum rule r);
struct node_2 *get_node(struct node *t, size_t i);
enum rule get_rule(struct node *t);
struct tokenitem *get_token(struct node_2 *n);

#endif /* NODE_H */
