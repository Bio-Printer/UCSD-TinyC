/* stmt.c -- the parser: statements, functions, external declarations, the
   reference scan (the pass itself and its set-up: compile.c). */
#include "tc.h"
#include "parse.h"
#pragma segment PARSE

struct Sym *label(char *name)
{
    struct Sym *s;
    for (s = labels; s; s = s->next)
        if (strcmp(s->name, name) == 0)
            return s;
    s = (struct Sym *)falloc(sizeof(struct Sym));
    s->name = falloc(strlen(name) + 1);
    strcpy(s->name, name);
    s->kind = S_LABEL;
    s->offset = ir_newlabel();
    s->next = labels;
    labels = s;
    return s;
}

void localdecl(void)
{
    struct Type *base;
    struct Type *t;
    struct Sym *s;
    struct Node *lv;
    char name[MAXNAME];
    int sc;
    int sized;
    base = declspec(&sc);
    if (tok == ';') {
        next();
        return;
    }
    for (;;) {
        t = declarator(base, name);
        if (!name[0])
            error(71 /* name expected */, 0);
        if (sc == K_TYPEDEF)
            addsym(name, S_TYPEDEF, t);
        else if (t->kind == TY_FUNC || sc == K_EXTERN) {
            s = lookup(name);
            if (!s || s->level != 0) {
                int save;
                save = level;
                level = 0;
                s = addsym(name, t->kind == TY_FUNC ? S_FUNC : S_GLOBAL, t);
                level = save;
                if (t->kind != TY_FUNC)
                    s->offset = -1;     /* by name: defined in some module */
            }
        } else if (sc == K_STATIC) {
            s = addsym(name, S_GLOBAL, t);
            s->sx = s->sx | SX_STATIC;
            if (tok == '=') {
                next();
                if (t->kind == TY_ARRAY && t->u.len < 0 && tok == T_STR) {
                    t->u.len = toklen;
                    t->size = toklen;
                }
                /* an array of unknown size: its place is known before its
                   size, which the initializer gives (as at file level) */
                sized = t->size >= 0;
                s->offset = sized ? allocglobal(t) : globoff;
                lv = mknode(N_VAR, t, 0, 0);
                lv->p.sym = s;
                ir_initbegin();
                initializer(lv, t, 1);
                ir_initend();
                if (!sized) {
                    globoff = s->offset;
                    s->offset = allocglobal(t);
                }
            } else
                s->offset = allocglobal(t);
        } else {
            if (tok == '=' && t->kind == TY_ARRAY && t->u.len < 0) {
                next();
                if (tok != T_STR)
                    error(72 /* array size required */, name);
                else {
                    t->u.len = toklen;
                    t->size = toklen;
                }
                s = addsym(name, S_LOCAL, t);
                s->offset = alloclocal(t);
                lv = mknode(N_VAR, t, 0, 0);
                lv->p.sym = s;
                initializer(lv, t, 0);
            } else {
                if (t->size < 0)
                    error(73 /* incomplete type */, name);
                s = addsym(name, S_LOCAL, t);
                s->offset = alloclocal(t);
                if (tok == '=') {
                    next();
                    lv = mknode(N_VAR, t, 0, 0);
                    lv->p.sym = s;
                    initializer(lv, t, 0);
                }
            }
        }
        if (tok != ',')
            break;
        next();
    }
    expect(';', ";");
}

void compound(int brk, int cont)
{
    int save;
    next();
    pushscope();
    save = curlocal;
    while (tok != '}' && tok != T_EOF) {
        if (istypename())
            localdecl();
        else if (tok == '{')
            compound(brk, cont);
        else
            statement(brk, cont);
    }
    expect('}', "}");
    curlocal = save;
    popscope();
}

struct Node *condparen(void)
{
    struct Node *n;
    expect('(', "(");
    n = cond(expr());
    expect(')', ")");
    return n;
}

/* The stack is what limits the parser: a statement() frame is the more
   stack the more it declares, and every nested statement has one, so the
   statements with many variables of their own (for, do, switch, case,
   goto, label) are functions of their own, and a block is parsed by
   compound() directly, not through a statement() that only calls it. */

