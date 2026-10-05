/* gen.c -- P-code generation: the emitter, procedures and the object file
 * (code for expressions: genx.c; the two are one segment, GEN).
 *
 * Each C function becomes one II.0 procedure: code, then its jump table
 * (long jumps), DATASZ, PARMSZ, EXITIC, ENTRIC and procnum/lexlevel at
 * JTAB.  Everything inside a procedure is self-relative, so procedures are
 * written to the object file as finished byte images; the linker only
 * fills in procedure numbers and CXP operands.
 *
 * Global initialisers are compiled into "init" procedures (one emitter
 * of their own) that the program runs once at startup.
 */
#include "tc.h"
#include "gen.h"
#pragma segment GEN

static struct Emit fe;          /* the current function */
static struct Emit ie;          /* global initialisers */
struct Emit *E;                 /* the current emitter (gen.h) */
static int ninit;
static int ininit;
int lvtemp;                     /* compound assignment (gen.h) */
struct Node *lvnode;

static void setupemit(struct Emit *e, int max, int maxlab, int maxfix, int maxrel)
{
    e->max = max;
    e->code = (unsigned char *)malloc(max);
    e->maxlab = maxlab;
    e->labpos = (int *)malloc(maxlab * sizeof(int));
    e->maxfix = maxfix;
    e->fixpos = (int *)malloc(maxfix * sizeof(int));
    e->fixlab = (int *)malloc(maxfix * sizeof(int));
    e->maxrel = maxrel;
    e->relpos = (int *)malloc(maxrel * sizeof(int));
    e->reltype = (int *)malloc(maxrel * sizeof(int));
    e->relname = (char **)malloc(maxrel * sizeof(char *));
    if (!e->code || !e->labpos || !e->fixlab || !e->relname)
        fatal(2 /* out of memory */, 0);
    e->pc = 0;
    e->nlab = 0;
    e->nfix = 0;
    e->nrel = 0;
}

/* the frame variables live in the emitter while it is not current */
static void saveframe(void)
{
    E->curlocal = curlocal;
    E->maxlocal = maxlocal;
    E->nparam = nparamwords;
    E->scratch = scratch;
}

static void loadframe(void)
{
    curlocal = E->curlocal;
    maxlocal = E->maxlocal;
    nparamwords = E->nparam;
    scratch = E->scratch;
}

/* ---- bytes ---- */

void ob(int b)
{
    if (E->pc >= E->max)
        fatal(95 /* function too large */, 0);
    E->code[E->pc++] = b & 255;
}

void big(int v)
{
    if (v >= 0 && v < 128)
        ob(v);
    else {
        ob(128 | ((v >> 8) & 127));
        ob(v & 255);
    }
}

void opbig(int op, int v)
{
    ob(op);
    big(v);
}

void ldc(int v)
{
    v = W16(v);
    if (v >= 0 && v < 128)
        ob(v);
    else if (v < 0 && v > -128) {
        ob(-v);
        ob(O_NGI);
    } else {
        ob(O_LDCI);
        ob(v & 255);
        ob((v >> 8) & 255);
    }
}

void ldl(int off)
{
    if (off >= 1 && off <= 16)
        ob(0xD7 + off);
    else
        opbig(O_LDL, off);
}

void gen_stl(int off)
{
    opbig(O_STL, off);
}

void lla(int off)
{
    opbig(O_LLA, off);
}

static void ldo(int off)
{
    if (off >= 1 && off <= 16)
        ob(0xE7 + off);
    else
        opbig(O_LDO, off);
}

static void sro(int off)
{
    opbig(O_SRO, off);
}

static void lao(int off)
{
    opbig(O_LAO, off);
}

void ind(int k)
{
    if (k == 0)
        ob(O_SIND0);
    else if (k > 0 && k < 8)
        ob(O_SIND0 + k);
    else
        opbig(O_IND, k);
}

void addconst(int bytes)
{
    if (bytes == 0)
        return;
    if (bytes > 0 && (bytes & 1) == 0)
        opbig(O_INC, bytes / 2);
    else {
        ldc(bytes);
        ob(O_ADI);
    }
}

void csp(int n)
{
    ob(O_CSP);
    ob(n);
}

/* relocation targets outlive the statement they came from: keep one copy of each name */
#define NHASH 64
struct Name {
    char *s;
    struct Name *next;
};
static struct Name **names;     /* [NHASH], allocated per run */

