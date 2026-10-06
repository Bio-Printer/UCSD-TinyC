/* genx.c -- P-code generation for expressions, assignments, calls,
 * conditions, return and switch: the second half of gen.c (split so that
 * each compiles in less memory on the P-System); same segment, GEN.
 */
#include "tc.h"
#include "gen.h"
#pragma segment GEN

#define LV_LOCAL 1
#define LV_GLOBAL 2
#define LV_PTR 3

struct LV {
    int kind;
    int off;                    /* word offset of the variable */
    int boff;                   /* extra byte offset */
    struct Node *ptr;           /* LV_PTR: address expression */
    char *name;                 /* LV_GLOBAL: link name, 0 for a module static */
};

/* global variable access: operand is always two bytes so the linker can
   fill it in -- R_GSTAT: module static (operand = offset in the module),
   R_GNAME: named variable (operand = word offset within it) */
static void gglob(int op, struct LV *lv, int add)
{
    int v;
    ob(op);
    if (lv->name) {
        reloc(R_GNAME, lv->name);
        v = add;
    } else {
        reloc(R_GSTAT, "");
        v = lv->off + add;
    }
    ob(128 | ((v >> 8) & 127));
    ob(v & 255);
}

static void lvinfo(struct Node *n, struct LV *lv)
{
    struct Node *p;
    lv->name = 0;
    if (n->op == N_VAR) {
        lv->kind = n->sym->kind == S_LOCAL ? LV_LOCAL : LV_GLOBAL;
        lv->off = n->sym->offset;
        lv->boff = 0;
        lv->ptr = 0;
        lv->name = lv->kind == LV_GLOBAL && lv->off < 0 ? n->sym->name : 0;
        return;
    }
    if (n->op == N_MEMBER) {
        lvinfo(n->a, lv);
        lv->boff = W16(lv->boff + n->val);
        return;
    }
    if (n->op == N_DEREF) {
        p = n->a;
        lv->kind = LV_PTR;
        lv->off = 0;
        lv->boff = 0;
        lv->ptr = p;
        if (p->op == N_ADD && p->b->op == N_NUM && p->type->kind == TY_PTR) {
            lv->ptr = p->a;
            lv->boff = p->b->val;
        }
        return;
    }
    if (n->op == N_CALL || n->op == N_ASSIGN || n->op == N_COND || n->op == N_COMMA) {
        /* structure-valued: the value is its address */
        lv->kind = LV_PTR;
        lv->off = 0;
        lv->boff = 0;
        lv->ptr = n;
        return;
    }
    error(57 /* lvalue required */, 0);
    lv->kind = LV_LOCAL;
    lv->off = scratch;
    lv->boff = 0;
    lv->ptr = 0;
}

static void gen_ptrvalue(struct Node *p)
{
    if (p->type->kind == TY_STRUCT || p->type->kind == TY_UNION)
        gen_value(p);           /* a structure-valued call etc.: its address */
    else
        gen_value(p);
}

/* push the address of lvalue n */
static void gen_addr(struct Node *n)
{
    struct LV lv;
    if (n->op == N_STR || n->op == N_FUNC) {
        gen_value(n);
        return;
    }
    lvinfo(n, &lv);
    if (lv.kind == LV_LOCAL) {
        lla(lv.off + (lv.boff >> 1));
        addconst(lv.boff & 1);
    } else if (lv.kind == LV_GLOBAL) {
        gglob(O_LAO, &lv, lv.boff >> 1);
        addconst(lv.boff & 1);
    } else {
        gen_ptrvalue(lv.ptr);
        addconst(lv.boff);
    }
}

/* push base and byte index for LDB/STB */
static void gen_byteaddr(struct Node *n)
{
    struct LV lv;
    struct Node *p;
    lvinfo(n, &lv);
    if (lv.kind == LV_LOCAL) {
        lla(lv.off + (lv.boff >> 1));
        ldc(lv.boff & 1);
    } else if (lv.kind == LV_GLOBAL) {
        gglob(O_LAO, &lv, lv.boff >> 1);
        ldc(lv.boff & 1);
    } else {
        p = lv.ptr;
        if (lv.boff == 0 && p->op == N_ADD && p->type->kind == TY_PTR && ischar(p->type->base)) {
            gen_value(p->a);
            gen_value(p->b);
        } else {
            gen_ptrvalue(p);
            ldc(lv.boff);
        }
    }
}

