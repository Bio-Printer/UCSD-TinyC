/* diff.c -- DIFF [-iwbBqs] [-u|-U N|-c|-C N] FILE1 FILE2: what differs between two text files, line by line.

   Output  (none)  normal:  3c3  or  5a6,7  or  8,9d7  and the lines, < from FILE1, > from FILE2
           -u, -U N  unified: --- FILE1  +++ FILE2  @@ -a,b +c,d @@ and the lines with a space, - or +,
                     N lines of context (3 without N)
           -c, -C N  context: *** FILE1  --- FILE2  and the changed lines marked  ! - +  with N lines of context
           -q  only say whether they differ      -s  say when they are the same
   Comparing  -i  ignore case   -w  ignore all blanks   -b  ignore changes in the amount of blanks
              -B  ignore the lines that are empty (added or removed)
   Exit status 0 same, 1 different, 2 trouble.  "-" is the standard input (for one of the files).
   Files that are not text (.TEXT, .C, .H and .BAS are) are compared byte by byte: Binary files ... differ.

   The files are read once, a window of lines at a time: files of any size, little memory.  When the two
   differ in a place it looks ahead 40 lines in each for the next lines they have in common; a change
   larger than that is shown as that much deleted and added, one piece after another.  The result is a
   correct diff (patch makes FILE2 out of FILE1) but not always the shortest one.
   Run it from the shell: DIFF A.C B.C, DIFF -u OLD.C NEW.C >FIX.TEXT. */
#pragma segment MAIN	/* calls within the program: 2-byte CGPs, no NOP after them */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define R 128                           /* lines kept for each file */
#define W 40                            /* how far ahead to look for lines in common */
#define MAXL 500
#define NH 24                           /* hunks in a group */

int optq, opts, opti, optw, optb, optB, fmt, ctx;       /* fmt: 0 normal, 1 unified, 2 context */

/* one file: pages and lines */
struct fl {
    FILE *f;
    char *name;
    int istext;
    int fblocks;
    unsigned char pg[1024];
    long pgno;
    int pglen;
    long pos[R];                        /* the start of line n at n % R */
    unsigned h1[R], h2[R];
    long nread;                         /* the lines whose start is known */
    long npos;                          /* where line nread starts */
    int eof;
};
struct fl fa, fb;
char lbuf[MAXL + 4];
int blen;
char lbuf2[MAXL + 4];

int pgread(struct fl *x, long n)
{
    int nb;
    unsigned char *z;
    if (n == x->pgno)
        return 1;
    nb = __cxp0i(28, x->f->fib, x->pg, 0, 2, (int)((x->istext ? 2 : 0) + 2 * n), 1, 0, 0);
    if (nb <= 0 || (long)(x->istext ? 2 : 0) + 2 * n >= x->fblocks) {
        x->pgno = -1;
        return 0;
    }
    if (nb > 2)
        nb = 2;
    x->pgno = n;
    x->pglen = nb * 512;
    if (x->istext) {
        z = memchr(x->pg, 0, x->pglen);
        if (z)
            x->pglen = z - x->pg;
    }
    return 1;
}

/* the line at p into buf (blen bytes; *np its next); 0 at the end */
int rdline(struct fl *x, long p, char *buf, long *np)
{
    long n;
    int off, i, c, k;
    for (;;) {
        n = p / 1024;
        off = (int)(p % 1024);
        if (!pgread(x, n))
            return 0;
        if (off < x->pglen)
            break;
        p = (n + 1) * 1024;
    }
    blen = 0;
    i = off;
    while (i < x->pglen) {
        c = x->pg[i++];
        if (x->istext) {
            if (c == 13)
                break;
            if (c == 16 && i < x->pglen) {
                k = x->pg[i++] - 32;
                while (k-- > 0 && blen < MAXL)
                    buf[blen++] = ' ';
                continue;
            }
        } else if (c == 10)
            break;
        if (blen < MAXL)
            buf[blen++] = c;
    }
    buf[blen] = 0;
    *np = n * 1024 + i;
    return 1;
}

/* the two hashes of buf as the options see it */
void hashline(char *buf, int n, unsigned *a, unsigned *b)
{
    unsigned x, y;
    int i, c, blank, last;
    x = 17;
    y = 5;
    blank = 0;
    last = n;
    if (optb || optw)
        while (last > 0 && (buf[last - 1] == ' ' || buf[last - 1] == '\t'))
            last--;
    for (i = 0; i < last; i++) {
        c = buf[i] & 255;
        if (c == ' ' || c == '\t') {
            if (optw)
                continue;
            if (optb) {
                if (blank)
                    continue;
                blank = 1;
                c = ' ';
            }
        } else
            blank = 0;
        if (opti)
            c = tolower(c);
        x = x * 31 + c;
        y = y * 131 + (c ^ 85);
    }
    *a = x;
    *b = y;
}

