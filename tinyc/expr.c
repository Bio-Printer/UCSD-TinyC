/* expr.c -- the parser: expressions. */
#include "tc.h"
#include "parse.h"
#pragma segment PARSE

/* ---- doubles (8 bytes: IEEE binary64) ---- */

/* a double constant's image from its text ("123e-2") */
void dblimage(char *text, unsigned char *img)
{
#ifdef __TINYC__
    __cspi(135, text, img);             /* ATODM (P-Code mode, like doubles) */
#else
    double d;
    unsigned long long u;
    int i;
    d = strtod(text, 0);
    memcpy(&u, &d, 8);
    for (i = 0; i < 8; i++) {
        img[i] = u & 255;
        u = u >> 8;
    }
#endif
}

/* a 4-byte UCSD real's exact value as a double image: 0.1m(24) * 2^(e-128)
   = 1.m(23) * 2^(e-129); binary64 exponent e - 129 + 1023 */
void real2dbl(unsigned char *f, unsigned char *img)
{
    int e;
    memset(img, 0, 8);
    if (f[0] == 0)
        return;
    e = f[0] + 894;
    img[3] = (f[3] & 7) << 5;
    img[4] = (f[3] >> 3) | ((f[2] & 7) << 5);
    img[5] = (f[2] >> 3) | ((f[1] & 7) << 5);
    img[6] = ((f[1] & 127) >> 3) | ((e & 15) << 4);
    img[7] = (f[1] & 128) | (e >> 4);
}

/* math.h's functions (declared for float) called with a double first
   argument are the engine's CSP 112..132 (__cspd), in this order */
static int dmathcsp(char *name)
{
    char *s;
    int k;
    int n;
    s = "sqrt sin cos tan asin acos atan atan2 exp log log10 pow floor ceil fabs fmod sinh cosh tanh ldexp frexp ";
    n = strlen(name);
    for (k = 112; *s; k++) {
        if (strncmp(s, name, n) == 0 && s[n] == ' ')
            return k;
        while (*s != ' ')
            s++;
        s++;
    }
    return 0;
}

/* the call c (its arguments already converted to float) as __cspd(k, ...)
   with the arguments as they were */
static struct Node *dmathcall(struct Node *c, int k)
{
    struct Node *first;
    struct Node *last;
    struct Node *a;
    struct Node *b;
    struct Node *nx;
    first = mknum(k, ty_int);
    last = first;
    for (a = c->b; a; a = nx) {
        nx = a->next;
        b = a;
        if (a->op == N_CAST && isfloatty(a->type) && !isdblty(a->type))
            b = cast(a->a, ty_double);          /* the argument before float */
        else if (isfloatty(a->type) && !isdblty(a->type))
            b = cast(a, ty_double);             /* a float (e.g. a constant) */
        b->next = 0;
        last->next = b;
        last = b;
    }
    c = mknode(N_INTRIN, ty_double, first, 0);
    c->val = I_CSPD;
    return c;
}

struct Node *mknode(int op, struct Type *t, struct Node *a, struct Node *b)
{
    struct Node *n;
    n = (struct Node *)xalloc(sizeof(struct Node));
    n->op = op;
    n->type = t;
    n->a = a;
    n->b = b;
    return n;
}

struct Node *mknum(int v, struct Type *t)
{
    struct Node *n;
    n = mknode(N_NUM, t, 0, 0);
    n->val = W16(v);
    if (islongty(t))
        n->val2 = (n->val < 0 && t->kind == TY_LONG) ? -1 : 0;
    return n;
}

int isconst(struct Node *n)
{
    return n->op == N_NUM && !islongty(n->type);
}

int islvalue(struct Node *n)
{
    return n->op == N_VAR || n->op == N_MEMBER || n->op == N_DEREF;
}

/* in global initialisers a string literal would point into the INIT
   segment, which is gone once the program runs: copy it to a global */
int globinit;

/* array -> pointer to first element, function -> function pointer */
struct Node *decay(struct Node *n)
{
    if (n->op == N_STR && globinit) {
        n->op = N_HEAPSTR;          /* copied to the heap at startup */
        n->type = ptrto(ty_char);
        return n;
    }
    if (n->type->kind == TY_ARRAY)
        return mknode(N_ADDR, ptrto(n->type->base), n, 0);
    if (n->type->kind == TY_FUNC)
        return mknode(N_ADDR, ptrto(n->type), n, 0);
    return n;
}

/* a runtime helper's type letter: int, unsigned, float, long, unsigned long */
static struct Type *htype(int c)
{
    switch (c) {
    case 'i': return ty_int;
    case 'u': return ty_uint;
    case 'f': return ty_float;
    case 'l': return ty_long;
    case 'L': return ty_ulong;
    }
    return 0;
}

/* Declare the runtime helper NAME (__NAME in tcrt.c) when it is first
   needed, not all of them in every module: each is a symbol, a type and
   its parameters in the memory of the whole pass.  The table is in the
   code (two string constants): NAME, then its result and parameter types
   (0: none). */
