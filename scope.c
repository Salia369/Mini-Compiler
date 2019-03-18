

#include <stddef.h>

#include "scope.h"
#include "symt.h"
#include "list.h"
#include "hasht.h"


//Search the stack of scopes for a given identifier.
struct typeinfo *scope_search(char *k)
{
	if (!k)
		return NULL;

	struct list_node *iter = list_tail(yyscopes);
	while (!list_end(iter)) {
		struct typeinfo *t = hasht_search(iter->data, k);
		if (t)
			return t;
		iter = iter->prev;
	}
	return NULL;
}

size_t scope_size(struct hasht *t)
{
	if (t == NULL)
		return 0;

	size_t total = 0;
    size_t i;
	for (i = 0; i < t->size; ++i) {
		struct hasht_node *slot = t->table[i];
		if (slot && !hasht_node_deleted(slot)) {
			total += typeinfo_size(slot->value);
		}
	}
	return total;
}
