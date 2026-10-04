/* stdlib.c -- Tiny-C library: the code behind <stdlib.h> */
#include "libint.h"
#include <psys.h>
#pragma nofltused
unsigned *__freelist;
void *malloc(size_t n)
{
    unsigned *p;
    unsigned *prev;
    unsigned *best;
    unsigned *bprev;
    unsigned w;
    char *np;
    w = (n + 1) / 2 + 2;                /* header + link space */
    best = NULL;
    bprev = NULL;
    prev = NULL;
    for (p = __freelist; p; p = (unsigned *)p[1]) {
        if (p[0] >= w && (!best || p[0] < best[0])) {
            best = p;
            bprev = prev;
        }
        prev = p;
    }
    if (best) {
        if (bprev)
            bprev[1] = best[1];
        else
            __freelist = (unsigned *)best[1];
        return best + 1;
    }
    if (__cspi(40) < (int)w + 300)      /* MEMAVAIL: keep room for the stack */
        return NULL;
    __cspv(1, &np, w);                  /* NEW */
    p = (unsigned *)np;
    p[0] = w;
    return p + 1;
}

void free(void *v)
{
    unsigned *p;
    if (!v)
        return;
    p = (unsigned *)v - 1;
    p[1] = (unsigned)__freelist;
    __freelist = p;
}

void *calloc(size_t n, size_t size)
{
    void *p;
    p = malloc(n * size);
    if (p)
        memset(p, 0, n * size);
    return p;
}

void *realloc(void *v, size_t n)
{
    unsigned *p;
    void *q;
    size_t old;
    if (!v)
        return malloc(n);
    p = (unsigned *)v - 1;
    old = (p[0] - 1) * 2;
    if (old >= n)
        return v;
    q = malloc(n);
    if (q) {
        memcpy(q, v, old);
        free(v);
    }
    return q;
}

int abs(int x)
{
    return x < 0 ? -x : x;
}

long labs(long x)
{
    return x < 0 ? -x : x;
}

div_t div(int a, int b)
{
    div_t r;
    r.quot = a / b;
    r.rem = a % b;
    return r;
}

unsigned long __rand = 1;
int rand(void)
{
    __rand = __rand * 1103515245L + 12345;
    return (int)(__rand >> 16) & 32767;
}

void srand(unsigned seed)
{
    __rand = seed;
}

long strtol(char *s, char **end, int base)
{
    long v;
    int neg;
    int d;
    while (*s == ' ' || *s == '\t' || *s == '\n')
        s++;
    neg = 0;
    if (*s == '-') {
        neg = 1;
        s++;
    } else if (*s == '+')
        s++;
    if ((base == 0 || base == 16) && s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) {
        s = s + 2;
        base = 16;
    } else if (base == 0 && s[0] == '0')
        base = 8;
    else if (base == 0)
        base = 10;
    v = 0;
    for (;;) {
        if (*s >= '0' && *s <= '9')
            d = *s - '0';
        else if (*s >= 'a' && *s <= 'z')
            d = *s - 'a' + 10;
        else if (*s >= 'A' && *s <= 'Z')
            d = *s - 'A' + 10;
        else
            break;
        if (d >= base)
            break;
        v = v * base + d;
        s++;
    }
    if (end)
        *end = s;
    return neg ? -v : v;
}

unsigned long strtoul(char *s, char **end, int base)
{
    return (unsigned long)strtol(s, end, base);
}

int atoi(char *s)
{
    return (int)strtol(s, NULL, 10);
}

long atol(char *s)
{
    return strtol(s, NULL, 10);
}

float strtod(char *s, char **end)
{
    float v;
    float scale;
    int neg;
    int e;
    int eneg;
    while (*s == ' ' || *s == '\t' || *s == '\n')
        s++;
    neg = 0;
    if (*s == '-') {
        neg = 1;
        s++;
    } else if (*s == '+')
        s++;
    v = 0.0;
    while (*s >= '0' && *s <= '9')
        v = v * 10.0 + (*s++ - '0');
    if (*s == '.') {
        s++;
        scale = 0.1;
        while (*s >= '0' && *s <= '9') {
            v = v + (*s++ - '0') * scale;
            scale = scale / 10.0;
        }
    }
    if (*s == 'e' || *s == 'E') {
        s++;
        eneg = 0;
        if (*s == '-') {
            eneg = 1;
            s++;
        } else if (*s == '+')
            s++;
        e = 0;
        while (*s >= '0' && *s <= '9')
            e = e * 10 + *s++ - '0';
        while (e-- > 0)
            v = eneg ? v / 10.0 : v * 10.0;
    }
    if (end)
        *end = s;
    return neg ? -v : v;
}

float atof(char *s)
{
    return strtod(s, NULL);
}

char *getenv(char *name)
{
    return NULL;
}

void __qswap(char *a, char *b, size_t n)
{
    char t;
    while (n-- > 0) {
        t = *a;
        *a++ = *b;
        *b++ = t;
    }
}

void qsort(void *base, size_t n, size_t size, int (*cmp)(void *, void *))
{
    char *b;
    size_t i;
    size_t last;
    if (n < 2)
        return;
    b = base;
    __qswap(b, b + (n / 2) * size, size);
    last = 0;
    for (i = 1; i < n; i++)
        if (cmp(b + i * size, b) < 0)
            __qswap(b + ++last * size, b + i * size, size);
    __qswap(b, b + last * size, size);
    qsort(b, last, size, cmp);
    qsort(b + (last + 1) * size, n - last - 1, size, cmp);
}

void *bsearch(void *key, void *base, size_t n, size_t size, int (*cmp)(void *, void *))
{
    size_t lo;
    size_t hi;
    size_t mid;
    int c;
    char *b;
    b = base;
    lo = 0;
    hi = n;
    while (lo < hi) {
        mid = (lo + hi) / 2;
        c = cmp(key, b + mid * size);
        if (c == 0)
            return b + mid * size;
        if (c < 0)
            hi = mid;
        else
            lo = mid + 1;
    }
    return NULL;
}

char *__heapmk;
/* MARK / RELEASE around a pass.  The free list is set aside at the mark
   and put back after the release: a pass never takes a block freed before
   it (that block would be lost -- the pass's memory goes back all at
   once), and blocks freed before the mark (a file opened and closed
   between passes) are reused afterwards instead of being lost. */
unsigned *__savedfree;

void __heapsave(void)
{
    __savedfree = __freelist;
    __freelist = NULL;
    __cspv(32, &__heapmk);              /* MARK */
}

void __heaprestore(void)
{
    __cspv(33, &__heapmk);              /* RELEASE */
    __freelist = __savedfree;
}

void exit(int status)
{
    SYSCOM->expansion[5] = status;      /* pexec_status() */
    __stdio_exit();
    __exitprog();
}

void abort(void)
{
    exit(3);
}