static struct Sym *declhelper(char *name)
{
    char *p;
    struct Type *ft;
    struct Param *q;
    struct Sym *s;
    struct Sym **pp;
    int n;
    int k;
    int savelevel;
    int savetent;
    for (k = 0; k < 2; k++) {
        p = k ? "ladd lll lsub lll lmul lll ldiv lll lmod lll uldiv LLL ulmod LLL land lll lor lll lxor lll lshl lli lshr lli ulshr LLi lneg ll0 lnot ll0 lcmp ill ulcmp iLL "
              : "divi iii modi iii udiv uuu umod uuu shl iii shr iii ushr uui xor iii sx ii0 utof fu0 ftou uf0 itol li0 utol lu0 ltoi il0 ltof fl0 ultof fL0 ftol lf0 ftoul Lf0 ";
        while (*p) {
            for (n = 0; p[n] != ' '; n++)
                ;
            if (strncmp(p, name + 2, n) == 0 && name[n + 2] == 0)
                break;
            p = p + n + 5;
        }
        if (*p)
            break;
    }
    if (k == 2)
        return 0;
    p = p + n + 1;
    savelevel = level;              /* a file-level, permanent declaration */
    savetent = tentative;
    level = 0;
    tentative = 0;
    ft = mktype(TY_FUNC, 2, 2);
    ft->base = htype(p[0]);
    if (p[1] != '0') {
        q = (struct Param *)palloc(sizeof(struct Param));
        q->type = htype(p[1]);
        ft->params = q;
        if (p[2] != '0') {
            q->next = (struct Param *)palloc(sizeof(struct Param));
            q->next->type = htype(p[2]);
        }
    }
    s = addsym(name, S_FUNC, ft);
    level = savelevel;
    tentative = savetent;
    if (level > 0) {
        /* popscope takes each scope's symbols off the front of their hash
           chains: keep this one behind the local ones */
        pp = &htab[hashstr(name) & (HSIZE - 1)];
        *pp = s->next;
        while (*pp && (*pp)->level > 0)
            pp = &(*pp)->next;
        s->next = *pp;
        *pp = s;
    }
    return s;
}

struct Sym *helper(char *name)
{
    struct Sym *s;
    s = lookup(name);
    if (!s)
        s = declhelper(name);
    if (!s || s->kind != S_FUNC)
        fatal(37 /* runtime helper not declared (tcrt.h) */, name);
    return s;
}

struct Node *call1(char *name, struct Node *a, struct Node *b)
{
    struct Sym *s;
    struct Node *n;
    struct Node *f;
    struct Param *p;
    s = helper(name);
    f = mknode(N_FUNC, s->type, 0, 0);
    f->sym = s;
    n = mknode(N_CALL, s->type->base, f, 0);
    p = s->type->params;
    n->b = a;
    if (b)
        a->next = b;
    return n;
}

/* an integer constant (int, unsigned, long, unsigned long) as decimal text
   in buf[12]: 32-bit division by 10 done a byte at a time */
static char *numtext(struct Node *n, char *buf)
{
    unsigned char b[4];
    unsigned hi;
    unsigned lo;
    int neg;
    int i;
    int k;
    int r;
    int nz;
    lo = n->val;
    if (islongty(n->type))
        hi = n->val2;
    else
        hi = !isunsignedty(n->type) && n->val < 0 ? -1 : 0;
    neg = !isunsignedty(n->type) && (int)hi < 0;
    if (neg) {
        lo = -lo;
        hi = ~hi + (lo == 0);
    }
    b[0] = hi >> 8;
    b[1] = hi & 255;
    b[2] = lo >> 8;
    b[3] = lo & 255;
    i = 11;
    buf[i] = 0;
    do {
        r = 0;
        nz = 0;
        for (k = 0; k < 4; k++) {
            r = r * 256 + b[k];
            b[k] = r / 10;
            r = r % 10;
            nz = nz | b[k];
        }
        buf[--i] = '0' + r;
    } while (nz);
    if (neg)
        buf[--i] = '-';
    return buf + i;
}

struct Node *cast(struct Node *n, struct Type *t);

struct Node *helpercall(char *name, struct Node *a, struct Node *b)
{
    struct Sym *s;
    struct Param *p;
    s = helper(name);
    p = s->type->params;
    a = cast(a, p->type);
    if (b)
        b = cast(b, p->next->type);
    return call1(name, a, b);
}

