/* sedre.c -- SED's regular expressions: POSIX basic (BRE) and extended (ERE) syntax with the GNU
   additions, matched by backtracking.

     c  .  [abc] [^abc] [a-z] [[:alpha:]]   one character          ^  $   the line's start, end
     x*  x\+ x\? x\{m,n\}   (ERE: x+ x? x{m,n})   repeat x         \(...\)  (ERE: (...))   a group
     \|  (ERE: |)  either     \1..\9  what a group matched         \w \W \s \S  word / blank characters
     \b \B \< \>  word edges  \n  newline   \t  tab               \.  \*  \[  \\  and so on: the character
   Backtracking is leftmost, with the first alternative that works (POSIX takes the longest). */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "sed.h"

#define NRE 32
#define MAXN 190                        /* nodes in one expression */
#define MAXSET 8

/* node: op, x, y, z, w */
#define O_MATCH 0
#define O_CHAR 1                        /* x: the character */
#define O_ANY 2
#define O_SET 3                         /* x: the set */
#define O_BOL 4
#define O_EOL 5
#define O_SAVE 6                        /* x: the slot (2 n, 2 n + 1) */
#define O_SPLIT 7                       /* x, y: offsets of the two ways, the first one tried first */
#define O_JMP 8                         /* x: offset */
#define O_REF 9                         /* x: the group */
#define O_RPT 10                        /* x: 1 char 2 any 3 set, y: its character or set, z: min, w: max (-1 none) */
#define O_WORDB 11                      /* \b */
#define O_NWORDB 12                     /* \B */
#define O_WBEG 13                       /* \< */
#define O_WEND 14                       /* \> */

char *re_err;
int re_ms, re_me;
int re_cs[10], re_ce[10];

int *rcode[NRE];                        /* the programs: 5 ints a node */
unsigned char *rsets[NRE];              /* the sets: 32 bytes each */
int rngroups[NRE];
int ricase[NRE];
int nre;

/* the compiler's work area */
int cn[MAXN * 5];
int pc;
unsigned char cset[MAXSET * 32];
int nset;
char *pp;                               /* the pattern, where the compiler is */
int pere, picase;
int png;                                /* groups so far */
int perr;

int emit(int op, int x, int y)
{
    int *n;
    if (pc >= MAXN - 1) {
        re_err = "regular expression too big";
        perr = 1;
        return pc;
    }
    n = cn + pc * 5;
    n[0] = op;
    n[1] = x;
    n[2] = y;
    n[3] = 0;
    n[4] = 0;
    return pc++;
}

/* a node at position at, the others move up one */
void insert(int at, int op, int x, int y)
{
    if (pc >= MAXN - 1) {
        re_err = "regular expression too big";
        perr = 1;
        return;
    }
    memmove(cn + (at + 1) * 5, cn + at * 5, (pc - at) * 5 * sizeof(int));
    pc++;
    cn[at * 5] = op;
    cn[at * 5 + 1] = x;
    cn[at * 5 + 2] = y;
    cn[at * 5 + 3] = 0;
    cn[at * 5 + 4] = 0;
}

void setbit(unsigned char *s, int c)
{
    s[(c & 255) >> 3] |= 1 << (c & 7);
}

int getbit(unsigned char *s, int c)
{
    return (s[(c & 255) >> 3] >> (c & 7)) & 1;
}

int newset(void)
{
    if (nset >= MAXSET) {
        re_err = "too many character classes";
        perr = 1;
        return 0;
    }
    memset(cset + nset * 32, 0, 32);
    return nset++;
}

