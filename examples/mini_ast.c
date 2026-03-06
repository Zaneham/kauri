/*
 * mini_ast.c: Parse and evaluate a simple arithmetic expression.
 *
 * Grammar: expr = term (('+' | '-') term)*
 *          term = atom (('*' | '/') atom)*
 *          atom = INTEGER | '(' expr ')'
 *
 * Exercises: KA_NEW, KA_CHAIN, ka_used/ka_cap, ka_peak, ka_free
 *
 * Build: gcc -std=c99 -Wall -DKAURI_DEBUG=1 -o mini_ast mini_ast.c
 */

#define KAURI_IMPL
#include "../kauri.h"
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>

/* ---- AST ---- */

enum { ND_INT, ND_BIN };

typedef struct nd_t {
    uint8_t      kind;
    char         op;       /* for ND_BIN: +, -, *, / */
    int          val;      /* for ND_INT */
    struct nd_t *lhs, *rhs;
} nd_t;

/* ---- Recursive Descent Parser ---- */

static const char *cur;   /* current position in input */

static void skip(void) { while (*cur == ' ') cur++; }

static nd_t *p_expr(ka_arena_t *A);

static nd_t *
p_atom(ka_arena_t *A)
{
    nd_t *n;
    skip();
    if (*cur == '(') {
        cur++;
        n = p_expr(A);
        skip();
        if (*cur == ')') cur++;
        return n;
    }
    if (isdigit((unsigned char)*cur)) {
        n = KA_NEW(A, nd_t);
        if (!n) return NULL;
        n->kind = ND_INT;
        n->val  = 0;
        KA_GUARD(g, 20);
        while (isdigit((unsigned char)*cur) && g--) {
            n->val = n->val * 10 + (*cur - '0');
            cur++;
        }
        n->lhs = n->rhs = NULL;
        return n;
    }
    return NULL;
}

static nd_t *
p_term(ka_arena_t *A)
{
    nd_t *left = p_atom(A);
    KA_GUARD(g, 64);
    skip();
    while ((*cur == '*' || *cur == '/') && g--) {
        nd_t *bin = KA_NEW(A, nd_t);
        if (!bin) return left;
        bin->kind = ND_BIN;
        bin->op   = *cur++;
        bin->lhs  = left;
        bin->rhs  = p_atom(A);
        left = bin;
        skip();
    }
    return left;
}

static nd_t *
p_expr(ka_arena_t *A)
{
    nd_t *left = p_term(A);
    KA_GUARD(g, 64);
    skip();
    while ((*cur == '+' || *cur == '-') && g--) {
        nd_t *bin = KA_NEW(A, nd_t);
        if (!bin) return left;
        bin->kind = ND_BIN;
        bin->op   = *cur++;
        bin->lhs  = left;
        bin->rhs  = p_term(A);
        left = bin;
        skip();
    }
    return left;
}

/* ---- Evaluator (iterative via explicit stack) ---- */

typedef struct { nd_t *nd; int state; int lv; } frame_t;
#define STK_MAX 64

static int
eval(nd_t *root)
{
    frame_t stk[STK_MAX];
    int     sp = 0, result = 0;

    if (!root) return 0;
    stk[0] = (frame_t){ root, 0, 0 };
    sp = 1;

    KA_GUARD(g, 512);
    while (sp > 0 && g--) {
        frame_t *f = &stk[sp - 1];
        if (f->nd->kind == ND_INT) {
            result = f->nd->val;
            sp--;
        } else if (f->state == 0) {
            /* push LHS */
            f->state = 1;
            if (sp < STK_MAX && f->nd->lhs) {
                stk[sp] = (frame_t){ f->nd->lhs, 0, 0 };
                sp++;
            }
        } else if (f->state == 1) {
            /* LHS done, save it, push RHS */
            f->lv = result;
            f->state = 2;
            if (sp < STK_MAX && f->nd->rhs) {
                stk[sp] = (frame_t){ f->nd->rhs, 0, 0 };
                sp++;
            }
        } else {
            /* both done, apply op */
            int rv = result;
            switch (f->nd->op) {
                case '+': result = f->lv + rv; break;
                case '-': result = f->lv - rv; break;
                case '*': result = f->lv * rv; break;
                case '/': result = rv ? f->lv / rv : 0; break;
            }
            sp--;
        }
    }
    return result;
}

/* ---- Pretty Print (iterative) ---- */

static void
pr_ast(nd_t *root)
{
    /* DFS with explicit stack, bracket notation */
    nd_t *stk[STK_MAX];
    int   act[STK_MAX];  /* 0=enter, 1=mid, 2=leave */
    int   sp = 0;

    if (!root) return;
    stk[0] = root; act[0] = 0; sp = 1;

    KA_GUARD(g, 512);
    while (sp > 0 && g--) {
        nd_t *n = stk[sp - 1];
        int   a = act[sp - 1];
        if (n->kind == ND_INT) {
            printf("%d", n->val);
            sp--;
        } else if (a == 0) {
            printf("(");
            act[sp - 1] = 1;
            if (sp < STK_MAX && n->lhs) {
                stk[sp] = n->lhs; act[sp] = 0; sp++;
            }
        } else if (a == 1) {
            printf(" %c ", n->op);
            act[sp - 1] = 2;
            if (sp < STK_MAX && n->rhs) {
                stk[sp] = n->rhs; act[sp] = 0; sp++;
            }
        } else {
            printf(")");
            sp--;
        }
    }
}

/* ---- Main ---- */

int
main(int argc, char **argv)
{
    /* small initial buffer + chaining = exercises KA_CHAIN */
    static uint8_t buf[128];
    ka_arena_t     A;
    nd_t          *ast;
    const char    *input;
    int            val;

    input = (argc > 1) ? argv[1] : "3 + 4 * (10 - 2) / 2";
    printf("input: \"%s\"\n\n", input);

    ka_init(&A, buf, sizeof(buf), KA_CHAIN);
    cur = input;
    ast = p_expr(&A);

    if (!ast) {
        fprintf(stderr, "parse failed\n");
        ka_free(&A);
        return 1;
    }

    printf("ast:   ");
    pr_ast(ast);
    printf("\n");

    val = eval(ast);
    printf("eval:  %d\n", val);

    printf("\nmemory: %u used / %u cap\n",
           (unsigned)ka_used(&A), (unsigned)ka_cap(&A));
    printf("peak allocs: %u (debug only, 0 in release)\n",
           (unsigned)ka_peak(&A));

    ka_free(&A);
    printf("freed. done.\n");
    return 0;
}