/* for (init; cond; inc) body */
static void forstmt(void)
{
    struct Node *inc;
    int l1;
    int l2;
    int l3;
    next();
    expect('(', "(");
    pushscope();
    if (istypename())
        localdecl();
    else {
        if (tok != ';')
            ir_discard(expr());
        expect(';', ";");
    }
    l1 = ir_newlabel();
    l2 = ir_newlabel();
    l3 = ir_newlabel();
    ir_setlabel(l1);
    if (tok != ';')
        ir_branch(cond(expr()), l3, 0);
    expect(';', ";");
    inc = 0;
    if (tok != ')')
        inc = expr();
    expect(')', ")");
    if (tok == '{')
        compound(l3, l2);
    else
        statement(l3, l2);
    ir_setlabel(l2);
    if (inc)
        ir_discard(inc);
    ir_jump(l1);
    ir_setlabel(l3);
    popscope();
}

/* do body while (cond); */
static void dostmt(void)
{
    struct Node *n;
    int l1;
    int l2;
    int l3;
    next();
    l1 = ir_newlabel();
    l2 = ir_newlabel();
    l3 = ir_newlabel();
    ir_setlabel(l1);
    if (tok == '{')
        compound(l3, l2);
    else
        statement(l3, l2);
    ir_setlabel(l2);
    if (tok != K_WHILE)
        error(74 /* while expected */, 0);
    next();
    n = condparen();
    ir_branch(n, l1, 1);
    ir_setlabel(l3);
    expect(';', ";");
}

/* switch (expr) body */
static void switchstmt(int cont)
{
    struct Node *n;
    struct SwVals *sv;
    int sn;
    int sdef;
    int t;
    int l1;
    int l2;
    int m;
    m = xmark();
    next();
    expect('(', "(");
    n = expr();
    expect(')', ")");
    n = decay(n);
    if (!isintegral(n->type) || islongty(n->type)) {
        if (islongty(n->type))
            n = cast(n, ty_int);
        else
            error(75 /* integer required */, 0);
    }
    t = alloclocal(ty_int);
    ir_valuestl(cast(n, ty_int), t);
    sv = swvals;
    sn = swn;
    sdef = swdef;
    swvals = 0;
    swn = 0;
    swdef = -1;
    l1 = ir_newlabel();
    l2 = ir_newlabel();
    ir_jump(l1);
    xrelease(m);
    if (tok == '{')
        compound(l2, cont);
    else
        statement(l2, cont);
    ir_jump(l2);
    ir_setlabel(l1);
    ir_switch(t, swn, swdef >= 0 ? swdef : l2);
    ir_setlabel(l2);
    swvals = sv;
    swn = sn;
    swdef = sdef;
}

/* case constant: (the statement after it is parsed by statement()) */
static void caselabel(void)
{
    struct SwVals *c;
    int t;
    int i;
    int k;
    int l;
    next();
    t = constexpr();
    expect(':', ":");
    if (swn < 0)
        error(76 /* case outside switch */, 0);
    else {
        /* the newest chunk holds swn % SWCHUNK values (SWCHUNK when that is 0) */
        i = swn;
        for (c = swvals; c; c = c->next) {
            k = i % SWCHUNK ? i % SWCHUNK : SWCHUNK;
            i = i - k;
            while (k-- > 0)
                if (c->v[k] == t)
                    error(77 /* duplicate case */, 0);
        }
        if (swn % SWCHUNK == 0) {
            c = (struct SwVals *)falloc(sizeof(struct SwVals));
            c->next = swvals;
            swvals = c;
        }
        swvals->v[swn % SWCHUNK] = t;
        swn++;
        l = ir_newlabel();
        ir_setlabel(l);
        ir_case(t, l);
    }
}

/* goto label; */
static void gotostmt(void)
{
    struct Sym *s;
    next();
    if (tok != T_ID)
        error(82 /* label expected */, 0);
    else {
        s = label(tokname);
        ir_jump(s->offset);
        next();
    }
    expect(';', ";");
}