/* conversions between scalar types */
struct Node *cast(struct Node *n, struct Type *t)
{
    struct Type *f;
    int fk;
    int tk;
    n = decay(n);
    f = n->type;
    fk = f->kind;
    tk = t->kind;
    if (tk == TY_VOID) {
        n = mknode(N_CAST, t, n, 0);
        return n;
    }
    if (f == t || (fk == tk && fk != TY_STRUCT && fk != TY_UNION && fk != TY_ARRAY))
        return fk == tk && f != t ? mknode(N_CAST, t, n, 0) : n;
    if (tk == TY_STRUCT || tk == TY_UNION || tk == TY_ARRAY || tk == TY_FUNC) {
        if (fk != tk)
            error(38 /* invalid conversion */, 0);
        return n;
    }
    if (!isscalar(f)) {
        error(38 /* invalid conversion */, 0);
        return n;
    }
    /* double <-> others: CSP 106..111 (gen); constants become doubles here */
    if (isdblty(t) || isdblty(f)) {
        if (isdblty(t) && (n->op == N_FNUM || isconst(n))) {
            struct Node *c;
            char num[12];
            c = mknode(N_FNUM, t, 0, 0);
            c->fimg = (unsigned char *)xalloc(8);
            if (n->op == N_FNUM && n->str)
                dblimage(n->str, c->fimg);
            else if (n->op == N_FNUM)
                real2dbl(n->fimg, c->fimg);
            else
                dblimage(numtext(n, num), c->fimg);
            return c;
        }
        return mknode(N_CAST, t, n, 0);
    }
    /* long <-> others go through helpers */
    if (islongty(t) && !islongty(f)) {
        if (isfloatty(f))
            return call1(tk == TY_LONG ? "__ftol" : "__ftoul", n, 0);
        if (n->op == N_NUM) {
            struct Node *c;
            c = mknum(n->val, t);
            if (isunsignedty(f) || f->kind == TY_PTR)
                c->val2 = 0;
            else
                c->val2 = n->val < 0 ? -1 : 0;
            return c;
        }
        if (isunsignedty(f) || fk == TY_PTR)
            return mknode(N_CAST, t, call1("__utol", mknode(N_CAST, ty_uint, n, 0), 0), 0);
        return mknode(N_CAST, t, call1("__itol", mknode(N_CAST, ty_int, n, 0), 0), 0);
    }
    if (islongty(f) && !islongty(t)) {
        if (isfloatty(t))
            return mknode(N_CAST, t, call1(fk == TY_LONG ? "__ltof" : "__ultof", n, 0), 0);
        if (n->op == N_NUM)
            return cast(mknum(n->val, isunsignedty(f) ? ty_uint : ty_int), t);
        return cast(call1("__ltoi", n, 0), t);
    }
    if (islongty(f) && islongty(t))
        return mknode(N_CAST, t, n, 0);
    /* word and float */
    if (n->op == N_NUM && !isfloatty(t) && !isfloatty(f)) {
        int v;
        v = n->val;
        if (tk == TY_CHAR) {
            v = v & 255;
            if (v > 127)
                v = v - 256;
        } else if (tk == TY_UCHAR)
            v = v & 255;
        return mknum(v, t);
    }
    if (isfloatty(t) && isunsignedty(f))
        return mknode(N_CAST, t, call1("__utof", mknode(N_CAST, ty_uint, n, 0), 0), 0);
    if (isunsignedty(t) && isfloatty(f) && tk != TY_UCHAR)
        return call1("__ftou", n, 0);
    return mknode(N_CAST, t, n, 0);
}

struct Type *arith(struct Type *a, struct Type *b)
{
    if (a->kind == TY_LDOUBLE || b->kind == TY_LDOUBLE)
        return ty_ldouble;
    if (a->kind == TY_DOUBLE || b->kind == TY_DOUBLE)
        return ty_double;
    if (a->kind == TY_FLOAT || b->kind == TY_FLOAT)
        return ty_float;
    if (a->kind == TY_ULONG || b->kind == TY_ULONG)
        return ty_ulong;
    if (a->kind == TY_LONG || b->kind == TY_LONG)
        return ty_long;
    if (a->kind == TY_UINT || b->kind == TY_UINT)
        return ty_uint;
    return ty_int;
}

int fold(int op, int a, int b, int uns)
{
    unsigned ua;
    unsigned ub;
    ua = a & 65535;
    ub = b & 65535;
    switch (op) {
    case N_ADD: return W16(a + b);
    case N_SUB: return W16(a - b);
    case N_MUL: return W16(a * b);
    case N_DIV: return uns ? W16(ua / ub) : W16(a / b);
    case N_MOD: return uns ? W16(ua % ub) : W16(a % b);
    case N_AND: return a & b;
    case N_OR: return a | b;
    case N_XOR: return a ^ b;
    case N_SHL: return W16(a << (b & 15));
    case N_SHR: return uns ? W16(ua >> (b & 15)) : W16(a >> (b & 15));
    case N_EQ: return a == b;
    case N_NE: return a != b;
    case N_LT: return uns ? ua < ub : a < b;
    case N_LE: return uns ? ua <= ub : a <= b;
    case N_GT: return uns ? ua > ub : a > b;
    case N_GE: return uns ? ua >= ub : a >= b;
    }
    return 0;
}

char *lhelper(int op, int uns)
{
    switch (op) {
    case N_ADD: return "__ladd";
    case N_SUB: return "__lsub";
    case N_MUL: return "__lmul";
    case N_DIV: return uns ? "__uldiv" : "__ldiv";
    case N_MOD: return uns ? "__ulmod" : "__lmod";
    case N_AND: return "__land";
    case N_OR: return "__lor";
    case N_XOR: return "__lxor";
    case N_SHL: return "__lshl";
    case N_SHR: return uns ? "__ulshr" : "__lshr";
    }
    return 0;
}

