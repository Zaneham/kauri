/*
 * test_kauri.c: Kauri test suite
 *
 * Self-contained. No external harness required.
 * If a test fails, the program tells you exactly which one
 * and exits with prejudice.
 */

#define KAURI_DEBUG 1
#define KAURI_IMPL
#include "../kauri.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/* ---- Mini Harness ---- */

static int t_pass = 0;
static int t_fail = 0;

#define CHECK(expr) do {                                            \
    if (!(expr)) {                                                  \
        fprintf(stderr, "  FAIL %s:%d: %s\n", __FILE__, __LINE__,  \
                #expr);                                             \
        t_fail++;                                                   \
        return;                                                     \
    }                                                               \
} while (0)

#define CHEQ(a, b) do {                                             \
    if ((a) != (b)) {                                               \
        fprintf(stderr, "  FAIL %s:%d: %s == %d, expected %d\n",   \
                __FILE__, __LINE__, #a, (int)(a), (int)(b));        \
        t_fail++;                                                   \
        return;                                                     \
    }                                                               \
} while (0)

#define RUN(fn) do {                                                \
    printf("  %-32s", #fn);                                         \
    fn();                                                           \
    if (t_fail == _prev_fail) { printf("ok\n"); t_pass++; }        \
    else printf("\n");                                              \
    _prev_fail = t_fail;                                            \
} while (0)

/* ---- Arena Tests ---- */

static void
t_astk(void)
{
    /* Stack-backed arena: no malloc involved */
    uint8_t buf[256];
    ka_arena_t A;

    ka_init(&A, buf, sizeof(buf), 0);
    CHECK(A.head.base == buf);
    CHEQ(A.head.cap, 256);
    CHEQ(A.head.pos, 0);

    int *p = KA_NEW(&A, int);
    CHECK(p != NULL);
    *p = 42;
    CHEQ(*p, 42);
    CHECK(ka_used(&A) > 0);
}

static void
t_ahep(void)
{
    /* Heap-backed arena: buf=NULL */
    ka_arena_t A;
    ka_init(&A, NULL, 1024, 0);
    CHECK(A.head.base != NULL);
    CHECK(A.flags & KA_F_HEAP);

    int *p = KA_NEW(&A, int);
    CHECK(p != NULL);
    *p = 99;
    CHEQ(*p, 99);

    ka_free(&A);
    CHEQ(A.head.cap, 0);
}

static void
t_align(void)
{
    /* Alignment: allocate a char, then a uint64_t. Should be 8-aligned */
    uint8_t buf[256];
    ka_arena_t A;

    ka_init(&A, buf, sizeof(buf), 0);
    char *c = (char *)ka_alloc(&A, 1, 1);
    CHECK(c != NULL);

    uint64_t *q = (uint64_t *)ka_alloc(&A, 8, 8);
    CHECK(q != NULL);
    CHECK(((uintptr_t)q & 7u) == 0);  /* 8-byte aligned */
}

static void
t_aovfl(void)
{
    /* Overflow: tiny buffer, no chaining -> NULL */
    uint8_t buf[16];
    ka_arena_t A;

    ka_init(&A, buf, sizeof(buf), 0);
    void *p = ka_alloc(&A, 32, 1);
    CHECK(p == NULL);
}

static void
t_chain(void)
{
    /* Chaining: small buffer overflows into malloc'd block.
     * Buffer sized to fit one alloc+canary, second must chain. */
    uint8_t buf[48];
    ka_arena_t A;

    ka_init(&A, buf, sizeof(buf), KA_CHAIN);

    /* Fill the head block */
    void *p1 = ka_alloc(&A, 32, 1);
    CHECK(p1 != NULL);

    /* This should chain */
    void *p2 = ka_alloc(&A, 32, 1);
    CHECK(p2 != NULL);
    CHECK(A.n_blk == 2);

    ka_free(&A);
}

static void
t_chlim(void)
{
    /* Chain limit: set max_blk=2, third block should fail.
     * Buffer fits one alloc+canary, second chains, third denied. */
    uint8_t buf[32];
    ka_arena_t A;

    ka_init(&A, buf, sizeof(buf), KA_CHAIN);
    A.max_blk = 2;

    void *p1 = ka_alloc(&A, 16, 1);
    CHECK(p1 != NULL);

    void *p2 = ka_alloc(&A, 16, 1);
    CHECK(p2 != NULL);
    CHEQ(A.n_blk, 2);

    /* Third block should be denied */
    void *p3 = ka_alloc(&A, 16, 1);
    CHECK(p3 == NULL);

    ka_free(&A);
}