/* is line n of x there (reads as far as it must); 0 if the file ends first */
int have(struct fl *x, long n)
{
    long np;
    unsigned a, b;
    while (x->nread <= n && !x->eof) {
        if (!rdline(x, x->npos, lbuf, &np)) {
            x->eof = 1;
            break;
        }
        hashline(lbuf, blen, &a, &b);
        x->pos[x->nread % R] = x->npos;
        x->h1[x->nread % R] = a;
        x->h2[x->nread % R] = b;
        x->nread++;
        x->npos = np;
    }
    return n < x->nread;
}

int eq(long i, long j)
{
    return fa.h1[i % R] == fb.h1[j % R] && fa.h2[i % R] == fb.h2[j % R];
}

/* line n of x, as text, into buf (blen); the text if the line is not in the window any more is "" */
void text(struct fl *x, long n, char *buf)
{
    long np;
    buf[0] = 0;
    blen = 0;
    if (n < x->nread && n >= x->nread - R)
        rdline(x, x->pos[n % R], buf, &np);
}

int emptyline(struct fl *x, long n)
{
    text(x, n, lbuf2);
    return blen == 0;
}

/* ---- the output ---- */

struct hk {
    long a1, a2, b1, b2;                /* lines a1..a2-1 of FILE1 become b1..b2-1 of FILE2 */
};
int lead0, trail0;
struct hk hunks[NH];
int nh;
int ndiff;                              /* hunks found, any */
char *namea, *nameb;

void line_(char *pre, struct fl *x, long n)
{
    text(x, n, lbuf);
    fputs(pre, stdout);
    fputs(lbuf, stdout);
    fputs("\n", stdout);
}

void range(long a, long b)              /* lines a+1..b, in the form of diff */
{
    if (b - a == 1)
        printf("%ld", b);
    else if (b == a)
        printf("%ld", a);
    else
        printf("%ld,%ld", a + 1, b);
}

void normal_hunk(struct hk *h)
{
    long k;
    int del, add;
    del = h->a2 > h->a1;
    add = h->b2 > h->b1;
    if (!del) {
        printf("%ld", h->a1);
        printf("a");
        range(h->b1, h->b2);
    } else if (!add) {
        range(h->a1, h->a2);
        printf("d%ld", h->b1);
    } else {
        range(h->a1, h->a2);
        printf("c");
        range(h->b1, h->b2);
    }
    printf("\n");
    for (k = h->a1; k < h->a2; k++)
        line_("< ", &fa, k);
    if (del && add)
        printf("---\n");
    for (k = h->b1; k < h->b2; k++)
        line_("> ", &fb, k);
}

/* a group of hunks (the lines between them are the same): unified or context format */
void group_out(void)
{
    long a1, a2, b1, b2, k, p;
    int i, c1, c2;
    char *mark;
    if (nh == 0)
        return;
    c1 = lead0 ? 0 : ctx;               /* a hunk that goes on in the next group: no context there */
    c2 = trail0 ? 0 : ctx;
    lead0 = trail0 = 0;
    a1 = hunks[0].a1 - c1;
    b1 = hunks[0].b1 - c1;
    if (a1 < 0) {
        b1 = b1 - a1;
        a1 = 0;
    }
    if (b1 < 0) {
        a1 = a1 - b1;
        b1 = 0;
    }
    a2 = hunks[nh - 1].a2 + c2;
    b2 = hunks[nh - 1].b2 + c2;
    while (a2 > fa.nread && !have(&fa, a2 - 1) && a2 > hunks[nh - 1].a2) {
        a2--;
        b2--;
    }
    if (fmt == 1) {
        printf("@@ -%ld,%ld +%ld,%ld @@\n", a2 > a1 ? a1 + 1 : a1, a2 - a1, b2 > b1 ? b1 + 1 : b1, b2 - b1);
        p = a1;
        for (i = 0; i < nh; i++) {
            for (; p < hunks[i].a1; p++)
                line_(" ", &fa, p);
            for (k = hunks[i].a1; k < hunks[i].a2; k++)
                line_("-", &fa, k);
            for (k = hunks[i].b1; k < hunks[i].b2; k++)
                line_("+", &fb, k);
            p = hunks[i].a2;
        }
        for (; p < a2; p++)
            line_(" ", &fa, p);
    } else {
        printf("***************\n*** ");
        if (a2 - a1 == 1)
            printf("%ld", a1 + 1);
        else
            printf("%ld,%ld", a2 > a1 ? a1 + 1 : a1, a2);
        printf(" ****\n");
        for (i = 0; i < nh; i++)
            if (hunks[i].a2 > hunks[i].a1)
                break;
        if (i < nh) {                   /* FILE1's lines, if any were taken out or changed */
            p = a1;
            for (i = 0; i < nh; i++) {
                for (; p < hunks[i].a1; p++)
                    line_("  ", &fa, p);
                mark = hunks[i].b2 > hunks[i].b1 ? "! " : "- ";
                for (k = hunks[i].a1; k < hunks[i].a2; k++)
                    line_(mark, &fa, k);
                p = hunks[i].a2;
            }
            for (; p < a2; p++)
                line_("  ", &fa, p);
        }
        printf("--- ");
        if (b2 - b1 == 1)
            printf("%ld", b1 + 1);
        else
            printf("%ld,%ld", b2 > b1 ? b1 + 1 : b1, b2);
        printf(" ----\n");
        for (i = 0; i < nh; i++)
            if (hunks[i].b2 > hunks[i].b1)
                break;
        if (i < nh) {
            p = b1;
            for (i = 0; i < nh; i++) {
                for (; p < hunks[i].b1; p++)
                    line_("  ", &fb, p);
                mark = hunks[i].a2 > hunks[i].a1 ? "! " : "+ ";
                for (k = hunks[i].b1; k < hunks[i].b2; k++)
                    line_(mark, &fb, k);
                p = hunks[i].b2;
            }
            for (; p < b2; p++)
                line_("  ", &fb, p);
        }
    }
    nh = 0;
}

