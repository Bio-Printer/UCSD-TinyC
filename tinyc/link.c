/* link.c -- the linker: object modules -> II.0 code file.
 *
 * Input: one or more object files, each holding one or more modules (a
 * library is simply modules one after another).  A module has procedures
 * (code + relocations), exported variables ('D': name, size, initialised
 * or common) and a block of static variables.
 *
 * The files are read sequentially several times (no seeks):
 *   1. names: procedures, variables, modules, segments
 *   2. what is linked: each pass marks what the procedures already linked
 *      use (their relocations), until nothing more is (markpass); no
 *      reference graph is kept in memory
 * Everything reachable from main (and exit) is kept: a module is linked
 * when any of its procedures or variables is used, and then its
 * initialisers run too.  Globals are laid out from word 3 of the startup
 * frame: each linked module's statics, then every used variable.
 * Then pass 1 runs again and keeps only what is linked: the rest of a
 * library takes no memory while the code is written.
 * Segments are numbered (main's is 1, the others 7..15), procedures
 * within them, and block 0 plus the segments are written in one go.
 *
 * Segment 1 procedure 1 is the startup code generated here: zero the
 * globals, run the initialisers, call main, then exit(main's result).
 */
#include "tc.h"
#pragma segment LINK

#define MAXPROC 700
#define MAXDATA 400
#define MAXMOD  64
#define LHASH 128

/* 10 bytes: the P-System links the compiler itself (more than 500 of these) */
struct LProc {
    char *name;             /* a static one's without its MODULE' (LP_STATIC) */
    struct LProc *hnext;
    int codelen;
    unsigned char mod;
    unsigned char seg;      /* index into segnames */
    unsigned char bits;     /* LP_ below */
    unsigned char procnum;
};
#define LP_INIT   1         /* an initialiser */
#define LP_LIVE   2         /* linked */
#define LP_STATIC 4         /* name is modnames[mod]'name */

struct LData {
    char *name;
    int words;
    int strong;
    int mod;                /* defining module, -1 = none yet */
    int offset;
    int live;
    struct LData *hnext;
};

/* the procedures and variables in pass 1 order, in blocks of TBLK
   pointers: a table that doubled (and freed the old one) took 1,500
   words more when the compiler's own procedures passed 512 */
#define TBLK 64
static struct LProc **pblk[MAXPROC / TBLK + 1];
static int nprocs;
static struct LData **dblk[MAXDATA / TBLK + 1];
static int ndatas;
#define PROC(i) pblk[(i) / TBLK][(i) % TBLK]
#define DATA(i) dblk[(i) / TBLK][(i) % TBLK]
static struct LProc **lhash;
static struct LData **dhash;
static int *modstatic;
static int *modbase;
static int *modlive;
#define MAXUSES 64
static int *usemod;             /* module-level references ('U': e.g. __fltused) */
static int *usedata;
static int nuses;
static int nmods;
static char **segnames;         /* [24], allocated per run */
static int nsegs;
static int *segnum;             /* [24] segment index -> II.0 segment number */
static int globalwords;
static char **modnames;         /* [MAXMOD]: a static procedure's name is MODULE'name */
/* the second pass 1 (see link): which procedures and variables to keep, by
   their place in the files (bits), 0 in the first */
static struct PMark p1mark;
static unsigned char *plive;
static unsigned char *dlive;
static int nprecs;
static FILE *lin;
static unsigned char *lbuf;     /* the largest procedure (allocated after pass 1) */
static int lbufsize;
static int maxcode;             /* pass 1: the largest procedure */
static char **objfiles;
static int nobjfiles;
static int curfilei;
static int curmod;
static int mainparmsz;          /* main's parameter words and result words */
static int mainrw;
static int cmparmsz;             /* __callmain's (main(argc, argv): pexec.c) */
static int cmrw;
static struct LProc *callmp;

static int rd(void)
{
    int c;
    c = getc(lin);
    if (c == EOF)
        fatal(103 /* object file truncated */, 0);
    return c;
}

static int rdw(void)
{
    int lo;
    lo = rd();
    return W16(lo + rd() * 256);
}

static void rds(char *s)
{
    int n;
    int i;
    n = rd();
    for (i = 0; i < n; i++)
        s[i] = rd();
    s[n] = 0;
}

