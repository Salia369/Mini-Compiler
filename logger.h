

#ifndef LOGGER_H
#define LOGGER_H

#include <stdbool.h>

struct node;
struct typeinfo;

void log_error(const char *format, ...);
void log_debug(const char *format, ...);
void log_assert(bool p);

void log_lexical(const char *format, ...);
void log_semantic(struct node *t, const char *format, ...);
void log_check(const char *format, ...);
void log_unsupported();
void log_symbol(const char *k, struct typeinfo *v);

#endif /* LOGGER_H */
