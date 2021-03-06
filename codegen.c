//
//  codegen.c
//

#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>

#include "codegen.h"
#include "tree.h"
#include "symt.h"
#include "token.h"
#include "hasht.h"
#include "list.h"
#include "type.h"
#include "main.h"
#include "logger.h"
#include "scope.h"
#include "node.h"

#define append_code(i) do { n->code = list_concat(n->code, get_code(t, i)); } while (0)
#define child(i) tree_index(t, i)
#define map_c(a) map_code(n->rule, a)

struct list *yyscopes;
extern size_t yylabels;

extern struct typeinfo int_type;
extern struct typeinfo float_type;
extern struct typeinfo char_type;
extern struct typeinfo string_type;
extern struct typeinfo bool_type;
extern struct typeinfo void_type;
extern struct typeinfo class_type;
extern struct typeinfo unknown_type;
extern struct typeinfo ptr_type;

static enum opcode map_code(enum rule r, struct typeinfo *type);
static struct op *op_new(enum opcode code, char *name,
                         struct address a, struct address b, struct address c);
static void push_op(struct node_2 *n, struct op *op);
static struct op *label_new();
static struct address temp_new(struct typeinfo *t);
static struct list *get_code(struct node *t, int i);
static struct address get_place(struct node *t, int i);
static struct address get_label(struct op *op);

const struct address e = { UNKNOWN_R, 0, &unknown_type };
const struct address one = { CONST_R, 1, &int_type };

struct op break_op;
struct op continue_op;
struct op default_op;

static void backpatch(struct list *code, struct op *first, struct op *follow);
static struct op *handle_switch(struct list *code, struct op *next,
                                struct address temp, struct address test,
                                struct list *test_code);