/* is p the procedure called name (a static one: MODULE'name) */
static int procis(struct LProc *p, char *name)
{
    char *m;
    int n;
    if (!(p->bits & LP_STATIC))
        return strcmp(p->name, name) == 0;
    m = modnames[p->mod];
    n = strlen(m);
    return strncmp(name, m, n) == 0 && name[n] == '\'' && strcmp(name + n + 1, p->name) == 0;
}

static struct LProc *findproc(char *name)
{
    struct LProc *p;
    for (p = lhash[hashstr(name) & (LHASH - 1)]; p; p = p->hnext)
        if (procis(p, name))
            return p;
    return 0;
}

static int procindex(struct LProc *p)
{
    int i;
    for (i = 0; PROC(i) != p; i++)
        ;
    return i;
}

static struct LData *finddata(char *name)
{
    struct LData *d;
    for (d = dhash[hashstr(name) & (LHASH - 1)]; d; d = d->hnext)
        if (strcmp(d->name, name) == 0)
            return d;
    return 0;
}

static int dataindex(struct LData *d)
{
    int i;
    for (i = 0; DATA(i) != d; i++)
        ;
    return i;
}

static int segindex(char *name)
{
    int i;
    for (i = 0; i < nsegs; i++)
        if (strcmp(segnames[i], name) == 0)
            return i;
    if (nsegs >= 24)
        fatal(104 /* too many segments */, name);
    segnames[nsegs] = pstrdup(name);
    return nsegs++;
}

/* start reading the object files from the beginning */
static void rewindobjs(void)
{
    curfilei = -1;
    curmod = -1;
    lin = 0;
}

/* the next record over all files and modules: 0 at the very end */
static int nextrec(void)
{
    char magic[5];
    int c;
    int i;
    for (;;) {
        if (lin) {
            c = getc(lin);
            if (c == 'T') {             /* the next module in the same file */
                for (i = 1; i < 4; i++)
                    rd();
                continue;
            }
            if (c != EOF && c != 0)
                return c;
            fclose(lin);                /* end (binary files end in NULs) */
            lin = 0;
        }
        curfilei++;
        if (curfilei >= nobjfiles)
            return 0;
        lin = fopen(objfiles[curfilei], "rb");
        if (!lin)
            fatal(25 /* cannot open */, objfiles[curfilei]);
        for (i = 0; i < 4; i++)
            magic[i] = rd();
        magic[4] = 0;
        if (strcmp(magic, "TCOB") != 0)
            fatal(105 /* not a Tiny-C object file */, objfiles[curfilei]);
    }
}

/* read the rest of a record; a procedure's code goes to lbuf */
static void record(int c, char *name, char *seg, int *flags, int *parmsz, int *rw, int *codelen, int *jtab)
{
    int i;
    int n;
    if (c == 'M') {
        rds(name);
        curmod++;
        return;
    }
    if (c == 'G') {
        *codelen = rdw();
        return;
    }
    if (c == 'E')
        return;
    if (c == 'D') {
        *flags = rd();
        rds(name);
        *codelen = rdw();
        return;
    }
    if (c == 'U') {
        rds(name);
        return;
    }
    if (c != 'P')
        fatal(106 /* bad object file record */, 0);
    *flags = rd();
    rds(name);
    rds(seg);
    *parmsz = rdw();
    *rw = rd();
    n = rdw();
    *codelen = n;
    *jtab = rdw();
    if (!lbuf) {                        /* passes before the code is written: only the sizes */
        if (!lbuf && n > maxcode)
            maxcode = n;
        for (i = 0; i < n; i++)
            rd();
        return;
    }
    for (i = 0; i < n; i++)
        lbuf[i] = rd();
}