static void signext(void)
{
    reloc(R_CALL, "__sx");
    ob(O_CXP);
    ob(0);
    ob(0);
}

static void loadchar(struct Type *t)
{
    ob(O_LDB);
    if (t->kind == TY_CHAR)
        signext();
}

static int islv(struct Node *n)
{
    return n->op == N_VAR || n->op == N_MEMBER || n->op == N_DEREF;
}

/* a value of which only the low byte matters: char loads skip the sign
   extension, and conversions to char are unnecessary */
static void gen_lowbyte(struct Node *n)
{
    while (n->op == N_CAST && isword(n->type) && isword(n->a->type))
        n = n->a;
    if (islv(n) && ischar(n->type)) {
        gen_byteaddr(n);
        ob(O_LDB);
    } else
        gen_value(n);
}

/* load the value of lvalue n */
static void gen_load(struct Node *n)
{
    struct LV lv;
    struct Type *t;
    int w;
    t = n->type;
    if (t->kind == TY_ARRAY || t->kind == TY_STRUCT || t->kind == TY_UNION) {
        gen_addr(n);
        return;
    }
    if (ischar(t)) {
        gen_byteaddr(n);
        loadchar(t);
        return;
    }
    lvinfo(n, &lv);
    w = twords(t);
    if (w == 1) {
        if (lv.kind == LV_LOCAL && !(lv.boff & 1))
            ldl(lv.off + lv.boff / 2);
        else if (lv.kind == LV_GLOBAL && !(lv.boff & 1))
            gglob(O_LDO, &lv, lv.boff / 2);
        else if (lv.kind == LV_PTR && !(lv.boff & 1) && lv.boff >= 0) {
            gen_ptrvalue(lv.ptr);
            ind(lv.boff / 2);
        } else {
            gen_addr(n);
            ind(0);
        }
        return;
    }
    gen_addr(n);
    ob(O_LDM);
    ob(w);
}

/* store: the address part (pushed before the value) */
static void storepre(struct Node *n)
{
    struct LV lv;
    struct Type *t;
    t = n->type;
    if (ischar(t)) {
        gen_byteaddr(n);
        return;
    }
    lvinfo(n, &lv);
    if (twords(t) == 1 && (lv.kind == LV_LOCAL || lv.kind == LV_GLOBAL) && !(lv.boff & 1))
        return;
    gen_addr(n);
}

/* store: after the value was pushed */
static void storepost(struct Node *n)
{
    struct LV lv;
    struct Type *t;
    t = n->type;
    if (ischar(t)) {
        ob(O_STB);
        return;
    }
    lvinfo(n, &lv);
    if (twords(t) == 1) {
        if (lv.kind == LV_LOCAL && !(lv.boff & 1))
            gen_stl(lv.off + lv.boff / 2);
        else if (lv.kind == LV_GLOBAL && !(lv.boff & 1))
            gglob(O_SRO, &lv, lv.boff / 2);
        else
            ob(O_STO);
        return;
    }
    ob(O_STM);
    ob(twords(t));
}

static int simplelv(struct Node *n)
{
    struct LV lv;
    lvinfo(n, &lv);
    return lv.kind != LV_PTR;
}

/* via a temp holding the address */
static void loadvia(int ta, struct Type *t)
{
    ldl(ta);
    if (ischar(t)) {
        ldc(0);
        loadchar(t);
    } else if (twords(t) == 1)
        ind(0);
    else {
        ob(O_LDM);
        ob(twords(t));
    }
}

static void storeviapre(int ta, struct Type *t)
{
    ldl(ta);
    if (ischar(t))
        ldc(0);
}

