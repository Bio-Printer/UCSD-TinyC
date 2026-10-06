/* ir.c -- the intermediate file between the parser and the code generator.
 *
 * The parser (pass 2) writes one record per statement-level event:
 * labels and jumps (control flow is already lowered), conditional
 * branches, expressions to evaluate, returns, switch tables, and the
 * start and end of every function and initialiser.  Expressions are
 * written as trees with their types.  The code generator (pass 3) reads
 * the records back and generates exactly what it would have generated
 * had it been called directly -- but the two are never in memory at the
 * same time.
 *
 *   H modname            F nparam scratch      L lab      J lab
 *   B curlocal jumpif lab tree                 D curlocal tree
 *   V curlocal temp tree  (evaluate, store in temp)
 *   R curlocal sretoff functype [tree]         S temp n (val lab)* default
 *   E maxlocal static name seg exitlab functype
 *   I / i  initialiser begin / end             Z flush initialisers
 *   d strong name words  (an exported variable)
 *   G staticwords        Q end of file
 */
#include "tc.h"

/* ---- the writer (parser segment) ---- */
#pragma segment PARSE

static FILE *irout;
static int irlabels;
static int irinit;

/* types already written in the current record: later uses are 0xFF + index */
#define MAXTSEEN 64
static struct Type **tseen;     /* [MAXTSEEN], allocated per compile */
static int ntseen;

static void irb(int b)
{
    putc(b & 255, irout);
}

static void irw(int w)
{
    putc(w & 255, irout);
    putc((w >> 8) & 255, irout);
}

static void irs(char *s)
{
    irb(strlen(s));
    while (*s)
        irb(*s++);
}

static void irtype(struct Type *t)
{
    struct Param *p;
    int n;
    for (n = 0; n < ntseen; n++)
        if (tseen[n] == t) {
            irb(255);
            irb(n);
            return;
        }
    if (ntseen < MAXTSEEN)
        tseen[ntseen++] = t;
    irb(t->kind);
    irw(t->size);
    if (t->kind == TY_PTR || t->kind == TY_ARRAY) {
        irw(t->len);
        irtype(t->base);
    } else if (t->kind == TY_FUNC) {
        irb(t->variadic + 2 * t->oldstyle);
        irtype(t->base);
        n = 0;
        for (p = t->params; p; p = p->next)
            n++;
        irb(n);
        for (p = t->params; p; p = p->next)
            irtype(p->type);
    }
}

static void irnode(struct Node *n)
{
    int mask;
    int i;
    mask = 0;
    if (n->a)
        mask = mask | 1;
    if (n->b)
        mask = mask | 2;
    if (n->c)
        mask = mask | 4;
    if (n->next)
        mask = mask | 8;
    if (n->val)
        mask = mask | 16;
    if (n->val2)
        mask = mask | 32;
    if (n->sym)
        mask = mask | 64;
    irb(n->op);
    irtype(n->type);
    irb(mask);
    if (mask & 16)
        irw(n->val);
    if (mask & 32)
        irw(n->val2);
    if (mask & 64) {
        irb(n->sym->kind);
        irw(n->sym->offset);
        if (n->sym->kind == S_FUNC)
            irs(n->sym->lname ? n->sym->lname : n->sym->name);
        else
            irs(n->sym->offset < 0 ? n->sym->name : "");   /* globals by name */
    }
    if (n->op == N_STR || n->op == N_HEAPSTR) {
        irw(n->slen);
        for (i = 0; i < n->slen; i++)
            irb(n->str[i]);
    } else if (n->op == N_FNUM) {
        for (i = 0; i < n->type->size; i++)     /* 4, or 12 for a double */
            irb(n->fimg[i]);
    }
    if (mask & 1)
        irnode(n->a);
    if (mask & 2)
        irnode(n->b);
    if (mask & 4)
        irnode(n->c);
    if (mask & 8)
        irnode(n->next);
}

static void ircur(void)
{
    ntseen = 0;
    irw(irinit ? -1 : curlocal);
}

void ir_open(char *name, char *modname)
{
    irout = fopen(name, "wb");
    if (!irout)
        fatal(24 /* cannot create */, name);
    tseen = (struct Type **)malloc(MAXTSEEN * sizeof(struct Type *));
    if (!tseen)
        fatal(2 /* out of memory */, 0);
    irlabels = 0;
    irinit = 0;
    ntseen = 0;
    fputs("TCIR", irout);
    irb('H');
    irs(modname);
}

void ir_close(int globalwords)
{
    irb('G');
    irw(globalwords);
    irb('Q');
    fclose(irout);
}

