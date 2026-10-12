/* patch.c -- PATCH [-RsbClN] [-F N] [-pN] [-o OUTFILE] [-i PATCHFILE] [FILE]: apply a diff to a file.

   The patch (made by DIFF or GNU diff; unified, context or normal form) comes from -i PATCHFILE or the
   standard input (PATCH A.C <FIX.TEXT, DIFF -u A.C B.C | PATCH A.C).  FILE is the file to change; without
   it the name comes from the patch's headers (the new name, else the old one).  A patch for several files
   changes each in turn.  Each hunk is looked for where its line numbers say, then up to 60 lines before and
   after (an offset), then again with the first and last lines of its context left out (fuzz, up to 2).
   A hunk that cannot be placed is not applied: it goes to a reject file (NAME with .REJ before the
   extension: A.TEXT: A.REJ.TEXT), the others are, and the exit status is 1 (0 all applied, 2 trouble).
   -R  reverse: undo the patch      -s  say nothing but trouble       -b  keep the original (A.ORIG.TEXT)
   -C  only check: change nothing   -N  skip a hunk that is already (reverse-) applied: not done
   -l  ignore changes in blanks     -F N  the most fuzz (2)           -pN  drop N leading names (a/b/ ...)
   -o OUTFILE  write the result there; the file is left as it was       -i PATCHFILE  the patch
   Files are read a window at a time, any size.  Unlike diff the lines of the original are compared
   exactly (blanks aside with -l). */
#pragma segment MAIN	/* calls within the program: 2-byte CGPs, no NOP after them */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define R 192                           /* lines kept for each file */
#define MAXL 400
#define NHL 190
#define MAXOFF 60

int optR, opts, optb, optC, optl, fuzzmax, strip;
char *outname;

struct fl {
    FILE *f;
    int istext;
    int fblocks;
    unsigned char pg[1024];
    long pgno;
    int pglen;
    long pos[R];
    unsigned h1[R], h2[R];
    long nread;
    long npos;
    int eof;
};
struct fl po, pp;                       /* the original, the patch */
char lbuf[MAXL + 4];
int blen;

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

void hashline(char *buf, int n, unsigned *a, unsigned *b)
{
    unsigned x, y;
    int i, c, blank, last;
    x = 17;
    y = 5;
    blank = 0;
    last = n;
    if (optl)
        while (last > 0 && (buf[last - 1] == ' ' || buf[last - 1] == '\t'))
            last--;
    for (i = 0; i < last; i++) {
        c = buf[i] & 255;
        if (optl && (c == ' ' || c == '\t')) {
            if (blank)
                continue;
            blank = 1;
            c = ' ';
        } else
            blank = 0;
        x = x * 31 + c;
        y = y * 131 + (c ^ 85);
    }
    *a = x;
    *b = y;
}

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
    return n < x->nread && n >= x->nread - R;
}

void text(struct fl *x, long n, char *buf)
{
    long np;
    buf[0] = 0;
    blen = 0;
    if (n < x->nread && n >= x->nread - R)
        rdline(x, x->pos[n % R], buf, &np);
}

int openfl(struct fl *x, char *name)
{
    x->f = strcmp(name, "-") == 0 ? stdin : fopen(name, "r");
    if (!x->f)
        return 0;
    x->istext = (x->f->flags & __F_TEXT) != 0;
    x->fblocks = ((int *)x->f->fib)[17] - ((int *)x->f->fib)[16];
    x->pgno = -1;
    x->nread = 0;
    x->npos = 0;
    x->eof = 0;
    return 1;
}

/* ---- the hunk ---- */

char htype[NHL];                        /* ' ' context, '-' only in the old, '+' only in the new */
long hpl[NHL];                          /* the patch line holding its text */
int hplen;                              /* the characters before the text in a patch line (the marker) */
int nhl;
long hstart;                            /* the old side's first line (1 based) */
long hrawfrom, hrawto;                  /* the hunk's own lines in the patch, for the reject file */
unsigned oh1[NHL], oh2[NHL];            /* hashes of the old side's lines, in order */
int oidx[NHL];                          /* ... and where they are in htype */
int nold;
int hunkno, failed, total;
long pline;                             /* the next patch line to look at */
int pfeof;
char pbuf[MAXL + 4];