struct Node *binop(int op, struct Node *a, struct Node *b)
{
    struct Type *t;
    struct Type *at;
    struct Type *bt;
    struct Node *n;
    int size;
    int uns;
    a = decay(a);
    b = decay(b);
    at = a->type;
    bt = b->type;
    /* pointer arithmetic */
    if ((op == N_ADD || op == N_SUB) && at->kind == TY_PTR && isintegral(bt)) {
        size = at->base->size;
        if (size < 1)
            size = 1;
        b = cast(b, ty_int);
        if (isconst(b))
            b = mknum(W16(b->val * size), ty_int);
        else if (size != 1)
            b = mknode(N_MUL, ty_int, b, mknum(size, ty_int));
        n = mknode(op, at, a, b);
        if (isconst(a) && isconst(b))
            return mknum(fold(op, a->val, b->val, 0), at);
        return n;
    }
    if (op == N_ADD && isintegral(at) && bt->kind == TY_PTR)
        return binop(op, b, a);
    if (op == N_SUB && at->kind == TY_PTR && bt->kind == TY_PTR) {
        size = at->base->size;
        n = mknode(N_SUB, ty_int, a, b);
        if (size > 1)
            n = binop(N_DIV, n, mknum(size, ty_int));
        return n;
    }
    if (op >= N_EQ && op <= N_GE) {
        if (at->kind == TY_PTR || bt->kind == TY_PTR) {
            n = mknode(op, ty_int, cast(a, ty_uint), cast(b, ty_uint));
            return n;
        }
        if (!isscalar(at) || !isscalar(bt)) {
            error(39 /* invalid comparison */, 0);
            return mknum(0, ty_int);
        }
        t = arith(at, bt);
        a = cast(a, t);
        b = cast(b, t);
        if (islongty(t))
            return mknode(op, ty_int, call1(t->kind == TY_ULONG ? "__ulcmp" : "__lcmp", a, b), mknum(0, ty_int));
        if (isconst(a) && isconst(b) && !isfloatty(t))
            return mknum(fold(op, a->val, b->val, isunsignedty(t)), ty_int);
        return mknode(op, ty_int, a, b);
    }
    if (!isscalar(at) || !isscalar(bt) || at->kind == TY_PTR || bt->kind == TY_PTR) {
        error(40 /* invalid operands */, 0);
        return mknum(0, ty_int);
    }
    if (op == N_SHL || op == N_SHR) {
        t = arith(at, ty_int);
        if (!isintegral(t) || !isintegral(bt)) {
            error(41 /* invalid shift */, 0);
            return mknum(0, ty_int);
        }
        a = cast(a, t);
        b = cast(b, ty_int);
        if (islongty(t))
            return call1(lhelper(op, isunsignedty(t)), a, b);
        if (isconst(a) && isconst(b))
            return mknum(fold(op, a->val, b->val, isunsignedty(t)), t);
        if (op == N_SHL && isconst(b) && b->val >= 0 && b->val < 15)
            return mknode(N_MUL, t, a, mknum(1 << b->val, ty_int));
        if (op == N_SHL)
            return mknode(N_CAST, t, call1("__shl", cast(a, ty_int), b), 0);
        if (isunsignedty(t))
            return call1("__ushr", a, b);
        return call1("__shr", a, b);
    }
    t = arith(at, bt);
    if ((op == N_MOD || op == N_AND || op == N_OR || op == N_XOR) && isfloatty(t)) {
        error(40 /* invalid operands */, 0);
        return mknum(0, ty_int);
    }
    a = cast(a, t);
    b = cast(b, t);
    uns = isunsignedty(t);
    if (islongty(t))
        return mknode(N_CAST, t, call1(lhelper(op, uns), a, b), 0);
    if (isconst(a) && isconst(b) && !isfloatty(t)) {
        if ((op == N_DIV || op == N_MOD) && b->val == 0)
            error(42 /* division by zero */, 0);
        else
            return mknum(fold(op, a->val, b->val, uns), t);
    }
    if (!isfloatty(t)) {
        if (op == N_DIV)
            return mknode(N_CAST, t, call1(uns ? "__udiv" : "__divi", a, b), 0);
        if (op == N_MOD)
            return mknode(N_CAST, t, call1(uns ? "__umod" : "__modi", a, b), 0);
        if (op == N_XOR)
            return mknode(N_CAST, t, call1("__xor", cast(a, ty_int), cast(b, ty_int)), 0);
    }
    return mknode(op, t, a, b);
}

/* value of a condition as int 0/1 is produced by gen; this checks type */
struct Node *fzero(void)
{
    struct Node *n;
    n = mknode(N_FNUM, ty_float, 0, 0);
    n->fimg = (unsigned char *)xalloc(4);
    return n;
}