int classof(char *name, int len, int c)
{
    if (len == 5 && strncmp(name, "alpha", 5) == 0) return isalpha(c);
    if (len == 5 && strncmp(name, "digit", 5) == 0) return isdigit(c);
    if (len == 5 && strncmp(name, "alnum", 5) == 0) return isalnum(c);
    if (len == 5 && strncmp(name, "upper", 5) == 0) return isupper(c);
    if (len == 5 && strncmp(name, "lower", 5) == 0) return islower(c);
    if (len == 5 && strncmp(name, "space", 5) == 0) return isspace(c);
    if (len == 5 && strncmp(name, "punct", 5) == 0) return ispunct(c);
    if (len == 5 && strncmp(name, "print", 5) == 0) return isprint(c);
    if (len == 5 && strncmp(name, "graph", 5) == 0) return isgraph(c);
    if (len == 5 && strncmp(name, "cntrl", 5) == 0) return iscntrl(c);
    if (len == 6 && strncmp(name, "xdigit", 6) == 0) return isxdigit(c);
    if (len == 5 && strncmp(name, "blank", 5) == 0) return c == ' ' || c == '\t';
    return 0;
}

/* a bracket expression: pp is after the [ */
void bracket(void)
{
    int s, neg, c, lo, hi, first, i, k;
    char *e;
    s = newset();
    neg = 0;
    if (*pp == '^') {
        neg = 1;
        pp++;
    }
    first = 1;
    while (*pp && (*pp != ']' || first)) {
        first = 0;
        if (pp[0] == '[' && pp[1] == ':') {
            e = strstr(pp + 2, ":]");
            if (e) {
                for (c = 0; c < 256; c++)
                    if (classof(pp + 2, e - pp - 2, c))
                        setbit(cset + s * 32, c);
                pp = e + 2;
                continue;
            }
        }
        lo = *pp++ & 255;
        if (lo == '\\' && (*pp == 'n' || *pp == 't' || *pp == '\\' || *pp == ']')) {
            lo = *pp == 'n' ? '\n' : (*pp == 't' ? '\t' : *pp);
            pp++;
        }
        hi = lo;
        if (*pp == '-' && pp[1] && pp[1] != ']') {
            hi = pp[1] & 255;
            pp = pp + 2;
        }
        for (c = lo; c <= hi; c++)
            setbit(cset + s * 32, c);
    }
    if (*pp != ']') {
        re_err = "unterminated [";
        perr = 1;
        return;
    }
    pp++;
    if (picase)
        for (c = 0; c < 256; c++)
            if (getbit(cset + s * 32, c)) {
                setbit(cset + s * 32, tolower(c));
                setbit(cset + s * 32, toupper(c));
            }
    if (neg)
        for (i = 0; i < 32; i++)
            cset[s * 32 + i] = ~cset[s * 32 + i];
    k = emit(O_SET, s, 0);
}

void wordset(int kind)                  /* \w \W \s \S */
{
    int s, c, v;
    s = newset();
    for (c = 0; c < 256; c++) {
        v = (kind == 'w' || kind == 'W') ? (isalnum(c) || c == '_') : isspace(c);
        if (kind == 'W' || kind == 'S')
            v = !v;
        if (v)
            setbit(cset + s * 32, c);
    }
    emit(O_SET, s, 0);
}

int alt(int depth);

/* the next character is an operator (for BRE it has a backslash before) */
int opis(int c)
{
    if (pere)
        return *pp == c;
    return pp[0] == '\\' && pp[1] == c;
}

int oplen(void)
{
    return pere ? 1 : 2;
}

/* an interval {m,n}: pp after the open brace; fills *lo, *hi (-1: none); 0 if it is not one */
int interval(int *lo, int *hi)
{
    char *p;
    int m, n;
    p = pp;
    if (!isdigit(*p))
        return 0;
    m = 0;
    while (isdigit(*p))
        m = m * 10 + *p++ - '0';
    n = m;
    if (*p == ',') {
        p++;
        if (isdigit(*p)) {
            n = 0;
            while (isdigit(*p))
                n = n * 10 + *p++ - '0';
        } else
            n = -1;
    }
    if (pere) {
        if (*p != '}')
            return 0;
        p++;
    } else {
        if (p[0] != '\\' || p[1] != '}')
            return 0;
        p = p + 2;
    }
    pp = p;
    *lo = m;
    *hi = n;
    return 1;
}