void codegen(struct node *t)
{

    struct node_2 *n = t->data;

    /* pre-order handling of scopes */
    size_t scopes = list_size(yyscopes);
    bool scoped = false;
    enum region region_ = region;
    size_t offset_ = offset;

    switch (n->rule) {
        case CTOR_FUNCTION_DEF:
        case FUNCTION_DEF: {
            /* manage scopes for function recursion */
            scoped = true;
            
            /* retrieve class scopes */
            char *class_name = class_member(t);
            struct typeinfo *class = scope_search(class_name);
            if (class) {
                log_debug("pushing class %s scopes", class_name);
                scope_push(class->class.public);
                scope_push(class->class.private);
            }
            
            /* retrieve function scope */
            char *k = (n->rule == CTOR_FUNCTION_DEF)
            ? get_class(t) /* ctor function name is class name */
            : get_identifier(t);
            
            struct typeinfo *f = scope_search(k);
            log_assert(f);
            log_debug("pushing function %s scope", k);
            scope_push(f->function.symbols);
            
            /* manage memory regions */
            region = LOCAL_R;
            offset = scope_size(f->function.symbols);
            /* class functions' scope size does not account for implicit pointer */
            if (class)
                offset += typeinfo_size(&ptr_type);
            
            break;
        }
        default: {
            break;
        }
    }
    
    /* recurse through children */
    struct list_node *iter = list_head(t->children);
    while (!list_end(iter)) {
        codegen(iter->data);
        iter = iter->next;
    }
    
    /* leaf nodes have no code associated with them since all uses of
     symbols are handled in higher nodes */
    if (n->rule == TOKEN) {
        /* this step should already be handled in type_check() */
        /* n->place = scope_search(n->token->text)->place; */
        log_assert(n->token);
        return;
    }
    
    /** post-order **/
    switch(n->rule) {
        case INITIALIZER:
        case LITERAL: { /* pass up .place for symbols */
            n->place = get_place(t, 0);
            append_code(0);
            break;
        }
        case INIT_DECL: { /* initializer assignment */
            n->place = get_place(t, 0);
            
            struct node_2 *init = get_node(t, 1);
            if (init == NULL)
                break;
            
            char *k = get_identifier(t);
            struct typeinfo *v = scope_search(k);
            if (v->base == CLASS_T && strcmp(v->class.type, "string") == 0) {
                log_debug("found class initializer");
                struct address count = { CONST_R, 2, &int_type };
                struct address pointer;
                if (v->pointer) {
                    pointer = v->place;
                } else {
                    struct typeinfo *temp = typeinfo_copy(v);
                    temp->pointer = true;
                    pointer = temp_new(temp);
                    push_op(n, op_new(ADDR_O, k, pointer, v->place, e));
                }
                push_op(n, op_new(PARAM_O, NULL, pointer, e, e));
                push_op(n, op_new(PARAM_O, NULL, get_place(t, 1), e, e));
                push_op(n, op_new(CALL_O, "string__string", e, count, e));
                break;
            }
            append_code(1);
            
            push_op(n, op_new(ASN_O, get_identifier(t), n->place, init->place, e));
            break;
        }
        case SIMPLE_DECL: { /* default constructor call if class declaration */
            n->place = get_place(t, 1);
            append_code(1);
            char *k = get_identifier(t);
            char *class = get_class(t);
            if (class && !get_pointer(child(1))) {
                if (strcmp(class, "ifstream") == 0
                    || strcmp(class, "ofstream") == 0
                    || strcmp(class, "string") == 0)
                    break;
                struct address count = { CONST_R, 1, &int_type };
                char *name = NULL;
                asprintf(&name, "%s__%s", class, class);
                struct address pointer = temp_new(&ptr_type);
                push_op(n, op_new(ADDR_O, k, pointer, n->place, e));
                push_op(n, op_new(PARAM_O, k, pointer, e, e));
                push_op(n, op_new(CALL_O, name, e, count, e));
            }
            break;
        }
        case NEW_EXPR: { /* explicit constructor call */
            append_code(2); /* parameters */
            char *k = get_class(t);
            struct typeinfo *class = scope_search(k);
            if (class) {
                struct address size = { CONST_R,
                    scope_size(class->class.public)
                    + scope_size(class->class.private),
                    &int_type };
                class = typeinfo_copy(class);
                class->pointer = true;
                n->place = temp_new(class);
                push_op(n, op_new(NEW_O, k, n->place, size, e));
                
                char *name;
                asprintf(&name, "%s__%s", k, k);
                struct typeinfo *ctor = hasht_search(class->class.public, k);
                struct address count = { CONST_R,
                    list_size(ctor->function.parameters),
                    &int_type };
                push_op(n, op_new(PARAM_O, k, n->place, e, e));
                push_op(n, op_new(CALL_O, name, e, count, e));
            } else {
                struct node *type_spec = get_production(t, TYPE_SPEC_SEQ);
                struct typeinfo *type = typeinfo_copy(type_check(tree_index(type_spec, 0)));
                struct address size = { CONST_R,
                    typeinfo_size(type),
                    &int_type };
                type->pointer = true;
                n->place = temp_new(type);
                push_op(n, op_new(NEW_O, NULL, n->place, size, e));
            }
            break;
        }
        case DELETE_EXPR1: {
            append_code(1);
            char *k = get_class(t);
            push_op(n, op_new(DEL_O, k, get_place(t, 1), e, e));
            break;
        }
        case POSTFIX_ARROW_FIELD:
        case POSTFIX_DOT_FIELD: { /* handle public field access */
            if (get_rule(t->parent) == POSTFIX_CALL)
                break;
            char *k = get_identifier(child(0));
            struct typeinfo *v = scope_search(k);
            char *f = get_identifier(child(2));
            struct typeinfo *class = scope_search(v->class.type);
            struct typeinfo *field = hasht_search(class->class.public, f);
            log_assert(field->base != FUNCTION_T);
            struct address pointer;
            if (v->pointer) {
                pointer = v->place;
            } else {
                struct typeinfo *temp = typeinfo_copy(v);
                temp->pointer = true;
                pointer = temp_new(temp);
                push_op(n, op_new(ADDR_O, k, pointer, v->place, e));
            }
            struct address offset = { CONST_R, field->place.offset, &int_type };
            char *name;
            asprintf(&name, "%s%s%s", k,
                     (n->rule == POSTFIX_DOT_FIELD) ? "." : "->", f);
            if (get_rule(t->parent) == ASSIGN_EXPR) {
                struct typeinfo *temp = typeinfo_copy(field);
                temp->pointer = true;
                n->place = temp_new(temp);
                push_op(n, op_new(LFIELD_O, name, n->place, pointer, offset));
            } else {
                n->place = temp_new(field);
                push_op(n, op_new(RFIELD_O, name, n->place, pointer, offset));
            }
            break;
        }
        case POSTFIX_CALL: { /* function invocation */
            char *k = get_identifier(t);
            char *name;
            struct typeinfo *f = scope_search(k);
            enum rule child_rule = get_rule(child(0));
            bool member_call = (child_rule == POSTFIX_DOT_FIELD ||
                                child_rule == POSTFIX_ARROW_FIELD);
            if (member_call) {
                /* get name of field */
                log_assert(f);
                char *field = get_identifier(tree_index(child(0), 2));
                asprintf(&name, "%s__%s", f->class.type, field);
                
                /* get address of pointer to class instance */
                struct address pointer;
                if (f->pointer) {
                    pointer = f->place;
                } else {
                    pointer = temp_new(&ptr_type);
                    push_op(n, op_new(ADDR_O, k, pointer, f->place, e));
                }
                /* get member function pointer */
                struct typeinfo *class = scope_search(f->class.type);
                f = hasht_search(class->class.public, field);
                
                /* push class pointer */
                push_op(n, op_new(PARAM_O, k, pointer, e, e));
            } else {
                name = strdup(k);
            }
            log_assert(k && f);
            
            /* make a temp if returning something */
            if (typeinfo_compare(typeinfo_return(f), &void_type))
                n->place = e;
            else
                n->place = temp_new(typeinfo_return(f));
            
            /* count number of parameters */
            struct address count = { CONST_R,
                list_size(f->function.parameters),
                &int_type };
            if (member_call)
                ++count.offset;
            append_code(1); /* parameters */
            if (f->function.symbols)
                push_op(n, op_new(CALL_O, name, n->place, count, e));
            else
                push_op(n, op_new(CALLC_O, name, n->place, count, e));
            break;
        }
        case EXPR_LIST: { /* recursive list of parameters */
            iter = list_head(t->children);
            while (!list_end(iter)) {
                struct node *child = iter->data;
                struct node_2 *n_ = child->data;
                n->code = list_concat(n->code, n_->code);
                /* if child has a place, add a param */
                struct address p = get_place(child, -1);
                if (p.region != UNKNOWN_R)
                    push_op(n, op_new(PARAM_O, NULL, p, e, e));
                iter = iter->next;
            }
            break;
        }
        case BREAK_STATEMENT: { /* mark a break statement */
            push_op(n, &break_op);
            break;
        }
        case CONTINUE_STATEMENT: { /* mark a continue statement */
            push_op(n, &continue_op);
            break;
        }
        case CASE_STATEMENT: { /* mark a case in a switch */
            /* this label stores the case in address[1]
             temporarily so that handle_switch() can get it */
            struct op *label = label_new();
            label->address[1] = get_place(t, 1);
            push_op(n, label);
            append_code(2);
            break;
        }
        case DEFAULT_STATEMENT: { /* mark a default case */
            push_op(n, &default_op);
            append_code(1);
            break;
        }
        case IF_STATEMENT: { /* if (test) { body } */
            struct op *first = label_new();
            struct op *follow = label_new();
            append_code(1); /* test */
            push_op(n, op_new(IF_O, NULL, get_place(t, 1), get_label(first), e));
            push_op(n, op_new(GOTO_O, NULL, get_label(follow), e, e));
            push_op(n, first);
            append_code(2); /* true */
            push_op(n, follow);
            break;
        }
        case IF_ELSE_STATEMENT: { /* if (test) { true } else { false } chains */
            struct op *first = label_new();
            struct op *follow = label_new();
            append_code(1); /* test */
            push_op(n, op_new(IF_O, NULL, get_place(t, 1), get_label(first), e));
            push_op(n, op_new(GOTO_O, NULL, get_label(follow), e, e));
            push_op(n, first);
            append_code(2); /* true */
            push_op(n, follow);
            append_code(4); /* false */
            break;
        }
        case SWITCH_STATEMENT: { /* switch (test) { body } */
            struct address s = get_place(t, 1);
            struct op *test = label_new();
            struct op *next = label_new();
            struct op *dflt = NULL;
            
            /* create test code list starting with test label */
            struct list *test_code = list_new(NULL, NULL);
            list_push_back(test_code, test);
            
            /* call search for labels, tests, and breaks */
            append_code(1); /* test */
            push_op(n, op_new(GOTO_O, NULL, get_label(test), e, e));
            dflt = handle_switch(get_code(t, 2), next,
                                 temp_new(&bool_type), s, test_code);
            append_code(2); /* body */
            
            /* concat test code and follow label */
            n->code = list_concat(n->code, test_code);
            /* push GOTO default if found */
            if (dflt)
                push_op(n, op_new(GOTO_O, NULL, get_label(dflt), e, e));
            push_op(n, next);
            
            break;
        }
        case WHILE_LOOP: { /* while (test) { body } */
            struct op *first = label_new();
            struct op *body = label_new();
            struct op *follow = label_new();
            push_op(n, first); /* before test */
            append_code(1); /* test */
            push_op(n, op_new(IF_O, NULL, get_place(t, 1), get_label(body), e));
            push_op(n, op_new(GOTO_O, NULL, get_label(follow), e, e));
            push_op(n, body);
            backpatch(get_code(t, 2), first, follow);
            append_code(2); /* body */
            push_op(n, op_new(GOTO_O, NULL, get_label(first), e, e));
            push_op(n, follow);
            break;
        }
        case DO_WHILE_LOOP: { /* do { body } while (test); */
            struct op *first = label_new();
            struct op *follow = label_new();
            push_op(n, first); /* before body */
            backpatch(get_code(t, 1), first, follow);
            append_code(1); /* body */
            append_code(3); /* test */
            push_op(n, op_new(IF_O, NULL, get_place(t, 3), get_label(first), e));
            push_op(n, follow);
            break;
        }
        case FOR_LOOP: { /* for (init; test; post) { body } */
            append_code(1); /* init */
            struct op *first = label_new();
            struct op *body = label_new();
            struct op *follow = label_new();
            push_op(n, first); /* before condition */
            append_code(2); /* test */
            push_op(n, op_new(IF_O, NULL, get_place(t, 2), get_label(body), e));
            push_op(n, op_new(GOTO_O, NULL, get_label(follow), e, e));
            push_op(n, body);
            backpatch(get_code(t, 4), first, follow);
            append_code(4); /* body */
            append_code(3); /* post */
            push_op(n, op_new(GOTO_O, NULL, get_label(first), e, e));
            push_op(n, follow);
            break;
        }
        case SHIFT_LEFT: { /* for cout << thing */
            append_code(0); /* recursive couts */
            append_code(1); /* thing */
            struct address p = get_place(t, 1);
            /* if std::string, call c_str() */
            if (p.type->base == CLASS_T
                && (strcmp(p.type->class.type, "string") == 0)) {
                struct address temp = temp_new(&string_type);
                struct address count = { CONST_R,
                    1,
                    &int_type };
                push_op(n, op_new(PARAM_O, NULL, p, e, e));
                push_op(n, op_new(CALL_O, "string__c_str", temp, count, e));
                p = temp;
            }
            push_op(n, op_new(map_c(p.type), NULL, p, e, e));
            break;
        }
        case REL_LT:
        case REL_GT:
        case REL_LTEQ:
        case REL_GTEQ:
        case NOTEQUAL_EXPR:
        case EQUAL_EXPR:
        case LOGICAL_OR_EXPR:
        case LOGICAL_AND_EXPR: { /* int and float comparisons */
            /* TODO: handle short circuiting */
            n->place = temp_new(&bool_type);
            struct address l = get_place(t, 0);
            append_code(0); /* left */
            struct address r = get_place(t, 2);
            append_code(2); /* right */
            push_op(n, op_new(map_c(l.type), NULL, n->place, l, r));
            break;
        }
        case ADD_EXPR:
        case SUB_EXPR:
        case MULT_EXPR:
        case DIV_EXPR:
        case MOD_EXPR: { /* int and float arithmetic */
            struct address l = get_place(t, 0);
            append_code(0); /* left */
            struct address r = get_place(t, 2);
            append_code(2); /* right */
            n->place = temp_new(l.type);
            push_op(n, op_new(map_c(l.type), NULL, n->place, l, r));
            break;
        }
        case UNARY_PLUSPLUS: /* unary increment and decrement */
        case UNARY_MINUSMINUS: {
            n->place = get_place(t, 1);
            push_op(n, op_new(map_c(n->place.type), NULL, n->place, n->place, one));
            break;
        }
        case UNARY_STAR: { /* dereference */
            append_code(1);
            /* stop if this is an assignment operand */
            if (get_rule(t->parent) == ASSIGN_EXPR) {
                n->place = get_place(t, 1);
                break;
            }
            /* otherwise dereference for value */
            struct typeinfo *type = typeinfo_copy(get_place(t, 1).type);
            type->pointer = false;
            n->place = temp_new(type);
            push_op(n, op_new(RSTAR_O, NULL, n->place, get_place(t, 1), e));
            break;
        }
        case UNARY_AMPERSAND: { /* address-of */
            n->place = temp_new(&ptr_type);
            append_code(1);
            push_op(n, op_new(ADDR_O, NULL, n->place, get_place(t, 1), e));
            break;
        }
        case UNARY_NOT: { /* logical not */
            n->place = temp_new(&bool_type);
            append_code(1);
            push_op(n, op_new(NOT_O, NULL, n->place, get_place(t, 1), e));
            break;
        }
        case UNARY_MINUS: { /* negative int or float */
            struct address p = get_place(t, 1);
            n->place = temp_new(p.type);
            append_code(1);
            push_op(n, op_new(map_c(p.type), NULL, n->place, p, e));
            break;
        }
        case UNARY_SIZEOF_EXPR:
        case UNARY_SIZEOF_TYPE: { /* sizeof(symbol or type) */
            /* obtain size as a const int */
            struct address size;
            size.region = CONST_R;
            size.type = &int_type;
            
            struct typeinfo *type = NULL;
            
            char *k = get_identifier(t);
            if (k) {
                /* perform symbol lookup if possible */
                type = typeinfo_copy(scope_search(k));
                /* if dereferencing, use size of value */
                if (get_pointer(t))
                    type->pointer = false;
                /* if accessing an index, use size of array element */
                if (get_rule(child(1)) == POSTFIX_ARRAY_INDEX) {
                    struct typeinfo *temp = typeinfo_copy(type->array.type);
                    free(type);
                    type = temp;
                }
            } else {
                /* get type specifier */
                struct node *type_spec = get_production(t, TYPE_SPEC_SEQ);
                if (type_spec == NULL)
                    log_semantic(t, "sizeof operator missing type spec");
                type = typeinfo_copy(type_check(tree_index(type_spec, 0)));
                /* check if given a pointer type */
                if (get_pointer(t))
                    type->pointer = true;
            }
            if (type == NULL) /* still a semantic error */
                log_semantic(t, "sizeof operator missing type");
            
            size.offset = typeinfo_size(type);
            free(type);
            n->place = size;
            break;
        }
        case POSTFIX_ARRAY_INDEX: { /* get address of value at array[index] */
            if (get_rule(t->parent) == UNARY_SIZEOF_EXPR)
                break;
            char *k = get_identifier(t);
            struct typeinfo *array = scope_search(k);
            struct address index = get_place(t, 2);
            char *name;
            asprintf(&name, "%s[%d]", k, index.offset);
            /* calculate real offset */
            struct address offset = temp_new(&int_type);
            struct address size = { CONST_R,
                typeinfo_size(array->array.type),
                &int_type };
            push_op(n, op_new(MUL_O, NULL, offset, index, size));
            
            if (get_rule(t->parent) == ASSIGN_EXPR) {
                struct typeinfo *temp = typeinfo_copy(array->array.type);
                temp->pointer = true;
                n->place = temp_new(temp);
                push_op(n, op_new(LARR_O, name, n->place, array->place, offset));
            } else {
                n->place = temp_new(array->array.type);
                push_op(n, op_new(RARR_O, name, n->place, array->place, offset));
            }
            break;
        }
        case POSTFIX_PLUSPLUS:
        case POSTFIX_MINUSMINUS: { /* unary ++ and -- operators */
            n->place = get_place(t, 0);
            push_op(n, op_new(map_c(n->place.type), NULL, n->place, n->place, one));
            break;
        }
        case ASSIGN_EXPR: { /* non-initializer assignment */
            char *k = get_identifier(t);
            char *name = k;
            n->place = get_place(t, 0);
            struct address r = get_place(t, 2);
            append_code(0); /* left */
            append_code(2); /* right */
            enum opcode code = ASN_O;
            enum rule c = get_rule(child(0));
            /* handle doing assignment with pointers */
            if (c == UNARY_STAR
                || c == POSTFIX_ARRAY_INDEX
                || c == POSTFIX_DOT_FIELD
                || c == POSTFIX_ARROW_FIELD)
                code = LSTAR_O;
            else if (get_rule(child(2)) == UNARY_STAR)
                code = RSTAR_O;
            
            if (c == POSTFIX_ARRAY_INDEX)
                asprintf(&name, "%s[]", k);
            else if (c == POSTFIX_DOT_FIELD)
                asprintf(&name, "%s.%s", k, get_identifier(tree_index(child(0), 2)));
            else if (c == POSTFIX_ARROW_FIELD)
                asprintf(&name, "%s->%s", k, get_identifier(tree_index(child(0), 2)));
            
            push_op(n, op_new(code, name, n->place, r, e));
            break;
        }
        case RETURN_STATEMENT: { /* return (optional expr) */
            if (get_node(t, 1)) {
                n->place = get_place(t, 1);
                append_code(1); /* expr */
                push_op(n, op_new(RET_O, NULL, n->place, e, e));
            }
            else {
                push_op(n, op_new(RET_O, NULL, e, e, e));
            }
            break;
        }
        case CTOR_FUNCTION_DEF: { /* constructor function definition */
            char *k = class_member(t);
            /* TODO: remember why I don't print these two */
            if (strcmp(k, "ifstream") == 0 || strcmp(k, "ofstream") == 0)
                break;
            char *name;
            asprintf(&name, "%s__%s", k, k);
            struct typeinfo *f = scope_search(k);
            log_assert(f);
            /* get size of parameters */
            struct address param = { CONST_R, f->function.param_size, &int_type };
            /* get size of locals (including temps) */
            struct address local = { CONST_R, offset, &int_type };
            struct address ret = { UNKNOWN_R, 0, f };
            /* ctors in generated code have void type */
            f->function.type = &void_type;
            push_op(n, op_new(PROC_O, name, param, local, ret));
            append_code(1);
            push_op(n, op_new(END_O, NULL, e, e, e));
            break;
        }
        case FUNCTION_DEF: { /* function definition */
            char *k = get_identifier(t);
            /* some funky stuff to get class::function string */
            char *class = class_member(t);
            char *name;
            if (class)
                asprintf(&name, "%s__%s", class, k);
            else
                asprintf(&name, "%s", k);
            struct typeinfo *f = scope_search(k);
            log_assert(f);
            /* get size of parameters */
            struct address param = { CONST_R, f->function.param_size, &int_type };
            /* get size of locals (including temps) */
            struct address local = { CONST_R, offset, &int_type };
            struct address ret = { UNKNOWN_R, 0, f };
            push_op(n, op_new(PROC_O, name, param, local, ret));
            append_code(2);
            push_op(n, op_new(END_O, NULL, e, e, e));
            break;
        }
        default: { /* concatenate all children code to build list */
            iter = list_head(t->children);
            while (!list_end(iter)) {
                struct node *child = iter->data;
                struct node_2 *n_ = child->data;
                // noop for NULL code
                n->code = list_concat(n->code, n_->code);
                iter = iter->next;
            }
        }
    }
    
    // handle exit of scopes
    if (scoped) {
        while (list_size(yyscopes) != scopes) {
            log_debug("popping scope");
            scope_pop();
        }
        
        // restore region and offset
        region = region_;
        offset = offset_;
    }
}