void ir_data(char *name, int words, int strong)
{
    irb('d');
    irb(strong);
    irs(name);
    irw(words);
}

void ir_use(char *name)
{
    irb('u');
    irs(name);
}

void ir_funcbegin(void)
{
    irlabels = 0;
    irb('F');
    irw(nparamwords);
    irw(scratch);
}

int ir_newlabel(void)
{
    if (irlabels >= MAXLABEL)
        fatal(89 /* function too large (labels) */, 0);
    return irlabels++;
}

void ir_setlabel(int l)
{
    irb('L');
    irw(l);
}

void ir_jump(int l)
{
    irb('J');
    irw(l);
}

void ir_branch(struct Node *n, int l, int jumpif)
{
    irb('B');
    ircur();
    irb(jumpif);
    irw(l);
    irnode(n);
}

void ir_discard(struct Node *n)
{
    irb('D');
    ircur();
    irnode(n);
}

void ir_valuestl(struct Node *n, int t)
{
    irb('V');
    ircur();
    irw(t);
    irnode(n);
}

void ir_return(struct Node *n, struct Type *ft, int sretoff)
{
    ntseen = 0;
    irb('R');
    ircur();
    irw(sretoff);
    irtype(ft);
    irb(n != 0);
    if (n)
        irnode(n);
}

void ir_switch(int t, int *vals, int *labs, int n, int deflab)
{
    int i;
    irb('S');
    irw(t);
    irw(n);
    for (i = 0; i < n; i++) {
        irw(vals[i]);
        irw(labs[i]);
    }
    irw(deflab);
}

void ir_funcend(char *name, struct Type *ft, int exitlab, int isstatic, char *seg)
{
    ntseen = 0;
    irb('E');
    irw(maxlocal);
    irb(isstatic);
    irs(name);
    irs(seg);
    irw(exitlab);
    irtype(ft);
}

void ir_initbegin(void)
{
    if (irinit++ == 0)
        irb('I');
}

void ir_initend(void)
{
    if (--irinit == 0)
        irb('i');
}

void ir_initflush(void)
{
    irb('Z');
}

/* ---- the reader (code generator segment) ---- */
#pragma segment GEN

static FILE *irin;
static struct Type **rseen;     /* [MAXTSEEN], allocated per run */
static int nrseen;
static int *lmap;
static int nlmap;

static int rb(void)
{
    int c;
    c = getc(irin);
    if (c == EOF)
        fatal(90 /* intermediate file truncated */, 0);
    return c;
}

static int rw(void)
{
    int lo;
    lo = rb();
    return W16(lo + rb() * 256);
}

static char *rstr(void)
{
    int n;
    int i;
    char *s;
    n = rb();
    s = xalloc(n + 1);
    for (i = 0; i < n; i++)
        s[i] = rb();
    s[n] = 0;
    return s;
}

static struct Type *rtype(void)
{
    struct Type *t;
    struct Param *p;
    struct Param *last;
    int n;
    int i;
    int f;
    n = rb();
    if (n == 255)
        return rseen[rb()];
    t = (struct Type *)xalloc(sizeof(struct Type));
    if (nrseen < MAXTSEEN)
        rseen[nrseen++] = t;
    t->kind = n;
    t->size = rw();
    t->len = -1;
    t->align = t->kind == TY_CHAR || t->kind == TY_UCHAR ? 1 : 2;
    if (t->kind == TY_PTR || t->kind == TY_ARRAY) {
        t->len = rw();
        t->base = rtype();
    } else if (t->kind == TY_FUNC) {
        f = rb();
        t->variadic = f & 1;
        t->oldstyle = (f >> 1) & 1;
        t->base = rtype();
        n = rb();
        last = 0;
        for (i = 0; i < n; i++) {
            p = (struct Param *)xalloc(sizeof(struct Param));
            p->type = rtype();
            if (last)
                last->next = p;
            else
                t->params = p;
            last = p;
        }
    }
    return t;
}

static struct Node *rnode(void)
{
    struct Node *n;
    struct Sym *s;
    int mask;
    int i;
    n = (struct Node *)xalloc(sizeof(struct Node));
    n->op = rb();
    n->type = rtype();
    mask = rb();
    if (mask & 16)
        n->val = rw();
    if (mask & 32)
        n->val2 = rw();
    if (mask & 64) {
        s = (struct Sym *)xalloc(sizeof(struct Sym));
        s->kind = rb();
        s->offset = rw();
        s->name = rstr();
        n->sym = s;
    }
    if (n->op == N_STR || n->op == N_HEAPSTR) {
        n->slen = rw();
        n->str = xalloc(n->slen + 1);
        for (i = 0; i < n->slen; i++)
            n->str[i] = rb();
    } else if (n->op == N_FNUM) {
        n->fimg = (unsigned char *)xalloc(n->type->size);
        for (i = 0; i < n->type->size; i++)
            n->fimg[i] = rb();
    }
    if (mask & 1)
        n->a = rnode();
    if (mask & 2)
        n->b = rnode();
    if (mask & 4)
        n->c = rnode();
    if (mask & 8)
        n->next = rnode();
    return n;
}