/* the next patch line into pbuf; 0 at the end */
int nextp(void)
{
    if (!have(&pp, pline))
        return 0;
    text(&pp, pline, pbuf);
    pline++;
    return 1;
}

int atoi_(char **p)
{
    int n;
    n = 0;
    while (isdigit(**p))
        n = n * 10 + *(*p)++ - '0';
    return n;
}

/* "a,b" or "a" at *p: start and count as unified writes them; moves past */
void unirange(char **p, long *start, long *cnt)
{
    *start = atoi_(p);
    *cnt = 1;
    if (**p == ',') {
        (*p)++;
        *cnt = atoi_(p);
    }
}

void addhl(int t, long n)
{
    if (nhl < NHL) {
        htype[nhl] = t;
        hpl[nhl] = n;
    }
    nhl++;
}

void oldhash(void)
{
    int i;
    unsigned a, b;
    nold = 0;
    for (i = 0; i < nhl; i++)
        if (htype[i] != '+') {
            text(&pp, hpl[i], lbuf);
            hashline(lbuf + hplen, blen - hplen > 0 ? blen - hplen : 0, &a, &b);
            oh1[nold] = a;
            oh2[nold] = b;
            oidx[nold++] = i;
        }
}

/* a unified hunk: pbuf is its @@ line */
int unihunk(void)
{
    char *p;
    long os, oc, ns, nc;
    int c;
    p = pbuf + 4;
    unirange(&p, &os, &oc);
    p++;
    p++;
    unirange(&p, &ns, &nc);
    hstart = os;
    hplen = 1;
    nhl = 0;
    hrawfrom = pline - 1;
    while (oc > 0 || nc > 0) {
        if (!have(&pp, pline))
            break;
        text(&pp, pline, pbuf);
        c = pbuf[0];
        if (c == '\\') {
            pline++;
            continue;
        }
        if (c == '-' && oc > 0)
            oc--, addhl('-', pline);
        else if (c == '+' && nc > 0)
            nc--, addhl('+', pline);
        else if ((c == ' ' || c == 0) && oc > 0 && nc > 0) {
            oc--;
            nc--;
            addhl(' ', pline);
        } else
            break;
        pline++;
    }
    hrawto = pline;
    return nhl;
}

/* a context hunk: pbuf is the *** a,b **** line (after the row of stars) */
int ctxhunk(void)
{
    long ol[NHL], nl[NHL];
    char ot[NHL], nt[NHL];
    char *p;
    long os, oc, ns, nc;
    int no, nn, c, i, j, t;
    p = pbuf + 4;
    unirange(&p, &os, &oc);
    if (oc > 0 && *p == ',')
        ;
    hstart = os;
    hplen = 2;
    no = nn = 0;
    hrawfrom = pline - 2;
    while (have(&pp, pline)) {           /* the old side, up to the --- line */
        text(&pp, pline, pbuf);
        if (strncmp(pbuf, "--- ", 4) == 0 && strchr(pbuf, '-') && pbuf[blen - 1] == '-')
            break;
        if (strncmp(pbuf, "***************", 15) == 0 || strncmp(pbuf, "*** ", 4) == 0)
            return -1;
        if (no < NHL) {
            ot[no] = pbuf[0] == '-' ? '-' : (pbuf[0] == '!' ? '!' : ' ');
            ol[no] = pline;
            no++;
        }
        pline++;
    }
    pline++;                            /* the --- c,d ---- line */
    while (have(&pp, pline)) {           /* the new side */
        text(&pp, pline, pbuf);
        if (strncmp(pbuf, "***************", 15) == 0 || (strncmp(pbuf, "*** ", 4) == 0 && pbuf[4] != 0 && isdigit(pbuf[4]) == 0))
            break;
        if (strncmp(pbuf, "*** ", 4) == 0 || strncmp(pbuf, "--- ", 4) == 0 && pline > 0 && 0)
            break;
        if (nn < NHL) {
            nt[nn] = pbuf[0] == '+' ? '+' : (pbuf[0] == '!' ? '!' : ' ');
            nl[nn] = pline;
            nn++;
        }
        pline++;
    }
    nhl = 0;
    i = j = 0;
    if (nn == 0) {                      /* no new side printed: it is the old one without the deletions */
        for (i = 0; i < no; i++)
            addhl(ot[i] == ' ' ? ' ' : '-', ol[i]);
    } else if (no == 0) {               /* no old side printed: nothing was deleted */
        for (j = 0; j < nn; j++)
            addhl(nt[j] == ' ' ? ' ' : '+', nl[j]);
    } else {
        while (i < no || j < nn) {
            if (i < no && j < nn && ot[i] == ' ' && nt[j] == ' ') {
                addhl(' ', ol[i]);
                i++;
                j++;
            } else {
                while (i < no && ot[i] != ' ') {
                    addhl('-', ol[i]);
                    i++;
                }
                while (j < nn && nt[j] != ' ') {
                    addhl('+', nl[j]);
                    j++;
                }
                if (i < no && ot[i] == ' ' && j >= nn)
                    i++;
                else if (j < nn && nt[j] == ' ' && i >= no)
                    j++;
            }
        }
    }
    hrawto = pline;
    return nhl;
}