static struct op *handle_switch(struct list *code, struct op *next,
                                struct address temp, struct address test,
                                struct list *test_code)
{
    struct op *dflt = NULL;
    struct list_node *iter = list_head(code);
    while (!list_end(iter)) {
        struct op *op = iter->data;
        if (op->code == LABEL_O) {
            /* append IF(EQ(s, case), label), clear label */
            list_push_back(test_code, op_new(EQ_O, NULL, temp,
                                             test, op->address[1]));
            list_push_back(test_code, op_new(IF_O, NULL, temp,
                                             get_label(op), e));
            op->address[1] = e;
        } else if (op == &break_op) {
            /* replace marker with GOTO next for break statements */
            iter->data = op_new(GOTO_O, NULL, get_label(next), e, e);
        } else if (op == &default_op) {
            /* replace marker with default label and pass back */
            dflt = label_new();
            iter->data = dflt;
        }
        iter = iter->next;
    }
    return dflt;
}

static void backpatch(struct list *code, struct op *first, struct op *follow)
{
    struct list_node *iter = list_head(code);
    while (!list_end(iter)) {
        struct op *op = iter->data;
        if (follow && op == &break_op)
            iter->data = op_new(GOTO_O, NULL, get_label(follow), e, e);
        if (first && op == &continue_op)
            iter->data = op_new(GOTO_O, NULL, get_label(first), e, e);
        iter = iter->next;
    }
}
#undef append_code
#undef child
#undef map_c