static void pass1(void)
{
    char name[MAXNAME];
    char seg[MAXNAME];
    char rname[MAXNAME];
    int flags;
    int parmsz;
    int rw;
    int codelen;
    int jtab;
    int c;
    int n;
    int i;
    int h;
    struct LProc *p;
    struct LData *d;
    rewindobjs();
    while ((c = nextrec()) != 0) {
        record(c, name, seg, &flags, &parmsz, &rw, &codelen, &jtab);
        if (c == 'M') {
            if (curmod >= MAXMOD)
                fatal(108 /* too many functions */, 0);
            modstatic[curmod] = 0;
            if (!plive)
                modlive[curmod] = 0;
            modnames[curmod] = pstrdup(name);
            nmods = curmod + 1;
            continue;
        }
        if (c == 'G') {
            modstatic[curmod] = codelen;
            continue;
        }
        if (c == 'D') {
            d = finddata(name);
            if (!d) {
                if (ndatas >= MAXDATA)
                    fatal(108 /* too many functions */, 0);
                if (ndatas % TBLK == 0)
                    dblk[ndatas / TBLK] = (struct LData **)palloc(TBLK * sizeof(struct LData *));
                d = (struct LData *)palloc(sizeof(struct LData));
                d->name = pstrdup(name);
                d->mod = -1;
                h = hashstr(name) & (LHASH - 1);
                d->hnext = dhash[h];
                dhash[h] = d;
                DATA(ndatas) = d;
                ndatas++;
            }
            if (flags && d->strong)
                error(115 /* variable defined twice */, name);
            if (flags || d->mod < 0) {
                if (flags)
                    d->strong = 1;
                d->mod = curmod;
            }
            if (codelen > d->words)
                d->words = codelen;
            continue;
        }
        if (c != 'P')
            continue;
        n = rdw();
        for (i = 0; i < n; i++) {
            rdw();
            rd();
            rds(rname);
        }
        i = nprecs++;
        if (plive && !(plive[i >> 3] & (1 << (i & 7))))
            continue;                   /* (the second pass 1: not linked) */
        if (findproc(name))
            error(107 /* function defined twice */, name);
        if (nprocs >= MAXPROC)
            fatal(108 /* too many functions */, 0);
        if (nprocs % TBLK == 0)
            pblk[nprocs / TBLK] = (struct LProc **)palloc(TBLK * sizeof(struct LProc *));
        p = (struct LProc *)palloc(sizeof(struct LProc));
        p->mod = curmod;
        p->seg = segindex(seg);
        p->bits = (flags & LP_INIT) | (plive ? LP_LIVE : 0);
        p->codelen = codelen;
        i = strlen(modnames[curmod]);
        if (strncmp(name, modnames[curmod], i) == 0 && name[i] == '\'') {
            p->bits = p->bits | LP_STATIC;
            p->name = pstrdup(name + i + 1);
        } else
            p->name = pstrdup(name);
        if (strcmp(name, "main") == 0) {
            mainparmsz = parmsz;
            mainrw = rw;
        }
        if (strcmp(name, "__callmain") == 0) {
            cmparmsz = parmsz;
            cmrw = rw;
        }
        h = hashstr(name) & (LHASH - 1);
        p->hnext = lhash[h];
        lhash[h] = p;
        PROC(nprocs) = p;
        nprocs++;
    }
}

/* One pass over the files: what a linked procedure uses (its relocations)
   is linked too, and so are the initialisers of a linked module and the
   variables it uses ('U').  The first pass also finds what is undefined
   (of every procedure) and collects the 'U' records.  Repeated until
   nothing more is linked: no reference lists are kept (they took more
   memory than anything else when the compiler links itself) -- a few more
   reads of the files instead.  1 if something more was linked */