static void storeviapost(struct Type *t)
{
    if (ischar(t))
        ob(O_STB);
    else if (twords(t) == 1)
        ob(O_STO);
    else {
        ob(O_STM);
        ob(twords(t));
    }
}

/* ---- conversions ---- */

static void gen_dcast(struct Node *n);

static void gen_cast(struct Node *n)
{
    struct Type *t;
    int fk;
    t = n->type;
    fk = n->a->type->kind;
    if (t->kind == TY_VOID) {
        gen_discard(n->a);
        return;
    }
    if (isdblty(t) || isdblty(n->a->type)) {
        gen_dcast(n);
        return;
    }
    gen_value(n->a);
    if (isfloatty(t) && !isfloatty(n->a->type)) {
        if (!islongty(n->a->type))
            ob(O_FLT);
        return;
    }
    if (!isfloatty(t) && isfloatty(n->a->type)) {
        csp(CSP_TNC);
        fk = TY_INT;
    }
    if (t->kind == TY_UCHAR && fk != TY_UCHAR) {
        ldc(255);
        ob(O_LAND);
    } else if (t->kind == TY_CHAR && fk != TY_CHAR)
        signext();
}

/* ---- calls ---- */

static void gen_call(struct Node *n, int want)
{
    struct Type *ft;
    struct Node *a;
    struct Param *p;
    struct Sym *fs;
    int pw;
    int rw;
    int sret;
    int blk;
    int bw;
    int pos;
    int t;
    int L;
    int A;
    ft = n->a->type;
    if (ft->kind == TY_PTR)
        ft = ft->base;
    fs = n->a->op == N_FUNC ? n->a->sym : 0;
    rw = retwords(ft);
    sret = 0;
    pw = 0;
    if (ft->base->kind == TY_STRUCT || ft->base->kind == TY_UNION) {
        sret = newtemp((ft->base->size + 1) / 2);
        lla(sret);
        pw++;
    }
    /* variadic extras go to a block in the caller's frame */
    blk = 0;
    if ((ft->flags & TF_VARIADIC)) {
        p = ft->u.params;
        a = n->b;
        while (p && a) {
            p = p->next;
            a = a->next;
        }
        bw = 0;
        for (pos = 0; a; a = a->next)
            bw = bw + (a->type->kind == TY_STRUCT || a->type->kind == TY_UNION ? (a->type->size + 1) / 2 : twords(a->type));
        blk = newtemp(bw > 0 ? bw : 1);
        p = ft->u.params;
        a = n->b;
        while (p && a) {
            p = p->next;
            a = a->next;
        }
        pos = blk;
        for (; a; a = a->next) {
            if (a->type->kind == TY_STRUCT || a->type->kind == TY_UNION) {
                lla(pos);
                gen_value(a);
                opbig(O_MOV, (a->type->size + 1) / 2);
                pos = pos + (a->type->size + 1) / 2;
            } else if (twords(a->type) == 1) {
                gen_value(a);
                gen_stl(pos);
                pos++;
            } else {
                lla(pos);
                gen_value(a);
                ob(O_STM);
                ob(twords(a->type));
                pos = pos + twords(a->type);
            }
        }
    }
    p = ft->u.params;
    for (a = n->b; a; a = a->next) {
        if ((ft->flags & TF_VARIADIC) && !p)
            break;
        if (a->type->kind == TY_STRUCT || a->type->kind == TY_UNION) {
            gen_value(a);
            ob(O_LDM);
            ob((a->type->size + 1) / 2);
            pw = pw + (a->type->size + 1) / 2;
        } else {
            gen_value(a);
            pw = pw + twords(a->type);
        }
        if (p)
            p = p->next;
    }
    if ((ft->flags & TF_VARIADIC)) {
        lla(blk);
        pw++;
    }
    while (pw < rw) {
        ldc(0);
        pw++;
    }
    if (fs && n->a->val) {
        reloc(R_NEAR, fs->name);        /* the parser knows it is in this segment */
        ob(O_CGP);
        ob(0);
    } else if (fs) {
        reloc(R_CALL, fs->name);
        ob(O_CXP);
        ob(0);
        ob(0);
    } else if (z80calls) {
        /* indirect call for the Z80 interpreter (-z): patch the operands of the
           CXP that follows with the function value, at run time */
        gen_value(n->a);
        t = newtemp(1);
        gen_stl(t);
        ob(O_LPA);
        ob(0);
        A = E->pc;
        L = t <= 16 ? 1 : (t < 128 ? 2 : 3);
        ldc(4 + L);
        ob(O_ADI);
        ldl(t);
        ob(O_STO);
        if (E->pc != A + 3 + L)
            fatal(98 /* internal: indirect call layout */, 0);
        ob(O_CXP);
        ob(0);
        ob(0);
    } else {
        /* indirect call: the arguments are already on the stack; the function
           value (seg | proc << 8, see R_FNPTR) goes on top and CALLI does what
           CXP seg,proc would, returning to the instruction after the CSP.
           Nothing is stored into the code stream. */
        gen_value(n->a);
        csp(CSP_CALLI);
    }
    if (!want)
        drop(rw);
}