/* label: (the identifier and the colon are the current tokens) */
static void labeldef(void)
{
    struct Sym *s;
    s = label(tokname);
    if (s->defined)
        error(83 /* label redefined */, tokname);
    s->defined = 1;
    ir_setlabel(s->offset);
    next();
    next();
}

void statement(int brk, int cont)
{
    struct Node *n;
    int m;
    int save;
    int l1;
    int l2;
    m = xmark();
    save = curlocal;
    /* "case X:", "default:" and "label:" go round again for the statement
       after them: a run of case labels would need a stack frame per label */
    for (;;) {
    switch (tok) {
    case '{':
        compound(brk, cont);
        break;
    case ';':
        next();
        break;
    case K_IF:
        /* if ... else if ... else if ...: a loop, not a statement() per
           else if -- a long chain would need a stack frame per link */
        l2 = -1;
        for (;;) {
            next();
            n = condparen();
            l1 = ir_newlabel();
            ir_branch(n, l1, 0);
            xrelease(m);
            curlocal = save;
            if (tok == '{')
                compound(brk, cont);
            else
                statement(brk, cont);
            if (tok != K_ELSE) {
                ir_setlabel(l1);
                break;
            }
            next();
            if (l2 < 0)
                l2 = ir_newlabel();
            ir_jump(l2);
            ir_setlabel(l1);
            if (tok != K_IF) {
                if (tok == '{')
                    compound(brk, cont);
                else
                    statement(brk, cont);
                break;
            }
        }
        if (l2 >= 0)
            ir_setlabel(l2);
        break;
    case K_WHILE:
        next();
        l1 = ir_newlabel();
        l2 = ir_newlabel();
        ir_setlabel(l1);
        n = condparen();
        ir_branch(n, l2, 0);
        xrelease(m);
        curlocal = save;
        if (tok == '{')
            compound(l2, l1);
        else
            statement(l2, l1);
        ir_jump(l1);
        ir_setlabel(l2);
        break;
    case K_DO:
        dostmt();
        break;
    case K_FOR:
        forstmt();
        break;
    case K_SWITCH:
        switchstmt(cont);
        break;
    case K_CASE:
        caselabel();
        continue;
    case K_DEFAULT:
        next();
        expect(':', ":");
        if (swn < 0)
            error(78 /* default outside switch */, 0);
        else {
            swdef = ir_newlabel();
            ir_setlabel(swdef);
        }
        continue;
    case K_BREAK:
        next();
        if (brk < 0)
            error(79 /* break outside loop or switch */, 0);
        else
            ir_jump(brk);
        expect(';', ";");
        break;
    case K_CONTINUE:
        next();
        if (cont < 0)
            error(80 /* continue outside loop */, 0);
        else
            ir_jump(cont);
        expect(';', ";");
        break;
    case K_RETURN:
        next();
        n = 0;
        if (tok != ';') {
            n = expr();
            if (curft->base->kind == TY_VOID)
                error(81 /* void function returns a value */, 0);
            else if (curft->base->kind != TY_STRUCT && curft->base->kind != TY_UNION)
                n = cast(n, curft->base);
        }
        ir_return(n, curft, sretoff);
        ir_jump(exitlab);
        expect(';', ";");
        break;
    case K_GOTO:
        gotostmt();
        break;
    default:
        if (tok == T_ID && peek() == ':') {
            labeldef();
            continue;
        }
        n = expr();
        ir_discard(n);
        expect(';', ";");
        break;
    }
    break;
    }
    xrelease(m);
    curlocal = save;
}

/* ---- functions and external declarations ---- */