struct Node *cond(struct Node *n)
{
    n = decay(n);
    if (!isscalar(n->type))
        error(43 /* scalar required */, 0);
    if (isfloatty(n->type))
        n = binop(N_NE, n, fzero());
    return n;
}

/* ---- expressions ---- */

struct Node *arglist(struct Type *ft, int *nargs)
{
    struct Node *first;
    struct Node *last;
    struct Node *a;
    struct Param *p;
    int n;
    first = 0;
    last = 0;
    n = 0;
    p = ft ? ft->params : 0;
    while (tok != ')' && tok != T_EOF) {
        a = assign();
        if (ft) {
            if (p) {
                a = cast(a, p->type);
                p = p->next;
            } else {
                if (!ft->variadic && !ft->oldstyle)
                    error(44 /* too many arguments */, 0);
                a = decay(a);
                if (a->type->kind == TY_CHAR || a->type->kind == TY_UCHAR)
                    a = cast(a, ty_int);
            }
        }
        a->next = 0;
        if (last)
            last->next = a;
        else
            first = a;
        last = a;
        n++;
        if (tok != ',')
            break;
        next();
    }
    if (ft && p)
        error(45 /* too few arguments */, 0);
    expect(')', ")");
    *nargs = n;
    return first;
}

struct Node *intrinsic(int code)
{
    struct Node *n;
    int na;
    next();
    expect('(', "(");
    n = mknode(N_INTRIN, ty_int, 0, 0);
    n->val = code;
    n->a = arglist(0, &na);
    if (code == I_VASTART) {
        if (!curft || !curft->variadic)
            error(46 /* __va_start outside a variadic function */, 0);
        n->val2 = vaoff;
        n->type = ty_charp;
    } else if (code == I_CSPV || code == I_CXP0V || code == I_EXITP)
        n->type = ty_void;
    else if (code == I_CSPF)
        n->type = ty_float;
    else if (code == I_CSPD)
        n->type = ty_double;
    else if (code == I_OSVARA)
        n->type = ptrto(ty_int);
    if ((code >= I_CSPV && code <= I_CXP0I) || code == I_OSVAR || code == I_OSVARA || code == I_CSPD) {
        if (!n->a || !isconst(n->a))
            error(47 /* constant expected */, intrnames[code]);
    }
    return n;
}

struct Node *primary(void)
{
    struct Node *n;
    struct Sym *s;
    int i;
    switch (tok) {
    case T_NUM:
        if (toklong & 1) {
            n = mknode(N_NUM, (toklong & 2) ? ty_ulong : ty_long, 0, 0);
            n->val = tokval;
            n->val2 = tokval2;
        } else
            n = mknum(tokval, (toklong & 2) ? ty_uint : ty_int);
        next();
        return n;
    case T_FNUM:
        if (!insys)
            usesfloat = 1;
        /* an unsuffixed constant is a float, with L a double; its text is
           kept: a float constant that meets a double becomes one exactly */
        n = mknode(N_FNUM, toklong ? ty_double : ty_float, 0, 0);
        n->str = xalloc(strlen(toknum) + 1);
        strcpy(n->str, toknum);
        if (toklong) {
            n->fimg = (unsigned char *)xalloc(8);
            dblimage(n->str, n->fimg);
        } else {
            n->fimg = (unsigned char *)xalloc(4);
            memcpy(n->fimg, tokreal, 4);
        }
        next();
        return n;
    case T_STR:
        {
            /* the literal's array type lives as long as the statement */
            struct Type *st;
            st = (struct Type *)xalloc(sizeof(struct Type));
            st->kind = TY_ARRAY;
            st->size = toklen;
            st->align = 1;
            st->base = ty_char;
            st->len = toklen;
            n = mknode(N_STR, st, 0, 0);
        }
        n->str = xalloc(toklen);
        memcpy(n->str, tokstr, toklen);
        n->slen = toklen;
        next();
        return n;
    case '(':
        next();
        n = expr();
        expect(')', ")");
        return n;
    case T_ID:
        s = lookup(tokname);
        if (!s && tokname[0] == '_' && tokname[1] == '_') {
            for (i = 1; intrnames[i]; i++)
                if (strcmp(tokname, intrnames[i]) == 0)
                    return intrinsic(i);
        }
        if (!s) {
            error(48 /* undeclared identifier */, tokname);
            next();
            return mknum(0, ty_int);
        }
        next();
        if (s->kind == S_ENUMC)
            return mknum(s->offset, ty_int);
        if (s->kind == S_FUNC) {
            n = mknode(N_FUNC, s->type, 0, 0);
            n->sym = s;
            return n;
        }
        if (s->kind == S_TYPEDEF) {
            error(49 /* unexpected type name */, s->name);
            return mknum(0, ty_int);
        }
        n = mknode(N_VAR, s->type, 0, 0);
        n->sym = s;
        return n;
    }
    error(50 /* expression expected */, 0);
    next();
    return mknum(0, ty_int);
}

