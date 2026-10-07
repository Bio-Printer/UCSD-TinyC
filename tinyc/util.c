/* util.c -- errors and memory pools.
 *
 * Three pools, all bump allocators:
 *   perm  -- lives for the whole pass (types, globals, macros)
 *   func  -- reset at the end of every function definition
 *   expr  -- mark/release around every statement and every source line
 */
#include "tc.h"
#pragma segment MAIN

int nerrors;
int z80calls;
int curlocal;                   /* frame of the function being compiled */
int maxlocal;
int nparamwords;
int scratch;
char *cursegname;
FILE *objout;
char *curfile;
int curline;

/* ---- output: the compiler needs no printf (it is the library's, for
   user programs); these few routines are all it uses ---- */
char *itoa10(int n, char *b)
{
    char t[8];
    int i;
    int j;
    unsigned v;
    i = 0;
    j = 0;
    if (n < 0) {
        b[j++] = '-';
        v = -n;
    } else
        v = n;
    do {
        t[i++] = '0' + v % 10;
        v = v / 10;
    } while (v != 0);
    while (i > 0)
        b[j++] = t[--i];
    b[j] = 0;
    return b;
}

void say(char *s)
{
    fputs(s, stdout);
}

void sayn(int n)
{
    char b[8];
    say(itoa10(n, b));
}

/* s padded with blanks to w characters (left-justified) */
void sayw(char *s, int w)
{
    say(s);
    w = w - strlen(s);
    while (w-- > 0)
        putchar(' ');
}

/* n right-justified in w characters */
/* n (unsigned: sizes up to 65535) right-aligned in w columns */
void saynw(unsigned n, int w)
{
    char b[8];
    int i;
    i = 7;
    b[i] = 0;
    do {
        b[--i] = '0' + n % 10;
        n = n / 10;
    } while (n != 0);
    w = w - (7 - i);
    while (w-- > 0)
        putchar(' ');
    say(b + i);
}

/* Message texts live in a file (tcmsgs.txt; on the P-System TCMSGS.TEXT
   on the default volume, the boot volume or TINY-C:): line n is message n.  Keeping them
   out of the code saves memory in every pass. */
static void message(int n)
{
    FILE *fp;
    int c;
    int line;
    char path[200];
    char *dir;
    fp = 0;
#ifdef __TINYC__
    if (n != 2) {                   /* not when out of memory: fopen needs a buffer */
        fp = fopen("TCMSGS.TEXT", "r");
        if (!fp)
            fp = fopen("*TCMSGS.TEXT", "r");
        if (!fp)
            fp = fopen("TINY-C:TCMSGS.TEXT", "r");
    }
#else
    dir = getenv("TINYC_INCLUDE");
    if (dir) {
        strcpy(path, dir);
        strcat(path, "/tcmsgs.txt");
        fp = fopen(path, "r");
    }
#endif
    if (!fp) {
        if (n == 2)
            say("out of memory");
        else {
            say("message #");
            sayn(n);
        }
        return;
    }
    line = 1;
    while (line < n && (c = getc(fp)) != EOF)
        if (c == '\n')
            line++;
    while ((c = getc(fp)) != EOF && c != '\n')
        putchar(c);
    fclose(fp);
}

static void report(char *kind, int n, char *arg)
{
    if (curfile) {
        say(curfile);
        say(":");
        sayn(curline);
        say(": ");
    }
    say(kind);
    message(n);
    if (arg) {
        say(" '");
        say(arg);
        say("'");
    }
    say("\n");
}

void error(int n, char *arg)
{
    report("error: ", n, arg);
    nerrors++;
    if (nerrors >= 20)
        fatal(1 /* too many errors */, 0);
}

void warn(int n, char *arg)
{
    report("warning: ", n, arg);
}

void memfail(int n)
{
#ifdef __TINYC__
    say("(asked for ");
    sayn(n);
    say(" bytes, ");
    sayn(__cspi(40));
    say(" words free)\n");
#endif
    fatal(2 /* out of memory */, 0);
}

void fatal(int n, char *arg)
{
    report("fatal: ", n, arg);
    exit(2);
}