void funcdef(struct Sym *fs, int isstatic)
{
    struct Type *ft;
    struct Param *p;
    struct Param *pv[32];
    struct Sym *s;
    int np;
    int pw;
    int rw;
    int pad;
    int off;
    int i;
    char *seg;
    seg = cursegname;           /* a #pragma read as lookahead belongs to the next function */
    fs->sx = (fs->sx & SX_STATIC) | cursegi;    /* calls from the same segment can be CGP */
    curfnseg = cursegi;
    ft = fs->type;
    if (fs->defined)
        error(84 /* function redefined */, fs->name);
    fs->defined = 1;
    curfn = fs;
    curft = ft;
    labels = 0;
    pushscope();
    np = 0;
    pw = 0;
    for (p = ft->u.params; p; p = p->next) {
        if (np >= 32)
            fatal(85 /* too many parameters */, fs->name);
        pv[np++] = p;
        pw = pw + twords(p->type);
    }
    if (ft->flags & TF_VARIADIC)
        pw++;
    sretoff = 0;
    if (ft->base->kind == TY_STRUCT || ft->base->kind == TY_UNION)
        pw++;
    rw = retwords(ft);
    pad = rw > pw ? rw - pw : 0;
    off = pad + 1;
    vaoff = 0;
    if (ft->flags & TF_VARIADIC) {
        vaoff = off;
        off++;
    }
    for (i = np - 1; i >= 0; i--) {
        p = pv[i];
        if (i < npnames && pnames[i]) {
            s = addsym(pnames[i], S_LOCAL, p->type);
            s->offset = off;
        } else if (!(ft->flags & TF_OLDSTYLE))
            error(86 /* parameter name missing */, fs->name);
        off = off + twords(p->type);
    }
    if (ft->base->kind == TY_STRUCT || ft->base->kind == TY_UNION) {
        sretoff = off;
        off++;
    }
    nparamwords = off - 1;
    curlocal = nparamwords;
    scratch = ++curlocal;
    maxlocal = curlocal;
    ir_funcbegin();
    exitlab = ir_newlabel();
    compound(-1, -1);
    for (s = labels; s; s = s->next)
        if (!s->defined)
            error(87 /* undefined label */, s->name);
    ir_funcend(fs, ft, exitlab, isstatic, seg);
    curfnseg = 0;               /* file-scope initialisers run in segment INIT */
    popscope();
    curfn = 0;
    curft = 0;
    freset();
}

void external(void)
{
    int sysdecl;
    struct Type *base;
    struct Type *t;
    struct Sym *s;
    struct Node *lv;
    char name[MAXNAME];
    int sc;
    int m;
    m = xmark();
    base = declspec(&sc);
    if (tok == ';') {
        next();
        return;
    }
    for (;;) {
        sysdecl = insys && sc != K_TYPEDEF;
        tentative = sysdecl;
        t = declarator(base, name);
        tentative = 0;
        if (!name[0]) {
            error(71 /* name expected */, 0);
            next();
            return;
        }
        if (sysdecl) {
            if ((sc == K_EXTERN || (t->kind == TY_FUNC && tok != '{')) && !isref(name)) {
                if (tok != ',')
                    break;              /* not used by the program: not kept */
                next();
                continue;
            }
            t = permtype(t);
        }
        if (sc == K_TYPEDEF) {
            addsym(name, S_TYPEDEF, t);
        } else if (t->kind == TY_FUNC) {
            s = lookup(name);
            if (s && s->kind != S_FUNC) {
                error(88 /* redeclared */, name);
                s = 0;
            }
            if (!s)
                s = addsym(name, S_FUNC, t);
            else if (!s->defined && t->u.params)
                s->type = t;
            if (!symseg(s) && segexplicit)
                s->sx = s->sx | cursegi;    /* declared under #pragma segment: the
                                               linker checks it (message 116) */
            if (sc == K_STATIC)
                s->sx = s->sx | SX_STATIC;  /* link name MODULE'name (irlname) */
            if (tok == '{') {
                if (t != s->type)
                    s->type = t;
                funcdef(s, sc == K_STATIC);
                xrelease(m);
                return;
            }
        } else {
            s = lookup(name);
            if (s && (s->kind != S_GLOBAL || s->level != 0)) {
                error(88 /* redeclared */, name);
                s = 0;
            }
            if (!s) {
                s = addsym(name, S_GLOBAL, t);
                s->offset = -1;
                if (sc == K_STATIC) {
                    s->sx = s->sx | SX_STATIC;
                    if (t->size >= 0)
                        s->offset = allocglobal(t);
                }
            } else if (s->type->kind == TY_ARRAY && s->type->u.len < 0 && t->u.len >= 0)
                s->type = t;
            if (sc != K_EXTERN && !s->defined)
                s->defined = 1;             /* a tentative (common) definition */
            if (tok == '=') {
                next();
                if (t->kind == TY_ARRAY && t->u.len < 0 && tok == T_STR) {
                    t->u.len = toklen;
                    t->size = toklen;
                }
                if (s->defined == 2)
                    error(88 /* redeclared */, name);
                s->defined = 2;
                s->type = t;
                lv = mknode(N_VAR, t, 0, 0);
                lv->p.sym = s;
                if ((s->sx & SX_STATIC) && s->offset < 0) {
                    /* static array of unknown size: allocate after the initializer */
                    s->offset = globoff;
                    ir_initbegin();
                    initializer(lv, t, 1);
                    ir_initend();
                    globoff = s->offset;
                    s->offset = allocglobal(t);
                } else {
                    ir_initbegin();
                    initializer(lv, t, 1);
                    ir_initend();
                }
            } else if ((s->sx & SX_STATIC) && s->offset < 0 && t->size >= 0)
                s->offset = allocglobal(t);
        }
        if (tok != ',')
            break;
        next();
    }
    expect(';', ";");
    xrelease(m);
}