/* ---- intrinsics ---- */

static void gen_intrinsic(struct Node *n, int want)
{
    struct Node *a;
    int code;
    int num;
    code = n->val;
    a = n->a;
    if (code == 3) {            /* __va_start */
        ldl(n->val2);
        if (!want)
            drop(1);
        return;
    }
    if (code == 10) {           /* __exitprog */
        ldc(1);
        ldc(1);
        csp(CSP_EXIT);
        return;
    }
    if (code == 9 || code == 11) {  /* __osvar(n): OS global word n */
        ob(code == 9 ? O_LOD : O_LDA);
        ob(2);
        big(a->val);
        if (!want)
            drop(1);
        return;
    }
    num = 0;
    if (code >= 4) {
        num = a->val;
        a = a->next;
    }
    for (; a; a = a->next)
        gen_value(a);
    switch (code) {
    case 1: ob(O_DVI); break;
    case 2: ob(O_MODI); break;
    case 4: case 5: case 6: case 12: csp(num); break;
    case 7: case 8:
        ob(O_CXP);
        ob(0);
        ob(num);
        break;
    }
    if (!want)
        drop(valwords(n->type));
}

/* ---- expressions ---- */

static int isfconst(struct Node *n)
{
    return n->op == N_FNUM;
}

static void gen_fconst(unsigned char *f)
{
    ob(O_LDC);
    ob(2);
    if ((E->pc & 1) == 1)
        ob(0);
    ob(f[2]);
    ob(f[3]);
    ob(f[0]);
    ob(f[1]);
}

/* a double: LDC 4, the words last first (as for a REAL) */
static void gen_dconst(unsigned char *f)
{
    int w;
    ob(O_LDC);
    ob(4);
    if ((E->pc & 1) == 1)
        ob(0);
    for (w = 3; w >= 0; w--) {
        ob(f[2 * w]);
        ob(f[2 * w + 1]);
    }
}

/* doubles: CSP 100.. (the engine's NativeDouble.inc) */
#define CSP_DADD 100
#define CSP_DNEG 104
#define CSP_DCMP 105

/* DCMP's relation codes: 0 == 1 != 2 < 3 <= 4 > 5 >=, + 8 negates */
static int dblrel(int op)
{
    switch (op) {
    case N_EQ: return 0;
    case N_NE: return 1;
    case N_LT: return 2;
    case N_LE: return 3;
    case N_GT: return 4;
    }
    return 5;
}