/* the block [a0, pc) repeated lo to hi times (hi -1: no end) */
void repeat(int a0, int lo, int hi)
{
    int len, i, k;
    int blk[5 * 60];
    len = pc - a0;
    if (len == 1 && (cn[a0 * 5] == O_CHAR || cn[a0 * 5] == O_ANY || cn[a0 * 5] == O_SET)) {
        cn[a0 * 5 + 4] = hi;            /* one character: a counting node */
        cn[a0 * 5 + 3] = lo;
        cn[a0 * 5 + 2] = cn[a0 * 5 + 1];
        cn[a0 * 5 + 1] = cn[a0 * 5] == O_CHAR ? 1 : (cn[a0 * 5] == O_ANY ? 2 : 3);
        cn[a0 * 5] = O_RPT;
        return;
    }
    if (len > 60 || (lo > 0 && lo * len + a0 >= MAXN) || (hi > 0 && hi * len + a0 >= MAXN)) {
        re_err = "regular expression too big";
        perr = 1;
        return;
    }
    memcpy(blk, cn + a0 * 5, len * 5 * sizeof(int));
    if (lo == 0 && hi == 1) {
        insert(a0, O_SPLIT, 1, len + 1);
        return;
    }
    if (lo == 0 && hi < 0) {
        insert(a0, O_SPLIT, 1, len + 2);
        emit(O_JMP, -(len + 1), 0);
        return;
    }
    if (lo == 1 && hi < 0) {
        emit(O_SPLIT, -len, 1);
        return;
    }
    pc = a0;                            /* the general case: copies */
    for (i = 0; i < lo; i++) {
        memcpy(cn + pc * 5, blk, len * 5 * sizeof(int));
        pc = pc + len;
    }
    if (hi < 0) {
        k = pc;
        memcpy(cn + pc * 5, blk, len * 5 * sizeof(int));
        pc = pc + len;
        insert(k, O_SPLIT, 1, len + 2);
        emit(O_JMP, -(len + 1), 0);
    } else
        for (i = lo; i < hi; i++) {
            k = pc;
            memcpy(cn + pc * 5, blk, len * 5 * sizeof(int));
            pc = pc + len;
            insert(k, O_SPLIT, 1, len + 1);
        }
}

/* one atom and its repeats */
int piece(int first)
{
    int a0, c, lo, hi, g;
    a0 = pc;
    c = *pp & 255;
    if (c == 0)
        return 0;
    if (pere && (c == '|' || c == ')'))
        return 0;
    if (!pere && c == '\\' && (pp[1] == '|' || pp[1] == ')'))
        return 0;
    if (pere && c == '(' || !pere && c == '\\' && pp[1] == '(') {
        pp = pp + oplen();
        g = ++png;
        if (g > 9) {
            re_err = "too many groups";
            perr = 1;
            return 0;
        }
        emit(O_SAVE, 2 * g, 0);
        alt(1);
        if (!opis(')')) {
            re_err = "unmatched (";
            perr = 1;
            return 0;
        }
        pp = pp + oplen();
        emit(O_SAVE, 2 * g + 1, 0);
    } else if (c == '.') {
        pp++;
        emit(O_ANY, 0, 0);
    } else if (c == '[') {
        pp++;
        bracket();
    } else if (c == '^' && (pere || first)) {
        pp++;
        emit(O_BOL, 0, 0);
    } else if (c == '$' && (pere || !pp[1] || (pp[1] == '\\' && (pp[2] == ')' || pp[2] == '|')))) {
        pp++;
        emit(O_EOL, 0, 0);
    } else if (c == '*' && first) {
        pp++;
        emit(O_CHAR, '*', 0);
    } else if (c == '\\' && pp[1]) {
        c = pp[1] & 255;
        pp = pp + 2;
        if (c >= '1' && c <= '9')
            emit(O_REF, c - '0', 0);
        else if (c == 'n')
            emit(O_CHAR, '\n', 0);
        else if (c == 't')
            emit(O_CHAR, '\t', 0);
        else if (c == 'w' || c == 'W' || c == 's' || c == 'S')
            wordset(c);
        else if (c == 'b')
            emit(O_WORDB, 0, 0);
        else if (c == 'B')
            emit(O_NWORDB, 0, 0);
        else if (c == '<')
            emit(O_WBEG, 0, 0);
        else if (c == '>')
            emit(O_WEND, 0, 0);
        else
            emit(O_CHAR, picase ? tolower(c) : c, 0);
    } else {
        pp++;
        emit(O_CHAR, picase ? tolower(c) : c, 0);
    }
    for (;;) {
        if (perr)
            return 0;
        if (*pp == '*') {
            pp++;
            repeat(a0, 0, -1);
        } else if (pere ? *pp == '+' : (pp[0] == '\\' && pp[1] == '+')) {
            pp = pp + oplen();
            repeat(a0, 1, -1);
        } else if (pere ? *pp == '?' : (pp[0] == '\\' && pp[1] == '?')) {
            pp = pp + oplen();
            repeat(a0, 0, 1);
        } else if (pere ? *pp == '{' : (pp[0] == '\\' && pp[1] == '{')) {
            pp = pp + oplen();
            if (!interval(&lo, &hi)) {
                if (pere) {
                    pp--;
                    break;
                }
                re_err = "bad \\{ \\}";
                perr = 1;
                return 0;
            }
            repeat(a0, lo, hi);
        } else
            break;
    }
    return 1;
}