/* a normal-form hunk: pbuf is the command line */
int normhunk(void)
{
    char *p;
    long a, b, c, d;
    int cmd;
    p = pbuf;
    a = atoi_(&p);
    b = a;
    if (*p == ',') {
        p++;
        b = atoi_(&p);
    }
    cmd = *p++;
    c = atoi_(&p);
    hplen = 2;
    nhl = 0;
    hrawfrom = pline - 1;
    hstart = cmd == 'a' ? a : a;
    while (have(&pp, pline)) {
        text(&pp, pline, pbuf);
        if (pbuf[0] == '<')
            addhl('-', pline);
        else if (pbuf[0] == '>')
            addhl('+', pline);
        else if (strcmp(pbuf, "---") != 0)
            break;
        pline++;
    }
    hrawto = pline;
    return nhl;
}

int isnormalcmd(char *s)
{
    char *p;
    p = s;
    if (!isdigit(*p))
        return 0;
    while (isdigit(*p) || *p == ',')
        p++;
    if (*p != 'a' && *p != 'c' && *p != 'd')
        return 0;
    p++;
    if (!isdigit(*p))
        return 0;
    while (isdigit(*p) || *p == ',')
        p++;
    return *p == 0;
}

/* ---- applying ---- */

FILE *out;
long copied;                            /* original lines already written */
long offset;                            /* where the last hunk went, against where it said */
char tname[40];

void putline(char *s)
{
    fputs(s, out);
    fputs("\n", out);
}

/* the original's lines up to n (0 based, not including) go out */
void copyto(long n)
{
    while (copied < n) {
        if (!have(&po, copied))
            break;
        text(&po, copied, lbuf);
        putline(lbuf);
        copied++;
    }
}

int matches(long cand, int lead, int trail)
{
    int k;
    for (k = lead; k < nold - trail; k++) {
        if (!have(&po, cand + k - lead))
            return 0;
        if (po.h1[(cand + k - lead) % R] != oh1[k] || po.h2[(cand + k - lead) % R] != oh2[k])
            return 0;
    }
    return 1;
}

FILE *rej;
char rejname[40];
int rejopen;

/* NAME with ins before the extension: A.TEXT, .REJ -> A.REJ.TEXT */
void withsuffix(char *name, char *ins, char *dst)
{
    int k;
    k = strlen(name);
    while (k > 0 && name[k - 1] != '.' && name[k - 1] != ':')
        k--;
    if (k > 0 && name[k - 1] == '.')
        k--;
    else
        k = strlen(name);
    sprintf(dst, "%.*s%s%s", k, name, ins, name + k);
}