int headed;

void header(void)
{
    if (headed || fmt == 0)
        return;
    headed = 1;
    if (fmt == 1) {
        printf("--- %s\n+++ %s\n", namea, nameb);
    } else
        printf("*** %s\n--- %s\n", namea, nameb);
}

/* a hunk found: out now (normal) or into the group (the others) */
void hunk(long a1, long a2, long b1, long b2)
{
    int i;
    if (optB) {                         /* only empty lines added or removed: not a difference */
        for (i = 0; i < 1; i++) {
            long k;
            int allempty;
            allempty = 1;
            for (k = a1; k < a2; k++)
                if (!emptyline(&fa, k))
                    allempty = 0;
            for (k = b1; k < b2; k++)
                if (!emptyline(&fb, k))
                    allempty = 0;
            if (allempty)
                return;
        }
    }
    ndiff++;
    if (optq)
        return;
    if (fmt == 0) {
        struct hk h;
        h.a1 = a1;
        h.a2 = a2;
        h.b1 = b1;
        h.b2 = b2;
        normal_hunk(&h);
        return;
    }
    header();
    if (nh > 0 && (a1 - hunks[nh - 1].a2 > 2 * ctx || nh >= NH || a2 - hunks[0].a1 > 50 || b2 - hunks[0].b1 > 50)) {
        i = a1 == hunks[nh - 1].a2 && b1 == hunks[nh - 1].b2;      /* one change cut in two */
        trail0 = i;
        group_out();
        lead0 = i;
    }
    if (nh > 0 && a1 == hunks[nh - 1].a2 && b1 == hunks[nh - 1].b2) {
        hunks[nh - 1].a2 = a2;          /* next to it: one hunk */
        hunks[nh - 1].b2 = b2;
        return;
    }
    hunks[nh].a1 = a1;
    hunks[nh].a2 = a2;
    hunks[nh].b1 = b1;
    hunks[nh].b2 = b2;
    nh++;
}

/* ---- the comparison ---- */

/* the next lines in common after i, j: *di, *dj the lines to skip in each; 0 if there are none in the window */
int resync(long i, long j, int *pdi, int *pdj)
{
    int s, di, dj, run1, bdi, bdj;
    int hava, havb;
    run1 = 0;
    for (s = 1; s <= 2 * W; s++)
        for (di = 0; di <= s && di <= W; di++) {
            dj = s - di;
            if (dj > W)
                continue;
            if (!have(&fa, i + di) || !have(&fb, j + dj))
                continue;
            if (!eq(i + di, j + dj))
                continue;
            hava = have(&fa, i + di + 1);
            havb = have(&fb, j + dj + 1);
            if ((hava && havb && eq(i + di + 1, j + dj + 1)) || (!hava && !havb)) {
                *pdi = di;
                *pdj = dj;
                return 1;
            }
            if (!run1 && s <= 2) {
                *pdi = di;
                *pdj = dj;
                return 1;
            }
            if (!run1) {
                run1 = 1;
                bdi = di;
                bdj = dj;
            }
        }
    if (run1) {
        *pdi = bdi;
        *pdj = bdj;
        return 1;
    }
    return 0;
}