void seq(void)
{
    int first;
    first = 1;
    while (!perr && piece(first))
        first = 0;
}

int alt(int depth)
{
    int start, jmps[12], nj, i;
    start = pc;
    nj = 0;
    seq();
    while (!perr && (pere ? *pp == '|' : (pp[0] == '\\' && pp[1] == '|'))) {
        pp = pp + oplen();
        insert(start, O_SPLIT, 1, pc - start + 2);
        if (nj < 12)
            jmps[nj++] = emit(O_JMP, 0, 0);
        start = pc;
        seq();
    }
    for (i = 0; i < nj; i++)
        cn[jmps[i] * 5 + 1] = pc - jmps[i];
    return 1;
}

int re_compile(char *pat, int ere, int icase)
{
    int r;
    if (nre >= NRE) {
        re_err = "too many regular expressions";
        return -1;
    }
    pc = 0;
    nset = 0;
    pp = pat;
    pere = ere;
    picase = icase;
    png = 0;
    perr = 0;
    re_err = 0;
    alt(0);
    if (!perr && *pp) {
        re_err = pere ? "unmatched )" : "unmatched \\)";
        perr = 1;
    }
    if (perr)
        return -1;
    emit(O_MATCH, 0, 0);
    if (perr)
        return -1;
    r = nre++;
    rcode[r] = malloc(pc * 5 * sizeof(int));
    rsets[r] = malloc(nset * 32 + 1);
    if (!rcode[r] || !rsets[r]) {
        re_err = "out of memory";
        nre--;
        return -1;
    }
    memcpy(rcode[r], cn, pc * 5 * sizeof(int));
    memcpy(rsets[r], cset, nset * 32);
    rngroups[r] = png;
    ricase[r] = icase;
    return r;
}

int re_groups(int re)
{
    return rngroups[re];
}

/* ---- matching ---- */

int *mc;                                /* the program being run */
unsigned char *msets;
char *mbase;                            /* the text */
int micase;
int mcap[20];
char *mend;
int mdepth;

int isword(int c)
{
    return isalnum(c) || c == '_';
}

int fold(int c)
{
    return micase ? tolower(c) : c;
}

/* len characters at a and b equal (case ignored with the flag) */
int same(char *a, char *b, int len)
{
    int k;
    for (k = 0; k < len; k++) {
        if (!b[k] || fold(a[k] & 255) != fold(b[k] & 255))
            return 0;
    }
    return 1;
}