#pragma segment CINIT

/* Collect every identifier the program itself uses: everything in the
   main file, and whatever is inside braces (function bodies, structures,
   initialisers) in included files.  Top-level declarations in included
   files of names never mentioned are then not kept at all. */
void addref(char *name)
{
    int a;
    int b;
    a = (hashstr(name) * 2 + 1) & 4095;
    b = refhash2(name);
    refbits[a >> 3] = refbits[a >> 3] | (1 << (a & 7));
    refbits[b >> 3] = refbits[b >> 3] | (1 << (b & 7));
}

void scanrefs(char *src)
{
    FILE *fp;
    int c;
    int q;
    int n;
    int sys;
    int bol;
    int depth;
    char name[MAXNAME];
    fp = fopen(src, "r");
    if (!fp)
        fatal(25 /* cannot open */, src);
    refbits = (unsigned char *)palloc(RBITS / 8);
    sys = 0;
    bol = 1;
    depth = 0;
    c = getc(fp);
    while (c != EOF) {
        if (bol && c == '#') {
            /* "#<line> [!]file": '!' = system header (not "#pragma ...") */
            c = getc(fp);
            if (c >= '0' && c <= '9') {
                while (c != EOF && c != ' ' && c != '\n')
                    c = getc(fp);
                if (c == ' ') {
                    c = getc(fp);
                    sys = c == '!';
                }
            }
            while (c != EOF && c != '\n')
                c = getc(fp);
            continue;
        }
        if (c == '\n') {
            bol = 1;
            c = getc(fp);
            continue;
        }
        bol = 0;
        if (c == '{')
            depth++;
        else if (c == '}')
            depth--;
        if (c == '"' || c == '\'') {
            q = c;
            c = getc(fp);
            while (c != EOF && c != q && c != '\n') {
                if (c == '\\')
                    c = getc(fp);
                c = getc(fp);
            }
            c = getc(fp);
            continue;
        }
        if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_') {
            n = 0;
            while ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_' || (c >= '0' && c <= '9')) {
                if (n < MAXNAME - 1)
                    name[n++] = c;
                c = getc(fp);
            }
            name[n] = 0;
            if (!sys || depth > 0)
                addref(name);
            continue;
        }
        if (c >= '0' && c <= '9') {
            while ((c >= '0' && c <= '9') || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '.' || c == '_')
                c = getc(fp);
            continue;
        }
        c = getc(fp);
    }
    fclose(fp);
}
