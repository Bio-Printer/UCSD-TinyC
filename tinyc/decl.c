/* decl.c -- the parser: declarations. */
#include "tc.h"
#include "parse.h"
#pragma segment PARSE

int isnested(void)
{
    int p;
    struct Sym *s;
    p = peek();
    if (p == '*' || p == '(')
        return 1;
    if (p == T_ID) {
        s = lookup(peekname());
        return !(s && s->kind == S_TYPEDEF);
    }
    return 0;
}

void dcl(struct Dcl *d)
{
    int nstars;
    int i;
    int variadic;
    int oldstyle;
    struct Type *ft;
    struct Param *pl;
    nstars = 0;
    while (tok == '*' || tok == K_CONST || tok == K_VOLATILE) {
        if (tok == '*')
            nstars++;
        next();
    }
    if (tok == '(' && isnested()) {
        next();
        dcl(d);
        expect(')', ")");
    } else if (tok == T_ID) {
        strcpy(d->name, tokname);
        next();
    }
    for (;;) {
        if (tok == '[') {
            next();
            if (d->n >= 12)
                fatal(62 /* declarator too complex */, 0);
            d->kind[d->n] = TY_ARRAY;
            d->len[d->n] = -1;
            if (tok != ']')
                d->len[d->n] = constexpr();
            expect(']', "]");
            d->n++;
        } else if (tok == '(') {
            next();
            pl = paramlist(&variadic, &oldstyle);
            ft = (struct Type *)xalloc(sizeof(struct Type));   /* see functype */
            ft->kind = TY_FUNC;
            ft->size = 2;
            ft->flags = TF_TEMP | (variadic ? TF_VARIADIC : 0) | (oldstyle ? TF_OLDSTYLE : 0);
            ft->u.params = pl;
            if (d->n >= 12)
                fatal(62 /* declarator too complex */, 0);
            d->kind[d->n] = TY_FUNC;
            d->ft[d->n] = ft;
            d->n++;
        } else
            break;
    }
    for (i = 0; i < nstars; i++) {
        if (d->n >= 12)
            fatal(62 /* declarator too complex */, 0);
        d->kind[d->n++] = TY_PTR;
    }
}

struct Type *applydcl(struct Type *t, struct Dcl *d)
{
    int i;
    struct Type *f;
    for (i = d->n - 1; i >= 0; i--) {
        if (d->kind[i] == TY_PTR)
            t = ptrto(t);
        else if (d->kind[i] == TY_ARRAY) {
            if (t->kind == TY_FUNC)
                error(63 /* array of functions */, 0);
            t = arrayof(t, d->len[i]);
        } else {
            f = d->ft[i];
            if (t->kind == TY_FUNC || t->kind == TY_ARRAY)
                error(64 /* function returning an array or function */, 0);
            f->base = t;
            t = tentative ? f : functype(f);    /* (permtype does it later) */
        }
    }
    return t;
}

/* The parameter names of a definition: the first parameter list at the
   outermost level of the declarator (not those of function-pointer
   parameters).  Only the function body that follows uses them. */
char **pnames;                  /* [32], allocated by compile */
int npnames;
static int pdepth;              /* paramlist nesting */
static int pnamed;              /* this declarator's names are recorded */

struct Type *declarator(struct Type *base, char *name)
{
    struct Dcl d;
    if (pdepth == 0)
        pnamed = 0;
    d.n = 0;
    d.name[0] = 0;
    dcl(&d);
    strcpy(name, d.name);
    return applydcl(base, &d);
}

struct Type *typename(void)
{
    struct Type *t;
    char name[MAXNAME];
    int sc;
    t = declspec(&sc);
    return declarator(t, name);
}