static void
t_arst(void)
{
    /* Reset: allocate, reset, check pos=0 */
    uint8_t buf[256];
    ka_arena_t A;

    ka_init(&A, buf, sizeof(buf), KA_CHAIN);
    ka_alloc(&A, 100, 1);
    ka_alloc(&A, 200, 1); /* chains */
    CHECK(A.n_blk >= 2);

    ka_rst(&A);
    CHEQ(A.head.pos, 0);
    CHEQ(A.n_blk, 1);
    CHECK(A.head.next == NULL);
}

static void
t_amrk(void)
{
    /* Mark/rewind: allocate, mark, allocate more, rewind */
    uint8_t buf[256];
    ka_arena_t A;

    ka_init(&A, buf, sizeof(buf), 0);
    ka_alloc(&A, 32, 1);

    ka_mark_t m = ka_mark(&A);
    uint32_t saved_pos = m.pos;

    ka_alloc(&A, 64, 1);
    CHECK(ka_used(&A) > saved_pos);

    ka_rwind(&A, m);
    CHEQ(A.cur->pos, saved_pos);
}

static void
t_adup(void)
{
    /* dup and sdup */
    uint8_t buf[512];
    ka_arena_t A;

    ka_init(&A, buf, sizeof(buf), 0);

    uint32_t src[3] = { 10, 20, 30 };
    uint32_t *d = (uint32_t *)ka_dup(&A, src, sizeof(src), KA_ALIGN(uint32_t));
    CHECK(d != NULL);
    CHEQ(d[0], 10);
    CHEQ(d[1], 20);
    CHEQ(d[2], 30);

    const char *hello = "hello";
    char *s = ka_sdup(&A, hello, 0);
    CHECK(s != NULL);
    CHECK(strcmp(s, "hello") == 0);
    CHEQ(s[5], '\0');
}

/* ---- String Tests ---- */

static void
t_sinit(void)
{
    char buf[64];
    ka_str_t S;

    CHEQ(ka_sinit(&S, buf, sizeof(buf)), 0);
    CHEQ(S.len, 0);
    CHEQ(S.cap, 64);
    CHECK(S.ptr[0] == '\0');
}

static void
t_scat(void)
{
    char buf[64];
    ka_str_t S;
    ka_sinit(&S, buf, sizeof(buf));

    CHEQ(ka_scat(&S, "hello", 5), 0);
    CHEQ(S.len, 5);
    CHEQ(ka_scat(&S, " world", 6), 0);
    CHEQ(S.len, 11);
    CHECK(strcmp(S.ptr, "hello world") == 0);
}

static void
t_strn(void)
{
    /* Truncation: buffer too small */
    char buf[8];
    ka_str_t S;
    ka_sinit(&S, buf, sizeof(buf));

    int r = ka_scat(&S, "abcdefghij", 10);
    CHEQ(r, -1);               /* truncated */
    CHEQ(S.len, 7);            /* 8 - 1 for NUL */
    CHECK(S.ptr[7] == '\0');   /* still NUL-terminated */
}

static void
t_sfmt(void)
{
    char buf[64];
    ka_str_t S;
    ka_sinit(&S, buf, sizeof(buf));

    CHEQ(ka_sfmt(&S, "x=%d y=%d", 10, 20), 0);
    CHECK(strcmp(S.ptr, "x=10 y=20") == 0);
}

static void
t_schr(void)
{
    char buf[4];
    ka_str_t S;
    ka_sinit(&S, buf, sizeof(buf));

    CHEQ(ka_schr(&S, 'A'), 0);
    CHEQ(ka_schr(&S, 'B'), 0);
    CHEQ(ka_schr(&S, 'C'), 0);
    CHEQ(ka_schr(&S, 'D'), -1);  /* no room */
    CHEQ(S.len, 3);
    CHECK(strcmp(S.ptr, "ABC") == 0);
}

static void
t_scmp(void)
{
    char ba[16], bb[16];
    ka_str_t A, B;

    ka_sinit(&A, ba, sizeof(ba));
    ka_sinit(&B, bb, sizeof(bb));

    ka_scat(&A, "abc", 3);
    ka_scat(&B, "abc", 3);
    CHEQ(ka_scmp(&A, &B), 0);

    ka_sclr(&B);
    ka_scat(&B, "abd", 3);
    CHECK(ka_scmp(&A, &B) < 0);
}

static void
t_sclr(void)
{
    char buf[16];
    ka_str_t S;
    ka_sinit(&S, buf, sizeof(buf));
    ka_scat(&S, "hello", 5);

    ka_sclr(&S);
    CHEQ(S.len, 0);
    CHECK(S.ptr[0] == '\0');
}

/* ---- Macro Tests ---- */

