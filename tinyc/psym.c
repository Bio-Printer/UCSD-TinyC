/* psym.c -- the parser: types and symbols. */
#include "tc.h"
#include "parse.h"
#pragma segment PARSE

#pragma segment PARSE

struct Type *ty_void;
struct Type *ty_char;
struct Type *ty_uchar;
struct Type *ty_int;
struct Type *ty_uint;
struct Type *ty_long;
struct Type *ty_ulong;
struct Type *ty_float;
struct Type *ty_double;
struct Type *ty_ldouble;
struct Type *ty_charp;

/* the symbol tables are allocated by each compile (in its pass memory) */
struct Sym **htab;              /* [HSIZE] */
struct Sym **ttab;              /* [HSIZE] */
struct Sym **scopes;            /* [40] */
int level;
struct Sym *labels;
int globoff;                    /* next free word of the module's static variables */
char *modname;
int segexplicit;               /* a #pragma segment has been seen */
int curfnseg;                   /* the segment of the function being compiled (index, symseg) */
int cursegi;                    /* the segment of #pragma segment (index; cursegname is its name) */
int usesfloat;          /* the module uses floating point (links printf's %f) */
int nofltused;

/* current function */
struct Sym *curfn;
struct Type *curft;
int exitlab;
int sretoff;
int vaoff;

/* current switch */
struct SwVals *swvals;
int swn;
int swdef;


/* ---- intrinsic functions ---- */

char *intrnames[] = {
    "", "__dvi", "__mdi", "__va_start", "__cspv", "__cspi", "__cspf",
    "__cxp0v", "__cxp0i", "__osvar", "__exitprog", "__osvaraddr", "__cspd", 0
};

/* ---- types ---- */

/* while a declarator from a system header is parsed, derived types go to
   the statement pool: most such declarations are dropped (see isref) */
int tentative;

struct Type *mktype(int kind, int size, int align)
{
    struct Type *t;
    if (tentative && (kind == TY_PTR || kind == TY_ARRAY || kind == TY_FUNC)) {
        t = (struct Type *)xalloc(sizeof(struct Type));
        t->flags = TF_TEMP;         /* marks a temporary type */
    } else {
        t = (struct Type *)palloc(sizeof(struct Type));
        if (align == 1)
            t->flags = TF_ALIGN1;
    }
    t->kind = kind;
    t->size = size;
    if (kind == TY_PTR || kind == TY_ARRAY)
        t->u.len = -1;              /* (fields, params: none) */
    return t;
}

/* set-up code runs once per compile: a segment of its own */
#pragma segment CINIT

void typeinit(void)
{
    ty_void = mktype(TY_VOID, 1, 1);
    ty_char = mktype(TY_CHAR, 1, 1);
    ty_uchar = mktype(TY_UCHAR, 1, 1);
    ty_int = mktype(TY_INT, 2, 2);
    ty_uint = mktype(TY_UINT, 2, 2);
    ty_long = mktype(TY_LONG, 4, 2);
    ty_ulong = mktype(TY_ULONG, 4, 2);
    ty_float = mktype(TY_FLOAT, 4, 2);
    ty_double = mktype(TY_DOUBLE, 8, 2);     /* 8 bytes: CSP 100.. (P-Code mode) */
    ty_ldouble = mktype(TY_LDOUBLE, 8, 2);
    ty_charp = ptrto(ty_char);
    globoff = 0;
}

#pragma segment PARSE

struct Type *ptrto(struct Type *t)
{
    struct Type *p;
    if (t->ptrto)
        return t->ptrto;
    if (tentative) {
        p = mktype(TY_PTR, 2, 2);   /* temporary: not cached */
        p->base = t;
        return p;
    }
    p = mktype(TY_PTR, 2, 2);
    p->base = t;
    t->ptrto = p;
    return p;
}

struct Type *arrayof(struct Type *t, int n)
{
    struct Type *a;
    a = mktype(TY_ARRAY, n < 0 ? -1 : W16(n * t->size), talign(t));
    a->base = t;
    a->u.len = n;
    return a;
}

/* the permanent function type with f's signature: function types are
   shared (a module declares hundreds of functions with a few dozen
   signatures).  f is a temporary whose result and parameter types are
   permanent. */
struct Type *functypes;

struct Type *functype(struct Type *f)
{
    struct Type *t;
    struct Param *p;
    struct Param *q;
    struct Param *last;
    for (t = functypes; t; t = t->v.next) {
        if (t->base != f->base || (t->flags & 3) != (f->flags & 3))
            continue;
        for (p = t->u.params, q = f->u.params; p && q; p = p->next, q = q->next)
            if (p->type != q->type)
                break;
        if (!p && !q)
            return t;
    }
    t = (struct Type *)palloc(sizeof(struct Type));
    t->kind = TY_FUNC;
    t->size = 2;
    t->base = f->base;
    t->flags = f->flags & (TF_VARIADIC | TF_OLDSTYLE);
    last = 0;
    for (q = f->u.params; q; q = q->next) {
        p = (struct Param *)palloc(sizeof(struct Param));
        p->type = q->type;
        if (last)
            last->next = p;
        else
            t->u.params = p;
        last = p;
    }
    t->v.next = functypes;
    functypes = t;
    return t;
}