/*
 * Maps a production rule to an opcode if supported.
 */
static enum opcode map_code(enum rule r, struct typeinfo *type)
{
    /* handle return types of functions */
    struct typeinfo *t = typeinfo_return(type);
    
    bool floating = t->base == FLOAT_T;
    switch (r) {
        case REL_LT:
            return floating ? FLT_O : LT_O; /* < */
        case REL_GT:
            return floating ? FGT_O : GT_O; /* > */
        case REL_LTEQ:
            return floating ? FLE_O : LE_O; /* <= */
        case REL_GTEQ:
            return floating ? FGE_O : GE_O; /* >= */
        case EQUAL_EXPR:
            return floating ? FEQ_O : EQ_O; /* == */
        case NOTEQUAL_EXPR:
            return floating ? FNE_O : NE_O; /* != */
        case LOGICAL_OR_EXPR:
            return OR_O;
        case LOGICAL_AND_EXPR:
            return AND_O;
        case ADD_EXPR:
        case UNARY_PLUSPLUS:
        case POSTFIX_PLUSPLUS:
            return floating ? FADD_O : ADD_O; /* + */
        case SUB_EXPR:
        case UNARY_MINUSMINUS:
        case POSTFIX_MINUSMINUS:
            return floating ? FSUB_O : SUB_O; /* - */
        case MULT_EXPR:
            return floating ? FMUL_O : MUL_O; /* * */
        case DIV_EXPR:
            return floating ? FDIV_O : DIV_O; /* / */
        case UNARY_MINUS:
            return floating ? FNEG_O : NEG_O;
        case MOD_EXPR:
            return MOD_O; /* % */
        case SHIFT_LEFT:
            if (t->base == INT_T)
                return PINT_O;
            else if (t->base == CHAR_T && !t->pointer)
                return PCHAR_O;
            else if (t->base == BOOL_T)
                return PBOOL_O;
            else if (t->base == FLOAT_T)
                return PFLOAT_O;
            else if (t->base == CHAR_T && t->pointer)
                return PSTR_O;
            else
                return ERRC_O;
        default:
            return ERRC_O; /* unknown */
    }
}