struct Param *paramlist(int *variadic, int *oldstyle)
{
    int record;
    struct Param *first;
    struct Param *last;
    struct Param *p;
    struct Type *t;
    char name[MAXNAME];
    int sc;
    *variadic = 0;
    *oldstyle = 0;
    first = 0;
    last = 0;
    record = pdepth == 0 && !pnamed;
    if (record) {
        pnamed = 1;
        npnames = 0;
    }
    if (tok == ')') {
        next();
        *oldstyle = 1;
        return 0;
    }
    if (tok == K_VOID && peek() == ')') {
        next();
        next();
        return 0;
    }
    for (;;) {
        if (tok == T_ELLIPSIS) {
            next();
            *variadic = 1;
            break;
        }
        t = declspec(&sc);
        pdepth++;
        t = declarator(t, name);
        pdepth--;
        if (t->kind == TY_ARRAY)
            t = ptrto(t->base);
        else if (t->kind == TY_FUNC)
            t = ptrto(t);
        p = (struct Param *)xalloc(sizeof(struct Param));   /* see functype */
        if (record && npnames < 32) {
            pnames[npnames] = 0;
            if (name[0]) {
                pnames[npnames] = xalloc(strlen(name) + 1);
                strcpy(pnames[npnames], name);
            }
            npnames++;
        }
        p->type = t;
        if (last)
            last->next = p;
        else
            first = p;
        last = p;
        if (tok != ',')
            break;
        next();
    }
    expect(')', ")");
    return first;
}

struct Type *structspec(int isunion)
{
    struct Sym *s;
    struct Type *t;
    struct Type *ft;
    struct Type *base;
    struct Field *f;
    struct Field *last;
    char tag[MAXNAME];
    char name[MAXNAME];
    int off;
    int size;
    int sc;
    next();
    tag[0] = 0;
    if (tok == T_ID) {
        strcpy(tag, tokname);
        next();
    }
    t = 0;
    if (tok != '{') {
        if (!tag[0]) {
            error(65 /* structure tag expected */, 0);
            return ty_int;
        }
        s = lookuptag(tag);
        if (s)
            return s->type;
        t = mktype(isunion ? TY_UNION : TY_STRUCT, -1, 2);
        t->v.tag = pstrdup(tag);
        s = addsym(tag, S_TAG, t);
        return t;
    }
    if (tag[0]) {
        s = lookuptag(tag);
        if (s && s->level == level && s->type->size < 0)
            t = s->type;
        else if (s && s->level == level)
            error(66 /* structure redefined */, tag);
    }
    if (!t) {
        t = mktype(isunion ? TY_UNION : TY_STRUCT, -1, 2);
        if (tag[0]) {
            t->v.tag = pstrdup(tag);
            addsym(tag, S_TAG, t);
        }
    }
    next();
    off = 0;
    size = 0;
    last = 0;
    while (tok != '}' && tok != T_EOF) {
        base = declspec(&sc);
        if (tok == ';') {               /* anonymous struct/union member */
            f = (struct Field *)palloc(sizeof(struct Field));
            f->type = base;
            if (!isunion) {
                off = (off + 1) & ~1;
                f->offset = off;
                off = off + base->size;
            }
            if (last)
                last->next = f;
            else
                t->u.fields = f;
            last = f;
            next();
            continue;
        }
        for (;;) {
            ft = declarator(base, name);
            if (ft->size < 0 || ft->kind == TY_FUNC)
                error(67 /* invalid member type */, name);
            f = (struct Field *)palloc(sizeof(struct Field));
            f->name = pstrdup(name);
            f->type = ft;
            if (isunion) {
                f->offset = 0;
                if (ft->size > size)
                    size = ft->size;
            } else {
                if (talign(ft) > 1)
                    off = (off + 1) & ~1;
                f->offset = off;
                off = off + ft->size;
            }
            if (last)
                last->next = f;
            else
                t->u.fields = f;
            last = f;
            if (tok != ',')
                break;
            next();
        }
        expect(';', ";");
    }
    expect('}', "}");
    if (!isunion)
        size = off;
    t->size = (size + 1) & ~1;
    if (t->size == 0)
        t->size = 2;
    return t;
}

struct Type *enumspec(void)
{
    struct Sym *s;
    int v;
    next();
    if (tok == T_ID) {
        s = lookuptag(tokname);
        if (!s)
            addsym(tokname, S_TAG, ty_int);
        next();
    }
    if (tok != '{')
        return ty_int;
    next();
    v = 0;
    while (tok == T_ID) {
        s = addsym(tokname, S_ENUMC, ty_int);
        next();
        if (tok == '=') {
            next();
            v = constexpr();
        }
        s->offset = v;
        v = W16(v + 1);
        if (tok != ',')
            break;
        next();
    }
    expect('}', "}");
    return ty_int;
}