void saverej(char *fname)
{
    long n;
    if (!rejopen) {
        withsuffix(fname, ".REJ", rejname);
        rej = fopen(rejname, "w");
        rejopen = 1;
    }
    if (!rej)
        return;
    for (n = hrawfrom; n < hrawto; n++) {
        text(&pp, n, pbuf);
        fputs(pbuf, rej);
        fputs("\n", rej);
    }
}

/* place and apply the hunk in hunk[]; fname for messages */
void apply(char *fname)
{
    int lead, trail, f, d, found, i, lead2, trail2, k;
    long cand, exp;
    int fz;
    hunkno++;
    total++;
    if (optR) {                         /* undo: what was added is deleted and the other way round */
        for (i = 0; i < nhl; i++)
            if (htype[i] != ' ')
                htype[i] = htype[i] == '+' ? '-' : '+';
    }
    oldhash();
    found = 0;
    cand = 0;
    fz = 0;
    if (nold == 0) {                    /* nothing to compare with: where it says (an insertion) */
        cand = hstart;
        found = 1;
    }
    for (f = 0; f <= fuzzmax && !found; f++) {
        lead = 0;
        trail = 0;
        i = 0;
        while (i < nhl && htype[i] == ' ' && lead < f) {
            lead++;
            i++;
        }
        i = nhl - 1;
        while (i >= 0 && htype[i] == ' ' && trail < f) {
            trail++;
            i--;
        }
        exp = hstart - 1 + lead + offset;
        if (nold == 0)
            break;
        for (d = 0; d <= MAXOFF && !found; d++) {
            for (k = 0; k < 2 && !found; k++) {
                cand = exp + (k ? -d : d);
                if (k == 1 && d == 0)
                    continue;
                if (cand - lead < copied)
                    continue;
                if (matches(cand, lead, trail)) {
                    found = 1;
                    fz = f;
                    lead2 = lead;
                    trail2 = trail;
                }
            }
        }
    }
    if (!found) {
        failed++;
        fprintf(stderr, "Hunk #%d FAILED at %ld.\n", hunkno, hstart);
        saverej(fname);
        return;
    }
    if (nold > 0) {
        cand = cand - lead2;            /* the old side's first line */
        copyto(cand);
        k = 0;
        for (i = 0; i < nhl; i++) {
            if (htype[i] == '+') {
                text(&pp, hpl[i], lbuf);
                putline(lbuf + hplen);
            } else {
                if (have(&po, cand + k)) {
                    if (htype[i] == ' ') {
                        text(&po, cand + k, lbuf);
                        putline(lbuf);
                    }
                }
                k++;
            }
        }
        copied = cand + k;
        offset = cand - (hstart - 1);
    } else {                            /* an insertion without context */
        copyto(cand);
        for (i = 0; i < nhl; i++)
            if (htype[i] == '+') {
                text(&pp, hpl[i], lbuf);
                putline(lbuf + hplen);
            }
    }
    if (!opts) {
        if (fz > 0)
            printf("Hunk #%d succeeded at %ld with fuzz %d.\n", hunkno, cand + 1, fz);
        else if (nold > 0 && offset != 0)
            printf("Hunk #%d succeeded at %ld (offset %ld line%s).\n", hunkno, cand + 1, offset, offset == 1 || offset == -1 ? "" : "s");
        else
            printf("Hunk #%d succeeded at %ld.\n", hunkno, cand + 1);
    }
}

/* ---- files ---- */

char target[80];
int haveTarget;
int opened;
char *filearg;

char *strip_(char *name)
{
    int n;
    char *p;
    n = strip;
    while (n-- > 0) {
        p = strchr(name, '/');
        if (!p)
            break;
        name = p + 1;
    }
    return name;
}

/* the name in a header line (after the three characters and a blank), cut at a tab or two blanks */
void headname(char *line, char *dst)
{
    char *p;
    int n;
    p = line + 4;
    n = 0;
    while (*p && *p != '\t' && !(*p == ' ' && p[1] == ' ') && n < 79)
        dst[n++] = *p++;
    dst[n] = 0;
}