/*
 * Allocates a new op, assigns code, name, and 3 addresses. Pass 'e'
 * address for 'empty' address slots.
 */
static struct op *op_new(enum opcode code, char *name,
                         struct address a, struct address b, struct address c)
{
    struct op *op = malloc(sizeof(*op));
    if (op == NULL)
        log_error("op_new(): could not malloc op");
    
    op->code = code;
    op->name = name;
    op->address[0] = a;
    op->address[1] = b;
    op->address[2] = c;
    
    return op;
}

/*
 * Allocates a new code list on a node if necessary. Pushes op to back
 * of list.
 */
static void push_op(struct node_2 *n, struct op *op)
{
    if (n->code == NULL)
        n->code = list_new(NULL, NULL);
    log_assert(n->code);
    list_push_back(n->code, op);
}

/*
 * Return a new op with the LABEL pseudo code.
 *
 * Keeps track of the number of labels created. Labels are 0 indexed
 * in address region LABEL_R.
 */
static struct op *label_new()
{
    struct address p = { LABEL_R, yylabels, &unknown_type };
    ++yylabels;
    return op_new(LABEL_O, NULL, p, e, e);
}

/*
 * Given a type for size reference, return an address for a temporary
 * of the size of that type (incrementing the global offset).
 */
static struct address temp_new(struct typeinfo *t)
{
    struct address p = { region, offset, t };
    offset += typeinfo_size(t);
    return p;
}