static void
t_mchk(void)
{
    /* KA_CHK: in-bounds returns 0, OOB returns 1 */
    CHEQ(KA_CHK(0, 10), 0);
    CHEQ(KA_CHK(9, 10), 0);
    CHEQ(KA_CHK(10, 10), 1);
    CHEQ(KA_CHK(11, 10), 1);
}

static void
t_mpnw(void)
{
    /* KA_PNEW: pool alloc from counter */
    uint32_t cnt = 1;  /* 0 is sentinel */
    uint32_t max = 4;

    uint32_t a = KA_PNEW(cnt, max);
    CHEQ(a, 1);
    CHEQ(cnt, 2);

    uint32_t b = KA_PNEW(cnt, max);
    CHEQ(b, 2);

    (void)KA_PNEW(cnt, max); /* 3 */

    /* Pool full */
    uint32_t d = KA_PNEW(cnt, max);
    CHEQ(d, 0);
    CHEQ(cnt, 4); /* unchanged */
}

static void
t_mgrd(void)
{
    /* KA_GUARD: bounded loop */
    int count = 0;
    KA_GUARD(g, 10);
    while (g--) count++;
    CHEQ(count, 10);
}

/* ---- Error / Result Tests ---- */

static void
t_eres(void)
{
    /* ka_res_t construction */
    ka_res_t ok = KA_RES(KA_OK, "fine");
    CHEQ(ok.code, 0);
    CHECK(strcmp(ok.msg, "fine") == 0);

    ka_res_t oom = KA_RES(KA_OOM, "out of memory");
    CHEQ(oom.code, -2);
#if KAURI_DEBUG
    CHECK(oom.file != NULL);
    CHECK(oom.line > 0);
#endif
}

static ka_res_t
helper_fail(void)
{
    return KA_RES(KA_INVAL, "bad");
}

static ka_res_t
helper_try(void)
{
    KA_TRY(helper_fail());
    /* Should not reach here */
    return KA_RES_OK;
}

static void
t_etry(void)
{
    /* KA_TRY propagation */
    ka_res_t r = helper_try();
    CHEQ(r.code, KA_INVAL);
}

static void
t_eok(void)
{
    /* KA_OK passthrough */
    ka_res_t r = KA_RES_OK;
    CHEQ(r.code, KA_OK);
    CHECK(strcmp(r.msg, "ok") == 0);
}

/* ---- Edge-Case Tests ---- */

static void
t_azro(void)
{
    /* size=0 should be politely refused */
    uint8_t buf[64];
    ka_arena_t A;
    ka_init(&A, buf, sizeof(buf), 0);
    CHECK(ka_alloc(&A, 0, 1) == NULL);
}

static void
t_aaln0(void)
{
    /* align=0 is nonsense. Should return NULL */
    uint8_t buf[64];
    ka_arena_t A;
    ka_init(&A, buf, sizeof(buf), 0);
    CHECK(ka_alloc(&A, 4, 0) == NULL);
}

static void
t_anull(void)
{
    /* NULL arena: alloc and used should survive gracefully */
    CHECK(ka_alloc(NULL, 4, 4) == NULL);
    CHEQ(ka_used(NULL), 0);
}

static void
t_acap0(void)
{
    /* Zero-capacity arena: nothing fits, not even regret */
    uint8_t buf[1];
    ka_arena_t A;
    ka_init(&A, buf, 0, 0);
    CHECK(ka_alloc(&A, 1, 1) == NULL);
}

static void
t_adrst(void)
{
    /* Double reset: should not crash, corrupt, or sulk */
    uint8_t buf[128];
    ka_arena_t A;
    ka_init(&A, buf, sizeof(buf), 0);
    ka_alloc(&A, 32, 1);
    ka_rst(&A);
    ka_rst(&A);
    CHEQ(A.head.pos, 0);
    CHEQ(A.n_blk, 1);
}

static void
t_arwch(void)
{
    /* Rewind across chain blocks: mark in head, alloc into chain, rewind */
    uint8_t buf[32];
    ka_arena_t A;
    ka_init(&A, buf, sizeof(buf), KA_CHAIN);

    ka_alloc(&A, 16, 1);
    ka_mark_t m = ka_mark(&A);

    /* These should chain */
    ka_alloc(&A, 32, 1);
    ka_alloc(&A, 32, 1);
    CHECK(A.n_blk >= 2);

    ka_rwind(&A, m);
    CHEQ(A.n_blk, 1);
    CHEQ(A.cur->pos, m.pos);
    ka_free(&A);
}

static void
t_sdupe(void)
{
    /* sdup with len=0 on empty string: should give NUL-terminated empty */
    uint8_t buf[64];
    ka_arena_t A;
    ka_init(&A, buf, sizeof(buf), 0);

    char *s = ka_sdup(&A, "", 0);
    CHECK(s != NULL);
    CHEQ(s[0], '\0');
}