/* conversions to and from double: CSP 106..111, 136, 137 */
static void gen_dcast(struct Node *n)
{
    struct Type *t;
    struct Type *f;
    int a;
    int b;
    t = n->type;
    f = n->a->type;
    if (isdblty(t) && isdblty(f)) {
        gen_value(n->a);
        return;
    }
    if (isdblty(t)) {
        if (isunsignedty(f) && !islongty(f))
            ldc(0);                     /* unsigned: as a long, high word 0 */
        gen_value(n->a);
        if (isfloatty(f))
            csp(106);                   /* FTOD */
        else if (islongty(f) && isunsignedty(f))
            csp(136);                   /* ULTOD */
        else if (islongty(f) || isunsignedty(f))
            csp(110);                   /* LTOD */
        else
            csp(108);                   /* ITOD */
        return;
    }
    gen_value(n->a);
    if (isfloatty(t)) {
        csp(107);                       /* DTOF */
        return;
    }
    if (islongty(t)) {
        csp(isunsignedty(t) ? 137 : 111);   /* DTOUL, DTOL */
        return;
    }
    if (isunsignedty(t) && t->kind != TY_UCHAR) {
        csp(111);                       /* DTOL, keep the low word */
        a = newtemp(1);
        b = newtemp(1);
        gen_stl(a);
        gen_stl(b);
        ldl(a);
        return;
    }
    csp(109);                           /* DTOI */
    if (t->kind == TY_UCHAR) {
        ldc(255);
        ob(O_LAND);
    } else if (t->kind == TY_CHAR)
        signext();
}

static void gen_str(struct Node *n)
{
    int i;
    if (n->slen > 255)
        error(99 /* string literal longer than 254 characters */, 0);
    ob(O_LPA);
    ob(n->slen);
    for (i = 0; i < n->slen; i++)
        ob(n->str[i]);
}

static int relop(int op, int isfloat, int invert)
{
    if (invert) {
        switch (op) {
        case N_EQ: op = N_NE; break;
        case N_NE: op = N_EQ; break;
        case N_LT: op = N_GE; break;
        case N_LE: op = N_GT; break;
        case N_GT: op = N_LE; break;
        case N_GE: op = N_LT; break;
        }
    }
    switch (op) {
    case N_EQ: return isfloat ? O_EQU : O_EQUI;
    case N_NE: return isfloat ? O_NEQ : O_NEQI;
    case N_LT: return isfloat ? O_LES : O_LESI;
    case N_LE: return isfloat ? O_LEQ : O_LEQI;
    case N_GT: return isfloat ? O_GRT : O_GRTI;
    }
    return isfloat ? O_GEQ : O_GEQI;
}

/* is n a char load whose sign does not matter against constant c? */
static int charsafe(struct Node *n, struct Node *c)
{
    while (n->op == N_CAST && isword(n->a->type))
        n = n->a;
    return n->type->kind == TY_CHAR && c->op == N_NUM && c->val >= 0 && c->val < 128;
}

/* push the two operands of a comparison, biased for unsigned order */
static void gen_cmpops(struct Node *n)
{
    int uns;
    int eq;
    struct Node *a;
    struct Node *b;
    a = n->a;
    b = n->b;
    eq = n->op == N_EQ || n->op == N_NE;
    uns = !eq && (isunsignedty(a->type) || a->type->kind == TY_PTR);
    if (eq && charsafe(a, b))
        gen_lowbyte(a);
    else
        gen_value(a);
    if (uns) {
        ldc(-32768);
        ob(O_ADI);
    }
    if (uns && b->op == N_NUM)
        ldc(b->val ^ -32768);
    else {
        if (eq && charsafe(b, a))
            gen_lowbyte(b);
        else
            gen_value(b);
        if (uns) {
            ldc(-32768);
            ob(O_ADI);
        }
    }
}

static void emitrelop(int op, struct Type *t, int invert)
{
    int o;
    if (isdblty(t)) {
        ldc(dblrel(op) + (invert ? 8 : 0));
        csp(CSP_DCMP);
        return;
    }
    o = relop(op, isfloatty(t), invert);
    ob(o);
    if (isfloatty(t))
        ob(2);
}