void compare(void)
{
    long i, j, a1, b1;
    int di, dj, open, na, nb;
    i = 0;
    j = 0;
    open = 0;
    a1 = b1 = 0;
    for (;;) {
        if (have(&fa, i) && have(&fb, j) && eq(i, j)) {
            if (open) {
                hunk(a1, i, b1, j);
                open = 0;
            }
            i++;
            j++;
            continue;
        }
        if (!have(&fa, i) && !have(&fb, j))
            break;
        if (!open) {
            open = 1;
            a1 = i;
            b1 = j;
        }
        if (optq && ndiff > 0)
            return;
        if (!have(&fa, i)) {            /* FILE1 is over: the rest of FILE2 is added */
            for (dj = 0; dj < W && have(&fb, j + dj); dj++)
                ;
            j = j + dj;
            if (dj == W) {              /* a piece at a time */
                hunk(a1, i, b1, j);
                open = 0;
            }
            continue;
        }
        if (!have(&fb, j)) {
            for (di = 0; di < W && have(&fa, i + di); di++)
                ;
            i = i + di;
            if (di == W) {
                hunk(a1, i, b1, j);
                open = 0;
            }
            continue;
        }
        if (resync(i, j, &di, &dj)) {
            i = i + di;
            j = j + dj;
        } else {                        /* nothing in common in the window: all of it changes */
            for (na = 0; na < W && have(&fa, i + na); na++)
                ;
            for (nb = 0; nb < W && have(&fb, j + nb); nb++)
                ;
            i = i + na;
            j = j + nb;
            hunk(a1, i, b1, j);
            open = 0;
        }
        if (optq && ndiff > 0)
            return;
    }
    if (open)
        hunk(a1, i, b1, j);
    if (fmt != 0)
        group_out();
}

int openfile(struct fl *x, char *name)
{
    if (strcmp(name, "-") == 0)
        x->f = stdin;
    else
        x->f = fopen(name, "r");
    if (!x->f) {
        fprintf(stderr, "diff: %s: cannot open it\n", name);
        return 0;
    }
    x->name = name;
    x->istext = (x->f->flags & __F_TEXT) != 0;
    x->fblocks = ((int *)x->f->fib)[17] - ((int *)x->f->fib)[16];
    x->pgno = -1;
    x->nread = 0;
    x->npos = 0;
    x->eof = 0;
    return 1;
}

/* two files that are not text: byte for byte */
int samebytes(void)
{
    char b1[512], b2[512];
    int n1, n2;
    for (;;) {
        n1 = fread(b1, 1, 512, fa.f);
        n2 = fread(b2, 1, 512, fb.f);
        if (n1 != n2 || memcmp(b1, b2, n1) != 0)
            return 0;
        if (n1 == 0)
            return 1;
    }
}

int main(int argc, char **argv)
{
    int i, k, nf;
    char *a;
    char *names[2];
    nf = 0;
    ctx = 3;
    for (i = 1; i < argc; i++) {
        a = argv[i];
        if (a[0] == '-' && a[1] != 0) {
            for (k = 1; a[k]; k++)
                switch (a[k]) {
                case 'i': opti = 1; break;
                case 'w': optw = 1; break;
                case 'b': optb = 1; break;
                case 'B': optB = 1; break;
                case 'q': optq = 1; break;
                case 's': opts = 1; break;
                case 'a': case 'u': case 'c': case 'U': case 'C':
                    if (a[k] == 'a')
                        break;
                    fmt = (a[k] == 'u' || a[k] == 'U') ? 1 : 2;
                    if (a[k] == 'U' || a[k] == 'C') {
                        if (a[k + 1])
                            ctx = atoi(a + k + 1);
                        else if (i + 1 < argc)
                            ctx = atoi(argv[++i]);
                        k = strlen(a) - 1;
                    }
                    break;
                default:
                    fprintf(stderr, "diff: invalid option -- '%c'\n", a[k]);
                    fprintf(stderr, "use: DIFF [-iwbBqs] [-u|-U N|-c|-C N] FILE1 FILE2\n");
                    return 2;
                }
        } else if (nf < 2)
            names[nf++] = a;
        else {
            fprintf(stderr, "diff: extra operand %s\n", a);
            return 2;
        }
    }
    if (nf != 2) {
        fprintf(stderr, "use: DIFF [-iwbBqs] [-u|-U N|-c|-C N] FILE1 FILE2\n");
        return 2;
    }
    if (strcmp(names[0], "-") == 0 && strcmp(names[1], "-") == 0) {
        fprintf(stderr, "diff: only one of the files can be the standard input\n");
        return 2;
    }
    namea = names[0];
    nameb = names[1];
    if (!openfile(&fa, names[0]) || !openfile(&fb, names[1]))
        return 2;
    if (!fa.istext || !fb.istext) {
        if (samebytes()) {
            if (opts)
                printf("Files %s and %s are identical\n", namea, nameb);
            return 0;
        }
        printf("Binary files %s and %s differ\n", namea, nameb);
        return 1;
    }
    compare();
    if (ndiff == 0) {
        if (opts)
            printf("Files %s and %s are identical\n", namea, nameb);
        return 0;
    }
    if (optq)
        printf("Files %s and %s differ\n", namea, nameb);
    return 1;
}