static int markpass(int first)
{
    char name[MAXNAME];
    char seg[MAXNAME];
    char rname[MAXNAME];
    int flags;
    int parmsz;
    int rw;
    int codelen;
    int jtab;
    int c;
    int n;
    int i;
    int k;
    int type;
    int live;
    int changed;
    struct LProc *p;
    struct LProc *t;
    struct LData *d;
    rewindobjs();
    k = 0;
    changed = 0;
    if (first)
        nuses = 0;
    while ((c = nextrec()) != 0) {
        record(c, name, seg, &flags, &parmsz, &rw, &codelen, &jtab);
        if (c == 'U') {
            if (!first)
                continue;
            d = finddata(name);
            if (!d || d->mod < 0)
                error(114 /* undefined variable */, name);
            else if (nuses < MAXUSES) {
                usemod[nuses] = curmod;
                usedata[nuses] = dataindex(d);
                nuses++;
            }
            continue;
        }
        if (c != 'P')
            continue;
        p = PROC(k);
        k++;
        if (!(p->bits & LP_LIVE) && (p->bits & LP_INIT) && modlive[p->mod]) {
            p->bits = p->bits | LP_LIVE;
            changed = 1;
        }
        live = p->bits & LP_LIVE;
        if (live && !modlive[p->mod]) {
            modlive[p->mod] = 1;
            changed = 1;
        }
        n = rdw();
        for (i = 0; i < n; i++) {
            rdw();
            type = rd();
            rds(rname);
            if (!live && !first)
                continue;
            if (type == 1 || type == 2 || type == 5) {
                t = findproc(rname);
                if (!t) {
                    if (first)
                        error(109 /* undefined function */, rname);
                } else if (live && !(t->bits & LP_LIVE)) {
                    t->bits = t->bits | LP_LIVE;
                    changed = 1;
                }
            } else if (type == 4) {
                d = finddata(rname);
                if (!d || d->mod < 0) {
                    if (first)
                        error(114 /* undefined variable */, rname);
                } else if (live && !d->live) {
                    d->live = 1;
                    modlive[d->mod] = 1;
                    changed = 1;
                }
            }
        }
    }
    for (i = 0; i < nuses; i++) {
        d = DATA(usedata[i]);
        if (modlive[usemod[i]] && !d->live) {
            d->live = 1;
            modlive[d->mod] = 1;
            changed = 1;
        }
    }
    return changed;
}

static void put16(unsigned char *b, int pos, int v)
{
    b[pos] = v & 255;
    b[pos + 1] = (v >> 8) & 255;
}

/* the startup procedure (segment 1, procedure 1, lex level 0) */
static int entrylen;
static unsigned char *entry;
static int entryjtab;

static void eb(int b)
{
    if (entrylen >= 600)
        fatal(108 /* too many functions */, 0);
    entry[entrylen++] = b;
}

static void ecall(struct LProc *p)
{
    if (segnum[p->seg] == 1) {
        eb(0xCF);               /* CGP */
        eb(p->procnum);
        eb(0xD7);
    } else {
        eb(0xCD);               /* CXP */
        eb(segnum[p->seg]);
        eb(p->procnum);
    }
}

static void makeentry(struct LProc *mainp, struct LProc *exitp)
{
    int i;
    int g;
    int exitpos;
    entrylen = 0;
    g = (globalwords - 3) * 2;
    if (g > 0) {                        /* FILLCHAR(globals, 0, bytes, 0) */
        eb(0xA5);
        eb(3);
        eb(0);
        eb(0xC7);
        eb(g & 255);
        eb((g >> 8) & 255);
        eb(0);
        eb(0x9E);
        eb(10);
    }
    for (i = 0; i < nprocs; i++)
        if ((PROC(i)->bits & (LP_LIVE | LP_INIT)) == (LP_LIVE | LP_INIT))
            ecall(PROC(i));
    if (callmp) {                       /* main(argc, argv): __callmain calls it */
        for (i = 0; i < cmparmsz / 2; i++)
            eb(0);
        ecall(callmp);
    } else {
        for (i = 0; i < mainparmsz / 2; i++)
            eb(0);
        ecall(mainp);
    }
    if (exitp) {
        if ((callmp ? cmrw : mainrw) == 0)
            eb(0);
        ecall(exitp);
    }
    exitpos = entrylen;
    eb(0xC1);                           /* RBP 0 */
    eb(0);
    if (entrylen & 1)
        eb(0);
    entryjtab = entrylen + 8;
    g = g < 0 ? 0 : g;
    eb(g & 255);
    eb((g >> 8) & 255);                 /* DATASZ */
    eb(4);
    eb(0);                              /* PARMSZ: INPUT, OUTPUT */
    put16(entry, entrylen, entryjtab - 4 - exitpos);
    entrylen += 2;
    put16(entry, entrylen, entryjtab - 2);
    entrylen += 2;
    eb(1);                              /* procedure 1 */
    eb(0);                              /* lex level 0 */
}