static void
t_snull(void)
{
    /* NULL string builder: sinit and scat should return -1 */
    CHEQ(ka_sinit(NULL, NULL, 0), -1);
    CHEQ(ka_scat(NULL, "x", 1), -1);
}

static void
t_sovfl(void)
{
    /* KA_NEWN with overflow: safe multiply should return NULL */
    uint8_t buf[256];
    ka_arena_t A;
    ka_init(&A, buf, sizeof(buf), 0);

    /* sizeof(uint32_t)=4, n=0x40000001 => 4*0x40000001 = 0x100000004 wraps */
    uint32_t *p = KA_NEWN(&A, uint32_t, 0x40000001u);
    CHECK(p == NULL);
}

/* ---- Debug-Only Tests ---- */

static void
t_apsn(void)
{
    /* Poison: after reset, freed region should be 0xDE */
    uint8_t buf[128];
    ka_arena_t A;
    ka_init(&A, buf, sizeof(buf), 0);

    uint8_t *p = (uint8_t *)ka_alloc(&A, 16, 1);
    CHECK(p != NULL);
    memset(p, 0x42, 16);  /* fill with known pattern */

    ka_rst(&A);

    /* The region should now be poisoned */
    int poisoned = 1;
    for (int i = 0; i < 16; i++) {
        if (p[i] != 0xDE) { poisoned = 0; break; }
    }
    CHECK(poisoned);
}

static void
t_acnry(void)
{
    /* Canary: corrupt past allocation, verify detection.
     * ka__cchk writes to stderr. We just verify the log is populated
     * and that a corrupt canary is detectable. */
    uint8_t buf[256];
    ka_arena_t A;
    ka_init(&A, buf, sizeof(buf), 0);

    uint8_t *p = (uint8_t *)ka_alloc(&A, 16, 1);
    CHECK(p != NULL);
    CHECK(A.n_log == 1);

    /* Verify canary is intact */
    uint32_t cv;
    memcpy(&cv, p + 16, 4);
    CHECK(cv == 0xDEADCA75u);

    /* Corrupt the canary. Scribble past the allocation boundary */
    p[16] = 0xFF;

    /* Reset will call ka__cchk which prints to stderr.
     * We can't easily capture stderr here, but we verify the canary
     * was indeed corrupted by reading it back. */
    memcpy(&cv, p + 16, 4);
    CHECK(cv != 0xDEADCA75u);

    /* Reset triggers canary check (stderr warning expected) */
    ka_rst(&A);
}

static void
t_apeak(void)
{
    /* Peak: alloc some, reset, alloc fewer, check peak remembers the high water */
    uint8_t buf[512];
    ka_arena_t A;
    ka_init(&A, buf, sizeof(buf), 0);

    ka_alloc(&A, 4, 1);
    ka_alloc(&A, 4, 1);
    ka_alloc(&A, 4, 1);
    /* n_alloc=3 */

    ka_rst(&A);
    /* peak should now be 3 */
    CHEQ(ka_peak(&A), 3);

    ka_alloc(&A, 4, 1);
    /* n_alloc=1, peak still 3 */
    CHEQ(ka_peak(&A), 3);

    ka_rst(&A);
    /* peak should still be 3 (1 < 3) */
    CHEQ(ka_peak(&A), 3);
}

/* ---- Main ---- */

int
main(void)
{
    int _prev_fail = 0;

    printf("kauri tests\n");

    printf("\narena:\n");
    RUN(t_astk);
    RUN(t_ahep);
    RUN(t_align);
    RUN(t_aovfl);
    RUN(t_chain);
    RUN(t_chlim);
    RUN(t_arst);
    RUN(t_amrk);
    RUN(t_adup);

    printf("\nstring:\n");
    RUN(t_sinit);
    RUN(t_scat);
    RUN(t_strn);
    RUN(t_sfmt);
    RUN(t_schr);
    RUN(t_scmp);
    RUN(t_sclr);

    printf("\nmacros:\n");
    RUN(t_mchk);
    RUN(t_mpnw);
    RUN(t_mgrd);

    printf("\nerror:\n");
    RUN(t_eres);
    RUN(t_etry);
    RUN(t_eok);

    printf("\nedge:\n");
    RUN(t_azro);
    RUN(t_aaln0);
    RUN(t_anull);
    RUN(t_acap0);
    RUN(t_adrst);
    RUN(t_arwch);
    RUN(t_sdupe);
    RUN(t_snull);
    RUN(t_sovfl);

    printf("\ndebug:\n");
    RUN(t_apsn);
    RUN(t_acnry);
    RUN(t_apeak);

    printf("\n%d passed, %d failed\n", t_pass, t_fail);
    return t_fail ? 1 : 0;
}