void branch(struct Node *n, int l, int jumpif)
{
    int skip;
    struct Type *t;
    switch (n->op) {
    case N_NUM:
        if (!islongty(n->type)) {
            if ((n->val != 0) == (jumpif != 0))
                jump(l);
            return;
        }
        break;
    case N_NOT:
        branch(n->a, l, !jumpif);
        return;
    case N_ANDAND:
        if (!jumpif) {
            branch(n->a, l, 0);
            branch(n->b, l, 0);
        } else {
            skip = newlabel();
            branch(n->a, skip, 0);
            branch(n->b, l, 1);
            setlabel(skip);
        }
        return;
    case N_OROR:
        if (jumpif) {
            branch(n->a, l, 1);
            branch(n->b, l, 1);
        } else {
            skip = newlabel();
            branch(n->a, skip, 1);
            branch(n->b, l, 0);
            setlabel(skip);
        }
        return;
    case N_EQ:
    case N_NE:
        t = n->a->type;
        if (!isfloatty(t)) {
            gen_cmpops(n);
            /* EFJ jumps if not equal, NFJ jumps if equal */
            if ((n->op == N_EQ) != (jumpif != 0))
                jmpop(O_EFJ, l);
            else
                jmpop(O_NFJ, l);
            return;
        }
        gen_cmpops(n);
        emitrelop(n->op, t, jumpif);
        jmpop(O_FJP, l);
        return;
    case N_LT:
    case N_LE:
    case N_GT:
    case N_GE:
        gen_cmpops(n);
        emitrelop(n->op, n->a->type, jumpif);
        jmpop(O_FJP, l);
        return;
    }
    t = n->type;
    if (isdblty(t)) {
        gen_value(n);
        gen_dconst((unsigned char *)"\0\0\0\0\0\0\0\0");
        ldc(jumpif ? 0 : 1);            /* == 0: FJP jumps when not; != 0 */
        csp(CSP_DCMP);
        jmpop(O_FJP, l);
        return;
    }
    if (isfloatty(t)) {
        gen_value(n);
        gen_fconst((unsigned char *)"\0\0\0\0");
        ob(jumpif ? O_EQU : O_NEQ);
        ob(2);
        jmpop(O_FJP, l);
        return;
    }
    if (islongty(t)) {
        gen_value(n);
        ob(O_LOR);
    } else
        gen_lowbyte(n);
    ldc(0);
    jmpop(jumpif ? O_EFJ : O_NFJ, l);
}

/* assignment family */
/* does the tree refer to the lvalue being updated (compound assignment)? */
static int haslvref(struct Node *n)
{
    for (; n; n = n->next) {
        if (n->op == N_LVREF)
            return 1;
        if ((n->a && haslvref(n->a)) || (n->b && haslvref(n->b)) || (n->c && haslvref(n->c)))
            return 1;
    }
    return 0;
}

static void gen_assign(struct Node *n, int want)
{
    struct Node *lhs;
    struct Node *rhs;
    struct Type *t;
    int ta;
    int w;
    int savet;
    struct Node *saven;
    lhs = n->a;
    rhs = n->b;
    t = lhs->type;
    if (n->op == N_ASSIGN && (t->kind == TY_STRUCT || t->kind == TY_UNION || t->kind == TY_ARRAY)) {
        if (t->kind == TY_ARRAY) {         /* char array = string literal (initialisers) */
            gen_value(rhs);
            ldc(0);
            gen_addr(lhs);
            ldc(0);
            ldc(rhs->slen < t->size ? rhs->slen : t->size);
            csp(CSP_MVL);
            return;
        }
        gen_addr(lhs);
        gen_value(rhs);
        opbig(O_MOV, (t->size + 1) / 2);
        if (want)
            gen_addr(lhs);
        return;
    }
    w = twords(t);
    savet = lvtemp;
    saven = lvnode;
    if (simplelv(lhs)) {
        lvtemp = 0;
        lvnode = lhs;
        if (n->op == N_POSTINC && want)
            gen_load(lhs);
        storepre(lhs);
        if (ischar(t))
            gen_lowbyte(rhs);
        else
            gen_value(rhs);
        storepost(lhs);
        if (want && n->op != N_POSTINC)
            gen_load(lhs);
    } else if (n->op == N_ASSIGN && !want && !haslvref(rhs)) {
        /* a plain store whose value is not used: the address stays on
           the stack (no temporary: STL t; SLDL t saved) */
        gen_addr(lhs);
        if (ischar(t)) {
            ldc(0);
            gen_lowbyte(rhs);
        } else
            gen_value(rhs);
        storeviapost(t);
    } else {
        ta = newtemp(1);
        gen_addr(lhs);
        gen_stl(ta);
        lvtemp = ta;
        lvnode = lhs;
        if (n->op == N_POSTINC && want)
            loadvia(ta, t);
        storeviapre(ta, t);
        if (ischar(t))
            gen_lowbyte(rhs);
        else
            gen_value(rhs);
        storeviapost(t);
        if (want && n->op != N_POSTINC)
            loadvia(ta, t);
    }
    lvtemp = savet;
    lvnode = saven;
    w = w;
}