static void patchproc(struct LProc *p, int pos, int type, char *rname)
{
    struct LProc *t;
    struct LData *d;
    int v;
    if (type == 3 || type == 4) {
        v = ((lbuf[pos] & 127) << 8) + lbuf[pos + 1];
        if (type == 3)
            v = v + modbase[p->mod];
        else {
            d = finddata(rname);
            if (!d)
                return;
            v = v + d->offset;
        }
        lbuf[pos] = 128 | ((v >> 8) & 127);
        lbuf[pos + 1] = v & 255;
        return;
    }
    t = findproc(rname);
    if (!t)
        return;
    if (type == 1) {
        if (t->seg == p->seg) {
            lbuf[pos] = 0xCF;
            lbuf[pos + 1] = t->procnum;
            lbuf[pos + 2] = 0xD7;
        } else {
            lbuf[pos] = 0xCD;
            lbuf[pos + 1] = segnum[t->seg];
            lbuf[pos + 2] = t->procnum;
        }
    } else if (type == 2) {
        lbuf[pos + 1] = segnum[t->seg];
        lbuf[pos + 2] = t->procnum;
    } else if (type == 5) {
        /* CGP p: the compiler took the callee to be in this segment (its
           definition, or a prototype under #pragma segment) */
        if (t->seg != p->seg)
            error(116 /* function is not in this segment */, rname);
        lbuf[pos + 1] = t->procnum;
    }
}

