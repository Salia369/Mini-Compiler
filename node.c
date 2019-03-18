

#include <stdlib.h>
#include <stdio.h>

#include "node.h"
#include "logger.h"
#include "list.h"
#include "tree.h"
#include "rules.h"

/*
 * Allocates a new blank node.
 *
 * Nodes hold semantic attributes, such as production rule, memory
 * address (place field), and non-NULL token pointers if a leaf.
 */
struct node_2 *node_new(enum rule r)
{
	struct node_2 *n = malloc(sizeof(*n));
	if (n == NULL)
		log_error("could not allocate memory for node");

	n->rule = r;
	n->place.region = UNKNOWN_R;
	n->place.offset = 0;
	n->code = NULL;
	n->token = NULL;

	return n;
}

/*
 * Given a tree and position, returns subtree's node.
 *
 * Returns NULL if child tree not found.
 */
struct node_2 *get_node(struct node *t, size_t i)
{
	log_assert(t);

	if (tree_size(t) > 1)
		t = tree_index(t, i);

	if (t == NULL)
		return NULL;

	struct node_2 *n = t->data;
	log_assert(n);

	return n;
}

/*
 * Given a tree node, extract its production rule.
 */
enum rule get_rule(struct node *t)
{
	log_assert(t);
    struct node_2 *n = t->data;
    return n->rule;
}

/*
 * Given a leaf node, extract its token, checking for NULL.
 */
struct tokenitem *get_token(struct node_2 *n)
{
	log_assert(n);
	struct tokenitem *t = n->token;
	log_assert(t);
	return t;
}
