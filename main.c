#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>

#define NOB_IMPLEMENTATION
#include "nob.h"
#define RC_IMPLEMENTATION
#include "rc.h"

typedef enum {
    EXPR_NIL,
    EXPR_SYMBOL,
    EXPR_INTEGER,
    EXPR_PAIR,
    __count_expr_kind,
} Expr_Kind;

typedef struct Expr Expr;

struct Expr {
    Expr_Kind kind;
    static_assert(__count_expr_kind == 4, "Amount of expression kinds has changed.");
    union {
        const char *symbol;
        int integer;
        struct {
            Expr *left;
            Expr *right;
        } pair;
    };
};

void destroy_expr(void *data)
{
    Expr *expr = data;
    switch (expr->kind) {
    case EXPR_NIL:
    case EXPR_SYMBOL:
    case EXPR_INTEGER:
        break;
    case EXPR_PAIR:
        rc_release(expr->pair.left);
        rc_release(expr->pair.right);
        break;
    case __count_expr_kind:
    default:
        UNREACHABLE("Expr_Kind");
    }
}

Expr *alloc_expr(Expr_Kind kind)
{
    Expr *expr = rc_alloc(sizeof(Expr), destroy_expr);
    expr->kind = kind;
    return expr;
}

static_assert(__count_expr_kind == 4, "Amount of expression kinds has changed. Change the ctors.");

Expr *make_nil(void)
{
    return alloc_expr(EXPR_NIL);
}

Expr *make_symbol(const char *symbol)
{
    Expr *expr = alloc_expr(EXPR_SYMBOL);
    expr->symbol = symbol;
    return expr;
}

Expr *make_integer(int integer)
{
    Expr *expr = alloc_expr(EXPR_INTEGER);
    expr->integer = integer;
    return expr;
}

Expr *make_pair(Expr *left, Expr *right)
{
    Expr *expr = alloc_expr(EXPR_PAIR);
    expr->pair.left  = left;
    expr->pair.right = right;
    return expr;
}

#define dump_expr(expr) dump_expr_opt(expr, 0)
void dump_expr_opt(Expr *expr, int level)
{
    for (int i = 0; i < level; ++i) {
        printf("  ");
    }
    switch (expr->kind) {
    case EXPR_NIL: {
        printf("NIL(%ld) %p\n", rc_count(expr), expr);
    } break;
    case EXPR_SYMBOL: {
        printf("SYMBOL(%ld) %p: %s\n", rc_count(expr), expr, expr->symbol);
    } break;
    case EXPR_INTEGER: {
        printf("INTEGER(%ld) %p: %d\n", rc_count(expr), expr, expr->integer);
    } break;
    case EXPR_PAIR: {
        printf("PAIR(%ld) %p:\n", rc_count(expr), expr);
        dump_expr_opt(expr->pair.left,  level + 1);
        dump_expr_opt(expr->pair.right, level + 1);
    } break;
    case __count_expr_kind:
    default: UNREACHABLE("Expr_Kind");
    }
}

Expr *args_to_list(va_list args)
{
    Expr *arg = va_arg(args, Expr *);
    if (arg->kind == EXPR_NIL) return arg;
    return make_pair(arg, args_to_list(args));
}

#define make_list(...) make_list_impl(NULL, __VA_ARGS__, make_nil())
Expr *make_list_impl(void *first, ...)
{
    va_list args;
    va_start(args, first);
    Expr *list = args_to_list(args);
    va_end(args);
    return list;
}

Expr *eval(Expr *expr)
{
    if (expr->kind == EXPR_PAIR) {
        assert(expr->pair.left->kind == EXPR_SYMBOL);
        if (strcmp(expr->pair.left->symbol, "+") == 0) {
            int result = 0;
            Expr *args = expr->pair.right;
            while (args->kind != EXPR_NIL) {
                assert(args->kind == EXPR_PAIR);
                assert(args->pair.left->kind == EXPR_INTEGER);
                result += args->pair.left->integer;
                args = args->pair.right;
            }
            return make_integer(result);
        } else if (strcmp(expr->pair.left->symbol, "swap") == 0) {
            Expr *args = expr->pair.right;
            assert(args->kind == EXPR_PAIR);
            assert(args->pair.left->kind == EXPR_PAIR);
            assert(args->pair.right->kind == EXPR_NIL);
            Expr *arg = args->pair.left;
            return make_pair(rc_acquire(arg->pair.right), rc_acquire(arg->pair.left));
        } else {
            TODO(temp_sprintf("Report unknown function error: %s", expr->pair.left->symbol));
        }
    } else {
        return expr;
    }
}

int main()
{
    printf("--- expr ---\n");
        // (swap . ((69 . 420) . nil))
        // (69 . 420)
        Expr *expr = make_list(
            make_symbol("swap"),
            make_pair(
                make_integer(69),
                make_integer(420)));
        printf("expr =\n");
        dump_expr_opt(expr, 1);
    printf("--- eval ---\n");
        Expr *result = eval(expr);
        printf("expr =\n");
        dump_expr_opt(expr, 1);
        printf("result =\n");
        dump_expr_opt(result, 1);
    printf("--- release expr ---\n");
        rc_release(expr);
        printf("result =\n");
        dump_expr_opt(result, 1);
    printf("--- dump expr ---\n");
        dump_expr(result);
    printf("--- release eval result ---\n");
        rc_release(result);
    return 0;
}