static char *intern(char *s)
{
    struct Name *n;
    int h;
    h = hashstr(s) & (NHASH - 1);
    for (n = names[h]; n; n = n->next)
        if (strcmp(n->s, s) == 0)
            return n->s;
    n = (struct Name *)palloc(sizeof(struct Name));
    n->s = pstrdup(s);
    n->next = names[h];
    names[h] = n;
    return n->s;
}

void reloc(int type, char *name)
{
    name = intern(name);
    if (E->nrel >= E->maxrel)
        fatal(96 /* too many calls in one function */, 0);
    E->relpos[E->nrel] = E->pc;
    E->reltype[E->nrel] = type;
    E->relname[E->nrel] = name;
    E->nrel++;
}

int newtemp(int words)
{
    curlocal = curlocal + words;
    if (curlocal > maxlocal)
        maxlocal = curlocal;
    return curlocal - words + 1;
}

/* discard n words from the evaluation stack (the P-machine has no pop) */
void drop(int n)
{
    while (n-- > 0)
        gen_stl(scratch);
}

/* ---- labels and jumps ---- */

int newlabel(void)
{
    if (E->nlab >= E->maxlab)
        fatal(89 /* function too large (labels) */, 0);
    E->labpos[E->nlab] = -1;
    return E->nlab++;
}

void setlabel(int l)
{
    int i;
    /* a UJP to this very label just before it (a return at the end of a
       function, an if without else ...) is 2 bytes for nothing: drop it,
       with the labels already set after it */
    if (E->nfix > 0 && E->fixpos[E->nfix - 1] == E->pc - 1 && E->fixlab[E->nfix - 1] == l &&
        E->code[E->pc - 2] == O_UJP) {
        E->nfix--;
        E->pc = E->pc - 2;
        for (i = 0; i < E->nlab; i++)
            if (E->labpos[i] == E->pc + 2)
                E->labpos[i] = E->pc;
    }
    E->labpos[l] = E->pc;
}

void jmpop(int op, int l)
{
    ob(op);
    if (E->nfix >= E->maxfix)
        fatal(97 /* function too large (jumps) */, 0);
    E->fixpos[E->nfix] = E->pc;
    E->fixlab[E->nfix] = l;
    E->nfix++;
    ob(0);
}

void jump(int l)
{
    jmpop(O_UJP, l);
}

/* a word in a case table: self-relative, target = address - word */
void caseword(int l)
{
    if (E->nfix >= E->maxfix)
        fatal(97 /* function too large (jumps) */, 0);
    E->fixpos[E->nfix] = E->pc;
    E->fixlab[E->nfix] = -2 - l;
    E->nfix++;
    ob(0);
    ob(0);
}

/* ---- types ---- */

int valwords(struct Type *t)
{
    if (t->kind == TY_STRUCT || t->kind == TY_UNION || t->kind == TY_ARRAY)
        return 1;           /* represented by its address */
    if (t->kind == TY_VOID)
        return 0;
    return twords(t);
}

int ischar(struct Type *t)
{
    return t->kind == TY_CHAR || t->kind == TY_UCHAR;
}

static int ismulti(struct Type *t)
{
    return isfloatty(t) || islongty(t);
}

/* ---- lvalues ---- */

static void outw(int w)
{
    putc(w & 255, objout);
    putc((w >> 8) & 255, objout);
}

static void outs(char *s)
{
    int n;
    n = strlen(s);
    putc(n, objout);
    while (*s)
        putc(*s++, objout);
}

static char *genmod;

void gen_objheader(char *modname)
{
    genmod = modname;
    names = (struct Name **)calloc(NHASH, sizeof(struct Name *));
    if (!names)
        fatal(2 /* out of memory */, 0);
    ninit = 0;
    ininit = 0;
    lvtemp = 0;
    lvnode = 0;
    setupemit(&fe, MAXCODE, MAXLABEL, MAXFIX, MAXREL);
    setupemit(&ie, 1600, 40, 80, 300);
    E = &fe;
    fputs("TCOB", objout);
    putc('M', objout);
    outs(modname);
}

void gen_objdata(char *name, int words, int strong)
{
    putc('D', objout);
    putc(strong, objout);
    outs(name);
    outw(words);
}

void gen_objuse(char *name)
{
    putc('U', objout);
    outs(name);
}

void gen_objend(int staticwords)
{
    putc('G', objout);
    outw(staticwords);
    putc('E', objout);
}