/* a permanent copy of a type built tentatively */
struct Type *permtype(struct Type *t)
{
    struct Type *n;
    struct Param *p;
    struct Param *q;
    struct Param *last;
    if (!(t->flags & TF_TEMP))
        return t;
    if (t->kind == TY_PTR)
        return ptrto(permtype(t->base));
    if (t->kind == TY_ARRAY)
        return arrayof(permtype(t->base), t->u.len);
    n = (struct Type *)xalloc(sizeof(struct Type));
    n->kind = TY_FUNC;
    n->base = permtype(t->base);
    n->flags = t->flags & (TF_VARIADIC | TF_OLDSTYLE);
    last = 0;
    for (p = t->u.params; p; p = p->next) {
        q = (struct Param *)xalloc(sizeof(struct Param));
        q->type = permtype(p->type);
        if (last)
            last->next = q;
        else
            n->u.params = q;
        last = q;
    }
    return functype(n);
}

/* ---- the names the program itself uses (see scanrefs) ----
   A Bloom filter: 4096 bits, two hash functions.  A false positive only
   keeps a declaration that was not needed. */
unsigned char *refbits;

int refhash2(char *s)
{
    int h;
    h = 7;
    while (*s)
        h = (h * 31 + *s++) & 4095;
    return h;
}

int isref(char *name)
{
    int a;
    int b;
    a = (hashstr(name) * 2 + 1) & 4095;
    b = refhash2(name);
    return (refbits[a >> 3] & (1 << (a & 7))) && (refbits[b >> 3] & (1 << (b & 7)));
}

int sametype(struct Type *a, struct Type *b)
{
    if (a == b)
        return 1;
    if (a->kind != b->kind)
        return 0;
    if (a->kind == TY_PTR || a->kind == TY_ARRAY)
        return sametype(a->base, b->base);
    return a->kind != TY_STRUCT && a->kind != TY_UNION && a->kind != TY_FUNC;
}

/* ---- symbols ---- */

struct Sym *lookup(char *name)
{
    struct Sym *s;
    for (s = htab[hashstr(name) & (HSIZE - 1)]; s; s = s->next)
        if (strcmp(s->name, name) == 0)
            return s;
    return 0;
}

struct Sym *lookuptag(char *name)
{
    struct Sym *s;
    for (s = ttab[hashstr(name) & (HSIZE - 1)]; s; s = s->next)
        if (strcmp(s->name, name) == 0)
            return s;
    return 0;
}

struct Sym *addsym(char *name, int kind, struct Type *t)
{
    struct Sym *s;
    int h;
    if (level == 0) {
        s = (struct Sym *)palloc(sizeof(struct Sym));
        s->name = pstrdup(name);
    } else {
        s = (struct Sym *)falloc(sizeof(struct Sym));
        s->name = falloc(strlen(name) + 1);
        strcpy(s->name, name);
    }
    s->kind = kind;
    s->type = t;
    s->level = level;
    h = hashstr(name) & (HSIZE - 1);
    if (kind == S_TAG) {
        s->next = ttab[h];
        ttab[h] = s;
    } else {
        s->next = htab[h];
        htab[h] = s;
    }
    s->scopenext = scopes[level];
    scopes[level] = s;
    return s;
}

void pushscope(void)
{
    level++;
    if (level >= 40)
        fatal(33 /* blocks nested too deeply */, 0);
    scopes[level] = 0;
}

void popscope(void)
{
    struct Sym *s;
    int h;
    for (s = scopes[level]; s; s = s->scopenext) {
        h = hashstr(s->name) & (HSIZE - 1);
        if (s->kind == S_TAG)
            ttab[h] = s->next;
        else
            htab[h] = s->next;
    }
    level--;
}

int allocglobal(struct Type *t)
{
    int off;
    int w;
    off = globoff;
    w = (t->size + 1) / 2;
    if (w <= 0)
        w = 1;
    globoff = globoff + w;
    if (globoff > 16000 || globoff < 0)
        fatal(34 /* too many global variables */, 0);
    return off;
}

int alloclocal(struct Type *t)
{
    int w;
    w = (t->size + 1) / 2;
    if (w <= 0)
        w = 1;
    curlocal = curlocal + w;
    if (curlocal > maxlocal)
        maxlocal = curlocal;
    if (curlocal > 12000)
        fatal(35 /* local variables too large */, 0);
    return curlocal - w + 1;
}

/* ---- tokens ---- */

void expect(int t, char *what)
{
    if (tok != t) {
        error(36 /* expected */, what);
        return;
    }
    next();
}

int istypename(void)
{
    struct Sym *s;
    if (tok >= K_FIRST && tok <= K_LAST) {
        switch (tok) {
        case K_CHAR: case K_CONST: case K_DOUBLE: case K_ENUM: case K_EXTERN:
        case K_FLOAT: case K_INT: case K_LONG: case K_REGISTER: case K_SHORT:
        case K_SIGNED: case K_STATIC: case K_STRUCT: case K_TYPEDEF: case K_UNION:
        case K_UNSIGNED: case K_VOID: case K_VOLATILE: case K_AUTO:
            return 1;
        }
        return 0;
    }
    if (tok == T_ID) {
        s = lookup(tokname);
        return s && s->kind == S_TYPEDEF;
    }
    return 0;
}

/* ---- nodes ---- */