int link(char **objs, int nobjs, char *code, char *progname)
{
    struct LProc *mainp;
    struct LProc *exitp;
    struct LProc *p;
    FILE *out;
    unsigned seglen[24];        /* a segment may pass 32767 bytes: unsigned */
    int segblk[24];
    int order[24];
    int pnum[24];
    int norder;
    int i;
    int j;
    int k;
    int s;
    int n;
    int blk;
    int pos;
    unsigned len;
    unsigned char *blk0;
    char name[MAXNAME];
    char seg[MAXNAME];
    char rname[MAXNAME];
    int flags;
    int parmsz;
    int rw;
    int codelen;
    int jtab;
    int c;
    int rpos;
    int rtype;
    int r;
    int *jt;
    unsigned char *keep;
    objfiles = objs;
    nobjfiles = nobjs;
    lbuf = 0;
    maxcode = 0;
    lhash = (struct LProc **)calloc(LHASH, sizeof(struct LProc *));
    dhash = (struct LData **)calloc(LHASH, sizeof(struct LData *));
    modstatic = (int *)malloc(MAXMOD * sizeof(int));
    modbase = (int *)malloc(MAXMOD * sizeof(int));
    modlive = (int *)malloc(MAXMOD * sizeof(int));
    modnames = (char **)malloc(MAXMOD * sizeof(char *));
    usemod = (int *)malloc(MAXUSES * sizeof(int));
    usedata = (int *)malloc(MAXUSES * sizeof(int));
    if (!lhash || !dhash || !modstatic || !modbase || !modlive || !modnames || !usemod || !usedata)
        fatal(2 /* out of memory */, 0);
    nprocs = 0;
    ndatas = 0;
    nmods = 0;
    segnames = (char **)malloc(24 * sizeof(char *));
    segnum = (int *)malloc(24 * sizeof(int));
    if (!segnames || !segnum)
        fatal(2 /* out of memory */, 0);
    nsegs = 0;
    segindex("");
    plive = (unsigned char *)calloc((MAXPROC + 7) / 8 + (MAXDATA + 7) / 8, 1);
    if (!plive)
        fatal(2 /* out of memory */, 0);
    dlive = plive + (MAXPROC + 7) / 8;
    keep = plive;
    plive = 0;
    pmark(&p1mark);                     /* (pass 1's tables: made again with only what is linked) */
    nprecs = 0;
    pass1();
    /* the reference lists (pass 2) are needed only to find what to link:
       given back before the code is written, and so is the buffer pass 2
       keeps one procedure's references in (ints).  The code buffer, the
       largest procedure, comes after: it, and the 600 bytes of the entry
       code, took about 1,000 words of the memory in which the linker opens
       the object files, its least free memory. */
    if (nerrors)
        return 0;
    mainp = findproc("main");
    if (!mainp) {
        error(110 /* no main function */, 0);
        return 0;
    }
    mainp->bits = mainp->bits | LP_LIVE;
    callmp = 0;
    if (mainparmsz >= 4) {              /* main has parameters: argc, argv */
        callmp = findproc("__callmain");
        if (callmp)
            callmp->bits = callmp->bits | LP_LIVE;
    }
    exitp = findproc("exit");
    if (exitp)
        exitp->bits = exitp->bits | LP_LIVE;
    i = 1;
    while (markpass(i) && !nerrors)     /* (the first pass reports what is undefined) */
        i = 0;
    if (nerrors)
        return 0;
    /* what is linked, by place in the files; then pass 1 again with that
       only: the tables of the procedures not linked (most of a library)
       are given back before the code is written */
    for (i = 0; i < nprocs; i++)
        if (PROC(i)->bits & LP_LIVE)
            keep[i >> 3] = keep[i >> 3] | (1 << (i & 7));
    for (i = 0; i < ndatas; i++)
        if (DATA(i)->live)
            dlive[i >> 3] = dlive[i >> 3] | (1 << (i & 7));
    prelease(&p1mark);
    memset(lhash, 0, LHASH * sizeof(struct LProc *));
    memset(dhash, 0, LHASH * sizeof(struct LData *));
    nprocs = 0;
    ndatas = 0;
    nsegs = 1;                          /* ("" stays: before the mark) */
    nprecs = 0;
    lbuf = 0;
    maxcode = 0;
    plive = keep;
    pass1();
    for (i = 0; i < ndatas; i++)
        DATA(i)->live = (dlive[i >> 3] >> (i & 7)) & 1;
    mainp = findproc("main");
    callmp = mainparmsz >= 4 ? findproc("__callmain") : 0;
    exitp = findproc("exit");
    /* globals: the linked modules' statics, then the used variables */
    globalwords = 3;
    for (i = 0; i < nmods; i++) {
        modbase[i] = globalwords;
        if (modlive[i])
            globalwords = globalwords + modstatic[i];
    }
    for (i = 0; i < ndatas; i++)
        if (DATA(i)->live) {
            DATA(i)->offset = globalwords;
            globalwords = globalwords + DATA(i)->words;
        }
    if (globalwords > 16000 || globalwords < 0)
        fatal(34 /* too many global variables */, 0);
    /* segment numbers: main's segment is 1 */
    for (i = 0; i < 24; i++) {
        segnum[i] = 0;
        seglen[i] = 0;
        pnum[i] = 0;
    }
    norder = 0;
    order[norder++] = mainp->seg;
    segnum[mainp->seg] = 1;
    for (i = 0; i < nprocs; i++) {
        p = PROC(i);
        if (!(p->bits & LP_LIVE) || segnum[p->seg])
            continue;
        if (norder >= 10)
            fatal(111 /* more than 10 segments */, segnames[p->seg]);
        segnum[p->seg] = norder + 6;
        order[norder++] = p->seg;
    }
    /* procedure numbers and segment lengths */
    pnum[mainp->seg] = 1;
    for (i = 0; i < nprocs; i++) {
        p = PROC(i);
        if (!(p->bits & LP_LIVE))
            continue;
        s = p->seg;
        p->procnum = ++pnum[s];
        if (pnum[s] > 255)
            fatal(112 /* more than 255 functions in segment */, segnames[s]);
        seglen[s] = seglen[s] + ((p->codelen + 1) & ~1);
    }
    entry = (unsigned char *)malloc(600);
    if (!entry)
        fatal(2 /* out of memory */, 0);
    makeentry(mainp, exitp);
    seglen[mainp->seg] = seglen[mainp->seg] + entrylen;
    blk = 1;
    for (k = 0; k < norder; k++) {
        s = order[k];
        seglen[s] = seglen[s] + 2 * pnum[s] + 2;
        segblk[s] = blk;
        blk = blk + (seglen[s] + 511) / 512;
    }
    out = fopen(code, "wb");
    if (!out)
        fatal(24 /* cannot create */, code);
    lbufsize = maxcode;
    lbufsize = lbufsize + 16;
    lbuf = (unsigned char *)malloc(lbufsize);
    if (!lbuf)
        fatal(2 /* out of memory */, 0);
    blk0 = (unsigned char *)malloc(512);
    memset(blk0, 0, 512);
    for (k = 0; k < norder; k++) {
        s = order[k];
        n = segnum[s];
        put16(blk0, 4 * n, segblk[s]);
        put16(blk0, 4 * n + 2, seglen[s]);
        for (j = 0; j < 8; j++)
            blk0[64 + 8 * n + j] = ' ';
        if (n == 1) {
            for (j = 0; j < 8 && progname[j]; j++)
                blk0[64 + 8 * n + j] = progname[j] >= 'a' && progname[j] <= 'z' ? progname[j] - 32 : progname[j];
        } else {
            for (j = 0; j < 8 && segnames[s][j]; j++)
                blk0[64 + 8 * n + j] = segnames[s][j];
        }
    }
    fwrite(blk0, 1, 512, out);
    jt = (int *)malloc(256 * sizeof(int));
    for (k = 0; k < norder; k++) {
        s = order[k];
        len = 0;
        if (segnum[s] == 1) {
            fwrite(entry, 1, entrylen, out);
            jt[1] = entryjtab;
            len = entrylen;
        }
        rewindobjs();
        i = 0;
        r = 0;
        while ((c = nextrec()) != 0) {
            record(c, name, seg, &flags, &parmsz, &rw, &codelen, &jtab);
            if (c != 'P')
                continue;
            p = 0;
            if (plive[r >> 3] & (1 << (r & 7))) {
                p = PROC(i);            /* (the table holds only what is linked) */
                i++;
            }
            r++;
            n = rdw();
            for (j = 0; j < n; j++) {
                rpos = rdw();
                rtype = rd();
                rds(rname);
                if (p && p->seg == s)
                    patchproc(p, rpos, rtype, rname);
            }
            if (!p || p->seg != s)
                continue;
            lbuf[jtab] = p->procnum;
#ifndef __TINYC__
            if (getenv("TINYC_MAP"))
                fprintf(stderr, "MAP %d %d %d %d %s%s%s\n", segnum[s], p->procnum, len, len + jtab,
                        p->bits & LP_STATIC ? modnames[p->mod] : "", p->bits & LP_STATIC ? "'" : "", p->name);
#endif
            if (codelen & 1)
                lbuf[codelen++] = 0;
            fwrite(lbuf, 1, codelen, out);
            jt[p->procnum] = len + jtab;
            len = len + codelen;
        }
        /* procedure dictionary: self-relative pointers, highest number first */
        for (j = pnum[s]; j >= 1; j--) {
            pos = len;
            putc((pos - jt[j]) & 255, out);
            putc(((pos - jt[j]) >> 8) & 255, out);
            len = len + 2;
        }
        putc(segnum[s], out);
        putc(pnum[s], out);
        len = len + 2;
        if (len != seglen[s])
            fatal(113 /* internal: segment length */, segnames[s]);
        while (len & 511) {
            putc(0, out);
            len++;
        }
    }
    fclose(out);
    for (k = 0; k < norder; k++) {
        s = order[k];
        say("  segment ");
        saynw(segnum[s], 2);
        say(" ");
        sayw(segnum[s] == 1 ? progname : segnames[s], 8);
        saynw(seglen[s], 6);
        say(" bytes ");
        saynw(pnum[s], 3);
        say(" procedures\n");
    }
    say("  globals ");
    sayn(globalwords - 3);
    say(" words\n");
    return nerrors == 0;
}

/* /J OUT=A,B,...: one library file from object files, one after another.
   A file on the P-System is padded with NULs to a whole block, and an
   object file ends in 'E', so trailing NULs are dropped (the linker reads
   a NUL as the end of a file). */
int join(char **objs, int nobjs, char *out)
{
    FILE *o;
    FILE *f;
    int i;
    int c;
    int zeros;
    o = fopen(out, "wb");
    if (!o)
        fatal(24 /* cannot create */, out);
    for (i = 0; i < nobjs; i++) {
        f = fopen(objs[i], "rb");
        if (!f)
            fatal(25 /* cannot open */, objs[i]);
        zeros = 0;
        while ((c = getc(f)) != EOF) {
            if (c == 0) {
                zeros++;
                continue;
            }
            for (; zeros > 0; zeros--)
                putc(0, o);
            putc(c, o);
        }
        fclose(f);
    }
    fclose(o);
    say("  joined ");
    sayn(nobjs);
    say(" object files\n");
    return 1;
}