void gen_value(struct Node *n)
{
    int l1;
    int l2;
    struct Type *t;
    t = n->type;
    switch (n->op) {
    case N_NUM:
        if (islongty(t)) {
            ldc(n->val2);
            ldc(n->val);
        } else
            ldc(n->val);
        return;
    case N_FNUM:
        if (isdblty(n->type))
            gen_dconst(n->fimg);
        else
            gen_fconst(n->fimg);
        return;
    case N_STR:
        gen_str(n);
        return;
    case N_HEAPSTR:
        {
            int t;
            t = newtemp(1);
            lla(t);
            ldc((n->slen + 1) / 2);
            csp(1);                     /* NEW */
            n->op = N_STR;
            gen_str(n);
            ldc(0);
            ldl(t);
            ldc(0);
            ldc(n->slen);
            csp(CSP_MVL);
            ldl(t);
        }
        return;
    case N_VAR:
    case N_MEMBER:
    case N_DEREF:
        gen_load(n);
        return;
    case N_LVREF:
        if (lvtemp)
            loadvia(lvtemp, lvnode->type);
        else
            gen_load(lvnode);
        return;
    case N_FUNC:
        ob(O_LDCI);
        reloc(R_FNPTR, n->sym->name);
        E->relpos[E->nrel - 1] = E->pc - 1;
        ob(0);
        ob(0);
        return;
    case N_ADDR:
        if (n->a->op == N_FUNC) {
            gen_value(n->a);
            return;
        }
        gen_addr(n->a);
        return;
    case N_CAST:
        gen_cast(n);
        return;
    case N_NEG:
        gen_value(n->a);
        if (isdblty(t))
            csp(CSP_DNEG);
        else
            ob(isfloatty(t) ? O_NGR : O_NGI);
        return;
    case N_BNOT:
        gen_value(n->a);
        ob(O_NOT);
        return;
    case N_NOT:
    case N_ANDAND:
    case N_OROR:
        l1 = newlabel();
        l2 = newlabel();
        branch(n, l1, 0);
        ldc(1);
        jump(l2);
        setlabel(l1);
        ldc(0);
        setlabel(l2);
        return;
    case N_EQ:
    case N_NE:
    case N_LT:
    case N_LE:
    case N_GT:
    case N_GE:
        gen_cmpops(n);
        emitrelop(n->op, n->a->type, 0);
        return;
    case N_COND:
        l1 = newlabel();
        l2 = newlabel();
        branch(n->a, l1, 0);
        gen_value(n->b);
        jump(l2);
        setlabel(l1);
        gen_value(n->c);
        setlabel(l2);
        return;
    case N_COMMA:
        gen_discard(n->a);
        gen_value(n->b);
        return;
    case N_ASSIGN:
    case N_OPASSIGN:
    case N_POSTINC:
        gen_assign(n, 1);
        return;
    case N_CALL:
        gen_call(n, 1);
        return;
    case N_INTRIN:
        gen_intrinsic(n, 1);
        return;
    case N_ADD:
    case N_SUB:
    case N_MUL:
        gen_value(n->a);
        if (n->op == N_ADD && t->kind == TY_PTR && n->b->op == N_MUL && n->b->b->op == N_NUM &&
            n->b->b->val > 0 && !(n->b->b->val & 1)) {
            /* pointer + index * (a whole number of words): IXA */
            gen_value(n->b->a);
            opbig(O_IXA, n->b->b->val / 2);
            return;
        }
        if (n->op == N_ADD && n->b->op == N_NUM && !isfloatty(t) && n->b->val > 0 && !(n->b->val & 1) && n->b->val < 256) {
            opbig(O_INC, n->b->val / 2);
            return;
        }
        gen_value(n->b);
        if (isdblty(t))
            csp(CSP_DADD + (n->op == N_ADD ? 0 : (n->op == N_SUB ? 1 : 2)));
        else if (isfloatty(t))
            ob(n->op == N_ADD ? O_ADR : (n->op == N_SUB ? O_SBR : O_MPR));
        else
            ob(n->op == N_ADD ? O_ADI : (n->op == N_SUB ? O_SBI : O_MPI));
        return;
    case N_DIV:
        gen_value(n->a);
        gen_value(n->b);
        if (isdblty(t))
            csp(CSP_DADD + 3);
        else
            ob(O_DVR);
        return;
    case N_AND:
        gen_value(n->a);
        gen_value(n->b);
        ob(O_LAND);
        return;
    case N_OR:
        gen_value(n->a);
        gen_value(n->b);
        ob(O_LOR);
        return;
    }
    error(100 /* internal: cannot generate node */, 0);
}