struct Type *declspec(int *sclass)
{
    int nlong;
    int nshort;
    int uns;
    int sgn;
    int base;
    struct Type *t;
    struct Sym *s;
    nlong = 0;
    nshort = 0;
    uns = 0;
    sgn = 0;
    base = 0;
    t = 0;
    *sclass = 0;
    for (;;) {
        switch (tok) {
        case K_TYPEDEF: case K_EXTERN: case K_STATIC:
            *sclass = tok;
            next();
            continue;
        case K_AUTO: case K_REGISTER: case K_CONST: case K_VOLATILE:
            next();
            continue;
        case K_VOID: case K_CHAR: case K_INT: case K_FLOAT: case K_DOUBLE:
            base = tok;
            next();
            continue;
        case K_LONG:
            nlong++;
            next();
            continue;
        case K_SHORT:
            nshort++;
            next();
            continue;
        case K_UNSIGNED:
            uns = 1;
            next();
            continue;
        case K_SIGNED:
            sgn = 1;
            next();
            continue;
        case K_STRUCT:
        case K_UNION:
            t = structspec(tok == K_UNION);
            continue;
        case K_ENUM:
            t = enumspec();
            continue;
        case T_ID:
            if (!t && !base && !nlong && !nshort && !uns && !sgn) {
                s = lookup(tokname);
                if (s && s->kind == S_TYPEDEF) {
                    t = s->type;
                    next();
                    continue;
                }
            }
            break;
        }
        break;
    }
    if (t)
        return t;
    switch (base) {
    case K_VOID: return ty_void;
    case K_CHAR: return uns ? ty_uchar : ty_char;
    case K_FLOAT:
        if (!insys)
            usesfloat = 1;
        return ty_float;
    case K_DOUBLE:
        if (!insys)
            usesfloat = 1;
        return nlong ? ty_ldouble : ty_double;
    }
    if (nlong)
        return uns ? ty_ulong : ty_long;
    return uns ? ty_uint : ty_int;
}

/* ---- initializers ---- */

struct Node *elem(struct Node *lv, int off, struct Type *t)
{
    struct Node *m;
    m = mknode(N_MEMBER, t, lv, 0);
    m->val = off;
    return m;
}

void initializer(struct Node *lv, struct Type *t, int global)
{
    struct Node *n;
    struct Field *f;
    int i;
    int m;
    int brace;
    int save;
    save = globinit;
    globinit = global;
    init1(lv, t, global);
    globinit = save;
}

void init1(struct Node *lv, struct Type *t, int global)
{
    struct Node *n;
    struct Field *f;
    int i;
    int m;
    int brace;
    if (t->kind == TY_ARRAY) {
        if (tok == T_STR && (t->base->kind == TY_CHAR || t->base->kind == TY_UCHAR)) {
            m = xmark();
            n = primary();
            if (t->u.len < 0) {
                t->u.len = n->slen;
                t->size = n->slen;
            } else if (n->slen - 1 > t->u.len)
                error(68 /* initializer string too long */, 0);
            ir_discard(mknode(N_ASSIGN, t, lv, n));
            xrelease(m);
            return;
        }
        if (tok != '{') {
            error(69 /* { expected */, 0);
            return;
        }
        next();
        i = 0;
        while (tok != '}' && tok != T_EOF) {
            if (t->u.len >= 0 && i >= t->u.len)
                error(70 /* too many initializers */, 0);
            init1(elem(lv, W16(i * t->base->size), t->base), t->base, global);
            i++;
            if (tok != ',')
                break;
            next();
        }
        expect('}', "}");
        if (t->u.len < 0) {
            t->u.len = i;
            t->size = W16(i * t->base->size);
        }
        return;
    }
    if (t->kind == TY_STRUCT || t->kind == TY_UNION) {
        if (tok != '{') {
            m = xmark();
            n = assign();
            ir_discard(mknode(N_ASSIGN, t, lv, n));
            xrelease(m);
            return;
        }
        next();
        f = t->u.fields;
        while (tok != '}' && tok != T_EOF) {
            if (!f) {
                error(70 /* too many initializers */, 0);
                break;
            }
            init1(elem(lv, f->offset, f->type), f->type, global);
            f = t->kind == TY_UNION ? 0 : f->next;
            if (tok != ',')
                break;
            next();
        }
        expect('}', "}");
        return;
    }
    brace = 0;
    if (tok == '{') {
        brace = 1;
        next();
    }
    m = xmark();
    n = assign();
    ir_discard(mknode(N_ASSIGN, t, lv, cast(n, t)));
    xrelease(m);
    if (brace)
        expect('}', "}");
}

/* ---- statements ---- */