/* finish the procedure in E and write it: flags 1 = init, 2 = static */
static void endproc(char *name, char *seg, int exitlab, int rw, int flags)
{
    int longlab[MAXLONGJ];
    int nlong;
    int i;
    int k;
    int t;
    int pos;
    int off;
    int jtab;
    int exitpos;
    int base;
    exitpos = E->labpos[exitlab];
    /* resolve jumps */
    nlong = 0;
    for (i = 0; i < E->nfix; i++) {
        pos = E->fixpos[i];
        if (E->fixlab[i] <= -2)
            continue;
        t = E->labpos[E->fixlab[i]];
        if (t < 0) {
            error(101 /* internal: undefined label */, name);
            continue;
        }
        off = t - (pos + 1);
        if (off >= 0 && off <= 127) {
            E->code[pos] = off;
            continue;
        }
        for (k = 0; k < nlong; k++)
            if (longlab[k] == t)
                break;
        if (k == nlong) {
            if (nlong >= MAXLONGJ)
                fatal(102 /* function too large (more than 60 long jumps); split it */, name);
            longlab[nlong++] = t;
        }
        E->code[pos] = (256 - 10 - 2 * k) & 255;
    }
    if (E->pc & 1)
        ob(0);
    base = E->pc;
    jtab = base + 2 * nlong + 8;
    for (k = nlong - 1; k >= 0; k--) {
        pos = jtab - 10 - 2 * k;
        ob(pos - longlab[k]);
        ob((pos - longlab[k]) >> 8);
    }
    ob((maxlocal - nparamwords) * 2);
    ob(((maxlocal - nparamwords) * 2) >> 8);
    ob(nparamwords * 2);
    ob((nparamwords * 2) >> 8);
    ob(jtab - 4 - exitpos);
    ob((jtab - 4 - exitpos) >> 8);
    ob(jtab - 2);
    ob((jtab - 2) >> 8);
    ob(0);                          /* procedure number: the linker */
    ob(1);                          /* lex level */
    /* case tables (after everything else is placed) */
    for (i = 0; i < E->nfix; i++) {
        if (E->fixlab[i] > -2)
            continue;
        pos = E->fixpos[i];
        t = E->labpos[-2 - E->fixlab[i]];
        E->code[pos] = (pos - t) & 255;
        E->code[pos + 1] = ((pos - t) >> 8) & 255;
    }
    putc('P', objout);
    putc(flags, objout);
    outs(name);
    outs(seg);
    outw(nparamwords * 2);
    putc(rw, objout);
    outw(E->pc);
    outw(jtab);
    for (i = 0; i < E->pc; i++)
        putc(E->code[i], objout);
    outw(E->nrel);
    for (i = 0; i < E->nrel; i++) {
        outw(E->relpos[i]);
        putc(E->reltype[i], objout);
        outs(E->relname[i]);
    }
}

void gen_funcbegin(void)
{
    E = &fe;
    E->pc = 0;
    E->nlab = 0;
    E->nfix = 0;
    E->nrel = 0;
    lvtemp = 0;
}

void gen_funcend(char *name, struct Type *ft, int exitlab, int isstatic, char *seg)
{
    int rw;
    rw = retwords(ft);
    setlabel(exitlab);
    ob(O_RNP);
    ob(rw);
    endproc(name, seg, exitlab, rw, isstatic ? 2 : 0);
}

/* ---- initialisers ---- */

static int initexit;

static void initstart(void)
{
    E->pc = 0;
    E->nlab = 0;
    E->nfix = 0;
    E->nrel = 0;
    curlocal = 0;
    scratch = 1;
    curlocal = 1;
    maxlocal = 1;
    nparamwords = 0;
    initexit = newlabel();
}

void gen_initflush(void)
{
    char name[40];
    struct Emit *save;
    save = E;
    saveframe();
    E = &ie;
    if (ininit == 0 && E->pc > 0) {
        loadframe();
        setlabel(initexit);
        ob(O_RNP);
        ob(0);
        strcpy(name, genmod);
        strcat(name, "'init");
        itoa10(ninit, name + strlen(name));
        ninit++;
        endproc(name, "INIT", initexit, 0, 1);
        E->pc = 0;
    }
    E = save;
    loadframe();
}

void gen_initbegin(void)
{
    if (ininit++)
        return;
    saveframe();
    E = &ie;
    if (E->pc == 0)
        initstart();
    else
        loadframe();
}

void gen_initend(void)
{
    if (--ininit)
        return;
    saveframe();
    E = &fe;
    loadframe();
    if (ie.pc > ie.max - 500 || ie.nrel > ie.maxrel - 60)
        gen_initflush();
}