/*
 * Returns code list given a child index on a tree if available.
 */
static struct list *get_code(struct node *t, int i)
{
    struct node_2 *n = get_node(t, i);
    
    if (n)
        return n->code;
    
    return NULL;
}

/*
 * Returns address given a child index on a tree. If negative, returns
 * own address. Looks up address based on token->text in scope if not
 * stored in the node.
 */
static struct address get_place(struct node *t, int i)
{
    struct node_2 *n = i > -1 ? get_node(t, i) : t->data;
    
    if (n) {
        if (n->place.region != UNKNOWN_R)
            return n->place;
        
        /* perform lookup if necessary */
        if (n->rule == TOKEN) {
            struct typeinfo *s = scope_search(n->token->text);
            if (s && s->place.region != UNKNOWN_R)
                return s->place;
        }
    }
    
    return e;
}

/*
 * Returns the label of an op (first address).
 */
static struct address get_label(struct op *op)
{
    log_assert(op->code == LABEL_O);
    return op->address[0];
}


void print_code(FILE *stream, struct list *code)
{
    struct list_node *iter = list_head(code);
    while (!list_end(iter) && iter != NULL) {
        if (iter->data){
            print_op(stream, iter->data);
        }
        iter = iter->next;
    }
}
#define R(rule) case rule: return #rule