struct Field *findfield(struct Type *t, char *name, int *off)
{
    struct Field *f;
    struct Field *r;
    int o;
    for (f = t->fields; f; f = f->next) {
        if (f->name && strcmp(f->name, name) == 0) {
            *off = f->offset;
            return f;
        }
        if (!f->name && (f->type->kind == TY_STRUCT || f->type->kind == TY_UNION)) {
            r = findfield(f->type, name, &o);
            if (r) {
                *off = f->offset + o;
                return r;
            }
        }
    }
    return 0;
}

struct Node *member(struct Node *n, char *name)
{
    struct Field *f;
    struct Node *m;
    int off;
    if (n->type->kind != TY_STRUCT && n->type->kind != TY_UNION) {
        error(51 /* not a structure */, name);
        return n;
    }
    if (n->type->size < 0)
        error(52 /* incomplete structure */, n->type->tag);
    f = findfield(n->type, name, &off);
    if (!f) {
        error(53 /* no such member */, name);
        return n;
    }
    m = mknode(N_MEMBER, f->type, n, 0);
    m->val = off;
    return m;
}

struct Node *deref(struct Node *n)
{
    n = decay(n);
    if (n->type->kind != TY_PTR) {
        error(54 /* pointer required */, 0);
        return n;
    }
    if (n->type->base->kind == TY_FUNC)
        return n;               /* *fp is the function itself */
    return mknode(N_DEREF, n->type->base, n, 0);
}

struct Node *postfix(void)
{
    struct Node *n;
    struct Node *c;
    struct Type *ft;
    int na;
    n = primary();
    for (;;) {
        if (tok == '[') {
            next();
            c = expr();
            expect(']', "]");
            n = deref(binop(N_ADD, n, c));
        } else if (tok == '(') {
            next();
            ft = 0;
            if (n->type->kind == TY_FUNC)
                ft = n->type;
            else if (n->type->kind == TY_PTR && n->type->base->kind == TY_FUNC) {
                ft = n->type->base;
            } else
                error(55 /* not a function */, 0);
            c = mknode(N_CALL, ft ? ft->base : ty_int, n, 0);
            if (n->op == N_FUNC && n->sym->seg && curfnseg && strcmp(n->sym->seg, curfnseg) == 0)
                n->val = 1;             /* the callee is in this segment: CGP */
            c->b = arglist(ft, &na);
            c->val = na;
            if (n->op == N_FUNC && ft && ft->variadic)
                fmtfix(n->sym->name, c->b);
            if (n->op == N_FUNC && c->b && c->b->op == N_CAST && isdblty(c->b->a->type) &&
                (na = dmathcsp(n->sym->name)) != 0)
                c = dmathcall(c, na);
            n = c;
        } else if (tok == '.') {
            next();
            if (tok != T_ID)
                error(56 /* member name expected */, 0);
            n = member(n, tokname);
            next();
        } else if (tok == T_ARROW) {
            next();
            if (tok != T_ID)
                error(56 /* member name expected */, 0);
            n = member(deref(n), tokname);
            next();
        } else if (tok == T_INC || tok == T_DEC) {
            struct Node *u;
            if (!islvalue(n))
                error(57 /* lvalue required */, 0);
            u = mknode(N_LVREF, n->type, 0, 0);
            u = cast(binop(tok == T_INC ? N_ADD : N_SUB, u, mknum(1, ty_int)), n->type);
            n = mknode(N_POSTINC, n->type, n, u);
            next();
        } else
            return n;
    }
}

struct Node *unary(void)
{
    struct Node *n;
    struct Node *u;
    struct Type *t;
    int op;
    switch (tok) {
    case '-':
        next();
        n = castexpr();
        n = decay(n);
        if (isconst(n))
            return mknum(-n->val, arith(n->type, ty_int));
        if (n->op == N_FNUM && isdblty(n->type)) {
            n->fimg[7] = n->fimg[7] ^ 128;
            return n;
        }
        if (n->op == N_FNUM) {
            n->fimg[1] = n->fimg[1] ^ 128;
            if (n->fimg[0] == 0)
                n->fimg[1] = 0;
            if (n->str) {                   /* the text too (for a double) */
                char *s;
                s = xalloc(strlen(n->str) + 2);
                if (n->str[0] == '-')
                    strcpy(s, n->str + 1);
                else {
                    s[0] = '-';
                    strcpy(s + 1, n->str);
                }
                n->str = s;
            }
            return n;
        }
        if (islongty(n->type))
            return mknode(N_CAST, n->type, call1("__lneg", n, 0), 0);
        if (!isintegral(n->type) && !isfloatty(n->type))
            error(58 /* invalid operand */, 0);
        t = isfloatty(n->type) ? n->type : arith(n->type, ty_int);
        return mknode(N_NEG, t, cast(n, t), 0);
    case '+':
        next();
        n = castexpr();
        return cast(n, isfloatty(n->type) || islongty(n->type) ? n->type : arith(n->type, ty_int));
    case '~':
        next();
        n = castexpr();
        if (!isintegral(n->type))
            error(58 /* invalid operand */, 0);
        if (islongty(n->type))
            return mknode(N_CAST, n->type, call1("__lnot", n, 0), 0);
        t = arith(n->type, ty_int);
        if (isconst(n))
            return mknum(~n->val, t);
        return mknode(N_BNOT, t, cast(n, t), 0);
    case '!':
        next();
        n = cond(castexpr());
        if (isconst(n))
            return mknum(!n->val, ty_int);
        return mknode(N_NOT, ty_int, n, 0);
    case '*':
        next();
        return deref(castexpr());
    case '&':
        next();
        n = castexpr();
        if (n->op == N_FUNC)
            return mknode(N_ADDR, ptrto(n->type), n, 0);
        if (!islvalue(n))
            error(57 /* lvalue required */, 0);
        return mknode(N_ADDR, ptrto(n->type), n, 0);
    case T_INC:
    case T_DEC:
        op = tok == T_INC ? N_ADD : N_SUB;
        next();
        n = unary();
        if (!islvalue(n))
            error(57 /* lvalue required */, 0);
        u = mknode(N_LVREF, n->type, 0, 0);
        u = cast(binop(op, u, mknum(1, ty_int)), n->type);
        return mknode(N_OPASSIGN, n->type, n, u);
    case K_SIZEOF:
        next();
        if (tok == '(' && (peek() >= K_FIRST || (peek() == T_ID && lookup(peekname()) && lookup(peekname())->kind == S_TYPEDEF))) {
            next();
            if (istypename()) {
                t = typename();
                expect(')', ")");
            } else {
                n = expr();
                expect(')', ")");
                t = n->type;
            }
        } else {
            n = unary();
            t = n->type;
        }
        if (t->size < 0)
            error(59 /* sizeof incomplete type */, 0);
        return mknum(t->size, ty_uint);
    }
    return postfix();
}