void openoutput(char *fname)
{
    int e;
    out = stdout;
    if (optC)
        out = fopen("PATCHTMP.TEXT", "w");
    else if (outname)
        out = fopen(outname, "w");
    else {
        withsuffix("PATCHTMP.TEXT", "", tname);
        out = fopen("PATCHTMP.TEXT", "w");
    }
    if (!out) {
        fprintf(stderr, "patch: cannot open the output file\n");
        exit(2);
    }
}

void startfile(char *fname)
{
    long dummy;
    char b[80];
    if (!opts)
        printf("patching file %s\n", fname);
    strcpy(target, fname);
    if (!openfl(&po, fname)) {
        po.f = 0;
        po.eof = 1;                      /* a file that is not there: made new */
        po.nread = 0;
        po.istext = 1;
        po.fblocks = 0;
    }
    copied = 0;
    offset = 0;
    hunkno = 0;
    rejopen = 0;
    openoutput(fname);
    opened = 1;
}

void finishfile(void)
{
    FILE *a, *b;
    int c;
    char bak[80];
    if (!opened)
        return;
    copyto(1000000000L);
    fclose(out);
    if (!outname && !optC) {
        if (optb && po.f) {
            withsuffix(target, ".ORIG", bak);
            a = fopen(target, "r");
            b = fopen(bak, "w");
            if (a && b)
                while ((c = fgetc(a)) != EOF)
                    fputc(c, b);
            if (a)
                fclose(a);
            if (b)
                fclose(b);
        }
        if (po.f && po.f != stdin)
            fclose(po.f);
        a = fopen("PATCHTMP.TEXT", "r");
        b = fopen(target, "w");
        if (a && b)
            while ((c = fgetc(a)) != EOF)
                fputc(c, b);
        if (a)
            fclose(a);
        if (b)
            fclose(b);
    } else if (po.f && po.f != stdin)
        fclose(po.f);
    if (!outname)
        remove("PATCHTMP.TEXT");
    if (rejopen && rej) {
        fclose(rej);
        printf("%d out of %d hunk%s FAILED -- saving rejects to file %s\n", failed, total, total == 1 ? "" : "s", rejname);
    }
    opened = 0;
}