void gen_discard(struct Node *n)
{
    switch (n->op) {
    case N_ASSIGN:
    case N_OPASSIGN:
    case N_POSTINC:
        gen_assign(n, 0);
        return;
    case N_CALL:
        gen_call(n, 0);
        return;
    case N_INTRIN:
        gen_intrinsic(n, 0);
        return;
    case N_COMMA:
        gen_discard(n->a);
        gen_discard(n->b);
        return;
    case N_CAST:
        if (n->type->kind == TY_VOID) {
            gen_discard(n->a);
            return;
        }
        break;
    case N_COND:
        {
            int l1;
            int l2;
            l1 = newlabel();
            l2 = newlabel();
            branch(n->a, l1, 0);
            gen_discard(n->b);
            jump(l2);
            setlabel(l1);
            gen_discard(n->c);
            setlabel(l2);
        }
        return;
    case N_NUM:
    case N_VAR:
        return;
    }
    gen_value(n);
    drop(valwords(n->type));
}

void gen_return(struct Node *n, struct Type *ft, int sretoff)
{
    int rw;
    struct Type *t;
    if (!n)
        return;
    t = ft->base;
    if (t->kind == TY_STRUCT || t->kind == TY_UNION) {
        ldl(sretoff);
        gen_value(n);
        opbig(O_MOV, (t->size + 1) / 2);
        ldl(sretoff);
        gen_stl(1);
        return;
    }
    rw = retwords(ft);
    if (rw == 1) {
        gen_value(n);
        gen_stl(1);
    } else {
        lla(1);
        gen_value(n);
        ob(O_STM);
        ob(rw);
    }
}

void gen_switch(int t, int *vals, int *labs, int n, int deflab)
{
    int lo;
    int hi;
    int i;
    int v;
    int range;
    if (n == 0) {
        jump(deflab);
        return;
    }
    lo = vals[0];
    hi = vals[0];
    for (i = 1; i < n; i++) {
        if (vals[i] < lo)
            lo = vals[i];
        if (vals[i] > hi)
            hi = vals[i];
    }
    range = hi - lo + 1;
    if (n >= 4 && range > 0 && range <= 3 * n + 6 && range < 1000) {
        ldl(t);
        ob(O_XJP);
        if (E->pc & 1)
            ob(0);
        ob(lo & 255);
        ob((lo >> 8) & 255);
        ob(hi & 255);
        ob((hi >> 8) & 255);
        jump(deflab);
        for (v = lo; v <= hi; v++) {
            for (i = 0; i < n; i++)
                if (vals[i] == v)
                    break;
            caseword(i < n ? labs[i] : deflab);
        }
        return;
    }
    for (i = 0; i < n; i++) {
        ldl(t);
        ldc(vals[i]);
        jmpop(O_NFJ, labs[i]);
    }
    jump(deflab);
}

/* ---- the object file ---- */