struct Node *castexpr(void)
{
    struct Type *t;
    struct Node *n;
    if (tok == '(' && (peek() >= K_FIRST || (peek() == T_ID && lookup(peekname()) && lookup(peekname())->kind == S_TYPEDEF))) {
        next();
        if (istypename()) {
            t = typename();
            expect(')', ")");
            n = castexpr();
            return cast(n, t);
        }
        n = expr();
        expect(')', ")");
        /* continue as a postfix expression */
        for (;;) {
            if (tok == '[') {
                struct Node *c;
                next();
                c = expr();
                expect(']', "]");
                n = deref(binop(N_ADD, n, c));
            } else if (tok == '.') {
                next();
                n = member(n, tokname);
                next();
            } else if (tok == T_ARROW) {
                next();
                n = member(deref(n), tokname);
                next();
            } else
                return n;
        }
    }
    return unary();
}

int binprec(int t, int *op)
{
    switch (t) {
    case '*': *op = N_MUL; return 10;
    case '/': *op = N_DIV; return 10;
    case '%': *op = N_MOD; return 10;
    case '+': *op = N_ADD; return 9;
    case '-': *op = N_SUB; return 9;
    case T_SHL: *op = N_SHL; return 8;
    case T_SHR: *op = N_SHR; return 8;
    case '<': *op = N_LT; return 7;
    case '>': *op = N_GT; return 7;
    case T_LE: *op = N_LE; return 7;
    case T_GE: *op = N_GE; return 7;
    case T_EQ: *op = N_EQ; return 6;
    case T_NE: *op = N_NE; return 6;
    case '&': *op = N_AND; return 5;
    case '^': *op = N_XOR; return 4;
    case '|': *op = N_OR; return 3;
    case T_ANDAND: *op = N_ANDAND; return 2;
    case T_OROR: *op = N_OROR; return 1;
    }
    return 0;
}

struct Node *binexpr(int minprec)
{
    struct Node *a;
    struct Node *b;
    int p;
    int op;
    a = castexpr();
    for (;;) {
        p = binprec(tok, &op);
        if (p == 0 || p < minprec)
            return a;
        next();
        b = binexpr(p + 1);
        if (op == N_ANDAND || op == N_OROR) {
            a = cond(a);
            b = cond(b);
            if (isconst(a) && isconst(b))
                a = mknum(op == N_ANDAND ? (a->val && b->val) : (a->val || b->val), ty_int);
            else
                a = mknode(op, ty_int, a, b);
        } else
            a = binop(op, a, b);
    }
}

struct Node *condexpr(void)
{
    struct Node *c;
    struct Node *a;
    struct Node *b;
    struct Node *n;
    struct Type *t;
    c = binexpr(1);
    if (tok != '?')
        return c;
    next();
    c = cond(c);
    a = decay(expr());
    expect(':', ":");
    b = decay(condexpr());
    if (isscalar(a->type) && isscalar(b->type) && a->type->kind != TY_PTR && b->type->kind != TY_PTR) {
        t = arith(a->type, b->type);
        a = cast(a, t);
        b = cast(b, t);
    } else if (a->type->kind == TY_PTR)
        t = a->type;
    else
        t = b->type;
    if (isconst(c))
        return c->val ? a : b;
    n = mknode(N_COND, t, c, a);
    n->c = b;
    return n;
}