int main(int argc, char **argv)
{
    int i, k, ok, c;
    char *a, *arg;
    char *pfile;
    char name1[80], name2[80], use[80];
    FILE *t;
    pfile = 0;
    filearg = 0;
    fuzzmax = 2;
    for (i = 1; i < argc; i++) {
        a = argv[i];
        if (a[0] == '-' && a[1] && strcmp(a, "-") != 0) {
            for (k = 1; a[k]; k++) {
                c = a[k];
                if (c == 'R') optR = 1;
                else if (c == 's') opts = 1;
                else if (c == 'b') optb = 1;
                else if (c == 'C') optC = 1;
                else if (c == 'l') optl = 1;
                else if (c == 'N' || c == 'f' || c == 'u' || c == 'c' || c == 'n' || c == 't')
                    ;
                else if (c == 'F' || c == 'p' || c == 'o' || c == 'i') {
                    if (a[k + 1])
                        arg = a + k + 1;
                    else if (i + 1 < argc)
                        arg = argv[++i];
                    else {
                        fprintf(stderr, "patch: option -%c requires an argument\n", c);
                        return 2;
                    }
                    if (c == 'F') fuzzmax = atoi(arg);
                    else if (c == 'p') strip = atoi(arg);
                    else if (c == 'o') outname = arg;
                    else pfile = arg;
                    break;
                } else {
                    fprintf(stderr, "patch: invalid option -- '%c'\n", c);
                    fprintf(stderr, "use: PATCH [-RsbClN] [-F N] [-pN] [-o OUT] [-i PATCHFILE] [FILE]\n");
                    return 2;
                }
            }
        } else if (!filearg)
            filearg = a;
        else {
            fprintf(stderr, "patch: extra operand %s\n", a);
            return 2;
        }
    }
    if (pfile) {
        if (!openfl(&pp, pfile)) {
            fprintf(stderr, "patch: %s: cannot open it\n", pfile);
            return 2;
        }
    } else {
        if (stdin->flags & __F_CON) {
            fprintf(stderr, "use: PATCH [-RsbClN] [-F N] [-pN] [-o OUT] [-i PATCHFILE] [FILE]   (the patch from -i or the standard input)\n");
            return 2;
        }
        pp.f = stdin;
        pp.istext = (stdin->flags & __F_TEXT) != 0;
        pp.fblocks = ((int *)stdin->fib)[17] - ((int *)stdin->fib)[16];
        pp.pgno = -1;
    }
    pline = 0;
    opened = 0;
    if (filearg)
        strcpy(use, filearg);
    ok = 1;
    while (nextp()) {
        if (strncmp(pbuf, "--- ", 4) == 0 && have(&pp, pline)) {            /* unified: --- old, +++ new */
            text(&pp, pline, lbuf);
            if (strncmp(lbuf, "+++ ", 4) == 0) {
                finishfile();
                headname(pbuf, name1);
                headname(lbuf, name2);
                pline++;
                if (filearg)
                    strcpy(use, filearg);
                else {
                    strcpy(use, strip_(name2));
                    t = fopen(use, "r");
                    if (!t) {
                        strcpy(use, strip_(name1));
                        t = fopen(use, "r");
                        if (!t)
                            strcpy(use, strip_(name2));      /* neither is there: the new one is made */
                    }
                    if (t)
                        fclose(t);
                }
                startfile(use);
                continue;
            }
        }
        if (strncmp(pbuf, "*** ", 4) == 0 && pbuf[4] && !isdigit(pbuf[4]) && have(&pp, pline)) {      /* context: *** old, --- new */
            text(&pp, pline, lbuf);
            if (strncmp(lbuf, "--- ", 4) == 0) {
                finishfile();
                headname(pbuf, name1);
                headname(lbuf, name2);
                pline++;
                if (filearg)
                    strcpy(use, filearg);
                else {
                    strcpy(use, strip_(name2));
                    t = fopen(use, "r");
                    if (!t) {
                        strcpy(use, strip_(name1));
                        t = fopen(use, "r");
                        if (!t)
                            strcpy(use, strip_(name2));
                    }
                    if (t)
                        fclose(t);
                }
                startfile(use);
                continue;
            }
        }
        if (strncmp(pbuf, "@@ -", 4) == 0) {
            if (!opened) {
                if (!filearg) {
                    fprintf(stderr, "patch: no file name (give FILE, or a patch with headers)\n");
                    return 2;
                }
                startfile(filearg);
            }
            if (unihunk() > NHL) {
                fprintf(stderr, "patch: a hunk of more than %d lines\n", NHL);
                return 2;
            }
            apply(use);
            continue;
        }
        if (strncmp(pbuf, "***************", 15) == 0 && nextp()) {
            if (!opened) {
                if (!filearg) {
                    fprintf(stderr, "patch: no file name\n");
                    return 2;
                }
                startfile(filearg);
            }
            if (ctxhunk() > NHL) {
                fprintf(stderr, "patch: a hunk of more than %d lines\n", NHL);
                return 2;
            }
            apply(use);
            pline--;                    /* the line that ended it is looked at again */
            continue;
        }
        if (isnormalcmd(pbuf)) {
            if (!opened) {
                if (!filearg) {
                    fprintf(stderr, "patch: no file name (a normal diff has none: give FILE)\n");
                    return 2;
                }
                startfile(filearg);
            }
            if (normhunk() > NHL) {
                fprintf(stderr, "patch: a hunk of more than %d lines\n", NHL);
                return 2;
            }
            apply(use);
            continue;
        }
    }
    finishfile();
    if (total == 0 && !opts)
        fprintf(stderr, "patch: no hunks found\n");
    return failed ? 1 : (total == 0 ? 2 : 0);
}
