/*
 * mini_lex.c: Tokenize a hardcoded expression using kauri arenas.
 *
 * Exercises: KA_NEWN, ka_sdup, ka_mark/ka_rwind, KA_GUARD
 *
 * Build: gcc -std=c99 -Wall -DKAURI_DEBUG=1 -o mini_lex mini_lex.c
 */

#define KAURI_IMPL
#include "../kauri.h"
#include <stdio.h>
#include <ctype.h>

/* ---- Token Kinds ---- */

enum {
    TK_EOF, TK_IDENT, TK_INT,
    TK_PLUS, TK_MINUS, TK_STAR, TK_SLASH
};

static const char *tk_name[] = {
    "EOF", "IDENT", "INT", "+", "-", "*", "/"
};

typedef struct {
    uint8_t  kind;
    char    *text;   /* arena-owned string */
} tok_t;

/* ---- Lexer ---- */

#define MAX_TOK 32

static uint32_t
lex(ka_arena_t *A, const char *src, tok_t *toks)
{
    uint32_t  n = 0;
    const char *p = src;
    KA_GUARD(g, 256);

    while (*p && n < MAX_TOK && g--) {
        /* skip whitespace */
        while (*p == ' ' || *p == '\t') p++;
        if (!*p) break;

        if (isalpha((unsigned char)*p) || *p == '_') {
            /* identifier — speculative lookahead demo:
             * mark before scanning, rewind if we wanted to reject */
            ka_mark_t m = ka_mark(A);
            const char *start = p;
            while (isalnum((unsigned char)*p) || *p == '_') p++;
            toks[n].kind = TK_IDENT;
            toks[n].text = ka_sdup(A, start, (uint32_t)(p - start));
            if (!toks[n].text) { ka_rwind(A, m); break; }
            n++;
        } else if (isdigit((unsigned char)*p)) {
            const char *start = p;
            while (isdigit((unsigned char)*p)) p++;
            toks[n].kind = TK_INT;
            toks[n].text = ka_sdup(A, start, (uint32_t)(p - start));
            n++;
        } else {
            /* single-char operators */
            char  ch = *p++;
            uint8_t k = TK_EOF;
            switch (ch) {
                case '+': k = TK_PLUS;  break;
                case '-': k = TK_MINUS; break;
                case '*': k = TK_STAR;  break;
                case '/': k = TK_SLASH; break;
                default:
                    fprintf(stderr, "unexpected '%c'\n", ch);
                    continue;
            }
            toks[n].kind = k;
            toks[n].text = ka_sdup(A, &ch, 1);
            n++;
        }
    }
    /* sentinel */
    toks[n].kind = TK_EOF;
    toks[n].text = NULL;
    return n;
}

/* ---- Main ---- */

int
main(void)
{
    static uint8_t buf[4096];
    ka_arena_t     A;
    tok_t         *toks;
    uint32_t       n, i;

    ka_init(&A, buf, sizeof(buf), 0);

    toks = KA_NEWN(&A, tok_t, MAX_TOK + 1);
    if (!toks) { fprintf(stderr, "OOM\n"); return 1; }

    printf("input: \"foo + bar * 123 - baz\"\n\n");
    n = lex(&A, "foo + bar * 123 - baz", toks);

    printf("%-6s  %-8s  %s\n", "idx", "kind", "text");
    printf("------  --------  ----\n");
    for (i = 0; i < n; i++) {
        printf("%-6u  %-8s  %s\n",
               (unsigned)i, tk_name[toks[i].kind], toks[i].text);
    }

    printf("\narena: %u / %u bytes used\n",
           (unsigned)ka_used(&A), (unsigned)ka_cap(&A));

    /* demonstrate rewind: mark, lex again, rewind */
    ka_mark_t m = ka_mark(&A);
    (void)lex(&A, "x + y", toks);
    printf("after second lex: %u bytes used\n", (unsigned)ka_used(&A));
    ka_rwind(&A, m);
    printf("after rewind:     %u bytes used\n", (unsigned)ka_used(&A));

    ka_rst(&A);
    printf("after reset:      %u bytes used\n", (unsigned)ka_used(&A));
    return 0;
}