int atom(int kind, int arg, int c)
{
    if (kind == 1)
        return fold(c) == arg;
    if (kind == 2)
        return c != 0;
    return getbit(msets + arg * 32, c);
}

int run(int pc, char *s)
{
    int *n;
    int i, k, old, len, mx;
    if (++mdepth > 400) {
        mdepth--;
        return 0;
    }
    for (;;) {
        n = mc + pc * 5;
        switch (n[0]) {
        case O_MATCH:
            mend = s;
            mdepth--;
            return 1;
        case O_CHAR:
            if (*s && fold(*s & 255) == n[1]) {
                s++;
                pc++;
                continue;
            }
            mdepth--;
            return 0;
        case O_ANY:
            if (*s) {
                s++;
                pc++;
                continue;
            }
            mdepth--;
            return 0;
        case O_SET:
            if (*s && getbit(msets + n[1] * 32, *s & 255)) {
                s++;
                pc++;
                continue;
            }
            mdepth--;
            return 0;
        case O_BOL:
            if (s == mbase) {
                pc++;
                continue;
            }
            mdepth--;
            return 0;
        case O_EOL:
            if (*s == 0) {
                pc++;
                continue;
            }
            mdepth--;
            return 0;
        case O_WORDB:
        case O_NWORDB:
        case O_WBEG:
        case O_WEND:
            i = s > mbase && isword(s[-1] & 255);
            k = *s && isword(*s & 255);
            if ((n[0] == O_WORDB && i != k) || (n[0] == O_NWORDB && i == k) || (n[0] == O_WBEG && !i && k)
                || (n[0] == O_WEND && i && !k)) {
                pc++;
                continue;
            }
            mdepth--;
            return 0;
        case O_SAVE:
            old = mcap[n[1]];
            mcap[n[1]] = s - mbase;
            if (run(pc + 1, s)) {
                mdepth--;
                return 1;
            }
            mcap[n[1]] = old;
            mdepth--;
            return 0;
        case O_SPLIT:
            if (run(pc + n[1], s)) {
                mdepth--;
                return 1;
            }
            pc = pc + n[2];
            continue;
        case O_JMP:
            pc = pc + n[1];
            continue;
        case O_REF:
            i = mcap[2 * n[1]];
            k = mcap[2 * n[1] + 1];
            if (i < 0 || k < 0) {
                mdepth--;
                return 0;
            }
            len = k - i;
            if (!same(mbase + i, s, len)) {
                mdepth--;
                return 0;
            }
            s = s + len;
            pc++;
            continue;
        case O_RPT:
            mx = n[4];
            for (i = 0; (mx < 0 || i < mx) && s[i] && atom(n[1], n[2], s[i] & 255); i++)
                ;
            for (; i >= n[3]; i--)
                if (run(pc + 1, s + i)) {
                    mdepth--;
                    return 1;
                }
            mdepth--;
            return 0;
        }
        mdepth--;
        return 0;
    }
}

int re_exec(int re, char *s, int from)
{
    int i, st, len;
    char *p;
    mc = rcode[re];
    msets = rsets[re];
    micase = ricase[re];
    mbase = s;
    len = strlen(s);
    for (st = from; st <= len; st++) {
        if (mc[0] == O_CHAR && !micase) {       /* the first character is known: skip to it */
            p = strchr(s + st, mc[1]);
            if (!p)
                return 0;
            st = p - s;
        }
        for (i = 0; i < 20; i++)
            mcap[i] = -1;
        mdepth = 0;
        if (run(0, s + st)) {
            re_ms = st;
            re_me = mend - s;
            for (i = 1; i < 10; i++) {
                re_cs[i] = mcap[2 * i];
                re_ce[i] = mcap[2 * i + 1];
            }
            return 1;
        }
        if (mc[0] == O_BOL)
            return 0;
    }
    return 0;
}