struct Node *assign(void)
{
    struct Node *a;
    struct Node *b;
    struct Node *u;
    int op;
    a = condexpr();
    if (tok == '=') {
        next();
        b = assign();
        if (!islvalue(a) || a->type->kind == TY_ARRAY)
            error(57 /* lvalue required */, 0);
        if (a->type->kind == TY_STRUCT || a->type->kind == TY_UNION) {
            if (!sametype(a->type, b->type) || a->type != b->type)
                if (a->type != b->type)
                    error(60 /* incompatible structure assignment */, 0);
            return mknode(N_ASSIGN, a->type, a, b);
        }
        return mknode(N_ASSIGN, a->type, a, cast(b, a->type));
    }
    if (tok >= T_ADDA && tok <= T_SHRA) {
        switch (tok) {
        case T_ADDA: op = N_ADD; break;
        case T_SUBA: op = N_SUB; break;
        case T_MULA: op = N_MUL; break;
        case T_DIVA: op = N_DIV; break;
        case T_MODA: op = N_MOD; break;
        case T_ANDA: op = N_AND; break;
        case T_ORA: op = N_OR; break;
        case T_XORA: op = N_XOR; break;
        case T_SHLA: op = N_SHL; break;
        default: op = N_SHR; break;
        }
        next();
        b = assign();
        if (!islvalue(a))
            error(57 /* lvalue required */, 0);
        u = mknode(N_LVREF, a->type, 0, 0);
        u = cast(binop(op, u, b), a->type);
        return mknode(N_OPASSIGN, a->type, a, u);
    }
    return a;
}

struct Node *expr(void)
{
    struct Node *a;
    struct Node *b;
    a = assign();
    while (tok == ',') {
        next();
        b = assign();
        a = mknode(N_COMMA, b->type, a, b);
    }
    return a;
}

int constexpr(void)
{
    struct Node *n;
    int m;
    m = xmark();
    n = condexpr();
    if (!isconst(n)) {
        if (n->op == N_NUM)
            return n->val;
        error(61 /* constant expression required */, 0);
        xrelease(m);
        return 0;
    }
    xrelease(m);
    return n->val;
}

/* ---- declarations ---- */

#pragma segment REALLIT

/* printf/scanf families with a literal format: a double argument meeting
   %f %e %g (no l or L) gets the l inserted, as C prints a double with %g
   (here %g is a float: floats are not widened); printf's %lf with a float
   argument widens the argument */
void fmtfix(char *name, struct Node *args)
{
    struct Node *f;
    struct Node *a;
    struct Node **pa;
    char *s;
    char *t;
    int i;
    int k;
    int scan;
    int lng;
    int c;
    k = 0;
    scan = 0;
    if (strcmp(name, "printf") == 0)
        k = 1;
    else if (strcmp(name, "fprintf") == 0 || strcmp(name, "sprintf") == 0)
        k = 2;
    else if (strcmp(name, "scanf") == 0)
        k = scan = 1;
    else if (strcmp(name, "fscanf") == 0 || strcmp(name, "sscanf") == 0) {
        k = 2;
        scan = 1;
    }
    pa = &args;
    while (k > 1 && *pa) {
        pa = &(*pa)->next;
        k--;
    }
    if (!k || !*pa)
        return;
    f = *pa;
    pa = &f->next;
    while (f->op == N_CAST || f->op == N_ADDR)
        f = f->a;
    if (f->op != N_STR)
        return;
    s = f->str;
    for (i = 0; i < f->slen && s[i]; i++) {
        if (s[i] != '%')
            continue;
        i++;
        if (s[i] == '%')
            continue;
        lng = 0;
        while (s[i] && (strchr("-+ #0123456789.*", s[i]) || s[i] == 'h' || s[i] == 'l' || s[i] == 'L')) {
            if (s[i] == '*' && !scan && *pa)
                pa = &(*pa)->next;
            if (s[i] == '*' && scan)
                lng = -1;               /* %*f: no argument */
            else if ((s[i] == 'l' || s[i] == 'L') && lng >= 0)
                lng = 1;
            i++;
        }
        c = s[i];
        if (scan && c == '[')
            while (s[i] && s[i] != ']')
                i++;
        if (lng < 0 || !c)
            continue;
        a = *pa;
        if (!a)
            return;
        if (c == 'f' || c == 'e' || c == 'g' || c == 'E' || c == 'G') {
            if (scan ? a->type->kind == TY_PTR && isdblty(a->type->base) : isdblty(a->type)) {
                if (!lng) {             /* insert the l */
                    t = xalloc(f->slen + 1);
                    memcpy(t, s, i);
                    t[i] = 'l';
                    memcpy(t + i + 1, s + i, f->slen - i);
                    f->str = s = t;
                    f->slen++;
                    f->type->size++;
                    f->type->len++;
                    i++;
                }
            } else if (!scan && lng && isfloatty(a->type)) {
                a = cast(a, ty_double);
                a->next = (*pa)->next;
                *pa = a;
            }
        }
        pa = &a->next;
    }
}

#pragma segment PARSE
