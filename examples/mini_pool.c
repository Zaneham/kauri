/*
 * mini_pool.c: Symbol table using kauri pool allocation.
 *
 * Exercises: KA_PNEW, KA_CHK, ka_sdup, ka_str_t / ka_sfmt
 *
 * Build: gcc -std=c99 -Wall -DKAURI_DEBUG=1 -o mini_pool mini_pool.c
 */

#define KAURI_IMPL
#include "../kauri.h"
#include <stdio.h>

/* ---- Symbol Table ---- */

enum { TY_INT, TY_FLT, TY_STR };

static const char *ty_name[] = { "int", "float", "str" };

typedef struct {
    char    *name;    /* arena-owned */
    uint8_t  type;
    uint8_t  scope;
} sym_t;

#define MAX_SYM 16

typedef struct {
    sym_t    slots[MAX_SYM];
    uint32_t count;   /* slot 0 reserved as sentinel */
} symtab_t;

/* Add a symbol. Returns index or 0 on failure. */
static uint32_t
sym_add(symtab_t *tab, ka_arena_t *A,
        const char *name, uint8_t type, uint8_t scope)
{
    uint32_t idx = KA_PNEW(tab->count, MAX_SYM);
    if (idx == 0) {
        fprintf(stderr, "symbol table full\n");
        return 0;
    }
    tab->slots[idx].name  = ka_sdup(A, name, 0);
    tab->slots[idx].type  = type;
    tab->slots[idx].scope = scope;
    return idx;
}

/* Look up by index with bounds checking. */
static sym_t *
sym_get(symtab_t *tab, uint32_t idx)
{
    if (KA_CHK(idx, tab->count)) return NULL;
    return &tab->slots[idx];
}

/* ---- Main ---- */

int
main(void)
{
    static uint8_t buf[2048];
    ka_arena_t     A;
    symtab_t       tab;
    char           dump[512];
    ka_str_t       S;
    uint32_t       i;

    ka_init(&A, buf, sizeof(buf), 0);
    memset(&tab, 0, sizeof(tab));
    tab.count = 1;  /* reserve slot 0 */

    /* populate */
    sym_add(&tab, &A, "argc",    TY_INT, 0);
    sym_add(&tab, &A, "argv",    TY_STR, 0);
    sym_add(&tab, &A, "x",       TY_FLT, 1);
    sym_add(&tab, &A, "counter", TY_INT, 1);
    sym_add(&tab, &A, "msg",     TY_STR, 2);

    /* build formatted dump via ka_str_t */
    ka_sinit(&S, dump, sizeof(dump));
    ka_sfmt(&S, "Symbol Table (%u entries):\n", (unsigned)(tab.count - 1));
    ka_sfmt(&S, "%-4s  %-10s  %-6s  %s\n", "idx", "name", "type", "scope");
    ka_sfmt(&S, "----  ----------  ------  -----\n");

    for (i = 1; i < tab.count; i++) {
        sym_t *s = sym_get(&tab, i);
        if (!s) continue;
        ka_sfmt(&S, "%-4u  %-10s  %-6s  %u\n",
                (unsigned)i, s->name, ty_name[s->type], (unsigned)s->scope);
    }
    printf("%s", S.ptr);

    /* demonstrate KA_CHK on out-of-bounds access */
    printf("\nlookup idx=3: %s\n",
           sym_get(&tab, 3) ? sym_get(&tab, 3)->name : "(null)");
    printf("lookup idx=99 (OOB): %s\n",
           sym_get(&tab, 99) ? "(found?!)" : "(null, correctly rejected)");

    printf("\narena: %u / %u bytes\n",
           (unsigned)ka_used(&A), (unsigned)ka_cap(&A));
    ka_rst(&A);
    return 0;
}