/* the parser's label numbers -> the code generator's */
static int lab(int l)
{
    if (l < 0 || l >= nlmap)
        fatal(91 /* internal: bad label in intermediate file */, 0);
    if (lmap[l] < 0)
        lmap[l] = newlabel();
    return lmap[l];
}

static void setcur(void)
{
    int c;
    c = rw();
    if (c >= 0) {
        curlocal = c;
        if (curlocal > maxlocal)
            maxlocal = curlocal;
    }
}

int gencode(char *irname, char *obj)
{
    char magic[5];
    char *modname;
    char *name;
    char *seg;
    int c;
    int i;
    int m;
    int t;
    int n;
    int l;
    int jumpif;
    int ml;
    int st;
    int *vals;
    int *labs;
    struct Node *e;
    struct Type *ft;
    irin = fopen(irname, "rb");
    if (!irin)
        fatal(25 /* cannot open */, irname);
    for (i = 0; i < 4; i++)
        magic[i] = rb();
    magic[4] = 0;
    if (strcmp(magic, "TCIR") != 0 || rb() != 'H')
        fatal(92 /* not an intermediate file */, irname);
    objout = fopen(obj, "wb");
    if (!objout)
        fatal(24 /* cannot create */, obj);
    nlmap = MAXLABEL;
    lmap = (int *)malloc(nlmap * sizeof(int));
    rseen = (struct Type **)malloc(MAXTSEEN * sizeof(struct Type *));
    if (!rseen)
        fatal(2 /* out of memory */, 0);
    if (!lmap)
        fatal(2 /* out of memory */, 0);
    m = xmark();
    modname = pstrdup(rstr());
    xrelease(m);
    gen_objheader(modname);
    for (;;) {
        m = xmark();
        nrseen = 0;
        c = rb();
        switch (c) {
        case 'F':
            gen_funcbegin();
            nparamwords = rw();
            scratch = rw();
            curlocal = scratch;
            maxlocal = scratch;
            for (i = 0; i < nlmap; i++)
                lmap[i] = -1;
            break;
        case 'L':
            setlabel(lab(rw()));
            break;
        case 'J':
            jump(lab(rw()));
            break;
        case 'B':
            setcur();
            jumpif = rb();
            l = lab(rw());
            branch(rnode(), l, jumpif);
            break;
        case 'D':
            setcur();
            gen_discard(rnode());
            break;
        case 'V':
            setcur();
            t = rw();
            gen_value(rnode());
            gen_stl(t);
            break;
        case 'R':
            setcur();
            st = rw();
            ft = rtype();
            e = rb() ? rnode() : 0;
            gen_return(e, ft, st);
            break;
        case 'S':
            t = rw();
            n = rw();
            if (n > 1024)
                fatal(93 /* too many cases */, 0);
            /* the case table, as large as this switch needs (not 1024
               cases for the whole pass) */
            vals = (int *)malloc(2 * n * sizeof(int) + 2);
            if (!vals)
                fatal(2 /* out of memory */, 0);
            labs = vals + n;
            for (i = 0; i < n; i++) {
                vals[i] = rw();
                labs[i] = lab(rw());
            }
            gen_switch(t, vals, labs, n, lab(rw()));
            free(vals);
            break;
        case 'E':
            ml = rw();
            if (ml > maxlocal)
                maxlocal = ml;
            st = rb();
            name = rstr();
            seg = rstr();
            l = lab(rw());
            ft = rtype();
            gen_funcend(name, ft, l, st, seg);
            break;
        case 'I':
            gen_initbegin();
            break;
        case 'i':
            gen_initend();
            break;
        case 'Z':
            gen_initflush();
            break;
        case 'd':
            st = rb();
            name = rstr();
            gen_objdata(name, rw(), st);
            break;
        case 'u':
            gen_objuse(rstr());
            break;
        case 'G':
            gen_objend(rw());
            break;
        case 'Q':
            fclose(irin);
            fclose(objout);
            return nerrors == 0;
        default:
            fatal(94 /* bad intermediate file record */, 0);
        }
        xrelease(m);
    }
}