#define PCHUNK 448           /* tools/pchunk.py: the least free memory against PCHUNK */
static char *pcur;
static int pleft;

char *palloc(int n)
{
    char *p;
    n = (n + 1) & ~1;
    if (n > pleft) {
        if (n > PCHUNK / 2) {
            p = (char *)malloc(n);
            if (!p)
                memfail(n);
            memset(p, 0, n);
            return p;
        }
        pcur = (char *)malloc(PCHUNK);
        if (!pcur)
            memfail(PCHUNK);
        pleft = PCHUNK;
    }
    p = pcur;
    pcur += n;
    pleft -= n;
    memset(p, 0, n);
    return p;
}

/* Give back everything allocated since pmark (the heap, and the
   permanent pool's place in its current block), within a pass: the
   linker's reference lists once it knows what to link.  Nothing taken
   since may be used afterwards.  On a host nothing is given back.
   Only the linker uses these: its segment, not the resident one. */
#pragma segment LINK
#ifdef __TINYC__
extern unsigned *__freelist;    /* the library's (malloc, free) */
#endif

void pmark(struct PMark *m)
{
#ifdef __TINYC__
    /* the free list is set aside: a block freed after the mark could be
       in the memory given back (as __heapsave) */
    m->heap[0] = (void *)__freelist;
    __freelist = 0;
    __cspv(32, &m->heap[1]);            /* MARK */
#endif
    m->pcur = pcur;
    m->pleft = pleft;
}

void prelease(struct PMark *m)
{
#ifdef __TINYC__
    __cspv(33, &m->heap[1]);            /* RELEASE */
    __freelist = (unsigned *)m->heap[0];
    pcur = m->pcur;
    pleft = m->pleft;
#endif
}
#pragma segment MAIN

/* the function pool: a chain of chunks reused for every function */
struct Chunk {
    struct Chunk *next;
    int size;
};
static struct Chunk *ffirst;
static struct Chunk *fchunk;
static int fused;

char *falloc(int n)
{
    char *p;
    struct Chunk *c;
    n = (n + 1) & ~1;
    if (!fchunk || fused + n > fchunk->size) {
        c = fchunk ? fchunk->next : ffirst;
        if (!c || c->size < n) {
            int sz;
            sz = n > PCHUNK ? n : PCHUNK;
            c = (struct Chunk *)malloc(sizeof(struct Chunk) + sz);
            if (!c)
                memfail(sz);
            c->size = sz;
            if (fchunk) {
                c->next = fchunk->next;
                fchunk->next = c;
            } else {
                c->next = ffirst;
                ffirst = c;
            }
        }
        fchunk = c;
        fused = 0;
    }
    p = (char *)(fchunk + 1) + fused;
    fused += n;
    memset(p, 0, n);
    return p;
}

void freset(void)
{
    fchunk = 0;
    fused = 0;
}

static char *xbuf;
static int xused;
static int xsize;

/* the size of the expression pool for the next pass (host structures are
   about three times larger than on the P-System) */
void xsetsize(int n)
{
#ifdef __TINYC__
    xsize = n;
#else
    xsize = n * 4;
#endif
}

char *xalloc(int n)
{
    char *p;
    if (!xbuf) {
        if (xsize == 0)
            xsetsize(3000);
        xbuf = (char *)malloc(xsize);
        if (!xbuf)
            memfail(xsize);
    }
    n = (n + 1) & ~1;
    if (xused + n > xsize)
        fatal(3 /* expression too complex */, 0);
    p = xbuf + xused;
    xused += n;
    memset(p, 0, n);
    return p;
}

int xmark(void)
{
    return xused;
}

void xrelease(int m)
{
    xused = m;
}

/* forget all pools (their memory was released, or is abandoned on a host) */
void resetpools(void)
{
    pcur = 0;
    pleft = 0;
    ffirst = 0;
    fchunk = 0;
    fused = 0;
    xbuf = 0;
    xused = 0;
}

char *pstrdup(char *s)
{
    char *p;
    p = palloc(strlen(s) + 1);
    strcpy(p, s);
    return p;
}

int hashstr(char *s)
{
    int h;
    h = 0;
    while (*s)
        h = (h * 5 + *s++) & 2047;
    return h;
}