static char *print_opcode(enum opcode code)
{
    switch (code) {
            R(PROC_O);
            R(END_O);
            R(PARAM_O);
            R(CALL_O);
            R(CALLC_O);
            R(RET_O);
            R(LABEL_O);
            R(GOTO_O);
            R(NEW_O);
            R(DEL_O);
            R(PINT_O);
            R(PCHAR_O);
            R(PBOOL_O);
            R(PFLOAT_O);
            R(PSTR_O);
            R(ADD_O);
            R(FADD_O);
            R(SUB_O);
            R(FSUB_O);
            R(MUL_O);
            R(FMUL_O);
            R(DIV_O);
            R(FDIV_O);
            R(MOD_O);
            R(LT_O);
            R(FLT_O);
            R(LE_O);
            R(FLE_O);
            R(GT_O);
            R(FGT_O);
            R(GE_O);
            R(FGE_O);
            R(EQ_O);
            R(FEQ_O);
            R(NE_O);
            R(FNE_O);
            R(OR_O);
            R(AND_O);
            R(NEG_O);
            R(FNEG_O);
            R(NOT_O);
            R(LSTAR_O);
            R(RSTAR_O);
            R(ASN_O);
            R(ADDR_O);
            R(LARR_O);
            R(RARR_O);
            R(LFIELD_O);
            R(RFIELD_O);
            R(IF_O);
            R(ERRC_O);
    }
    return NULL;
}
#undef R

/*
 * Given an op, prints it and its memory addresses.
 */
void print_op(FILE *stream, struct op *op)
{
    int i;
    fprintf(stream, "%-10s", print_opcode(op->code));
    fprintf(stream, "%-24s", op->name ? op->name : "");
    for (i = 0; i < 3; ++i) {
        struct address a = op->address[i];
        if (a.region != UNKNOWN_R) {
            print_address(stream, a);
            fprintf(stream, " ");
        }
    }
    fprintf(stream, "\n");
}

