/* compile.c -- the parser's pass: compile(), the runtime helpers' and
   #pragma declarations.  Split from stmt.c so that each compiles in less
   memory on the P-System (the declarations a file uses take its memory,
   in blocks of PCHUNK bytes); same segments (CINIT, PARSE). */
#include "tc.h"
#include "parse.h"

#pragma segment CINIT

/* declare the runtime helpers the code generator calls (defined in tcrt.h) */
void declhelper(char *name, struct Type *ret, struct Type *a, struct Type *b)
{
    struct Type *ft;
    struct Param *p;
    struct Sym *s;
    ft = mktype(TY_FUNC, 2, 2);
    ft->base = ret;
    if (a) {
        p = (struct Param *)palloc(sizeof(struct Param));
        p->type = a;
        ft->params = p;
        if (b) {
            p->next = (struct Param *)palloc(sizeof(struct Param));
            p->next->type = b;
        }
    }
    s = addsym(name, S_FUNC, ft);
}

void helpers(void)
{
    declhelper("__divi", ty_int, ty_int, ty_int);
    declhelper("__modi", ty_int, ty_int, ty_int);
    declhelper("__udiv", ty_uint, ty_uint, ty_uint);
    declhelper("__umod", ty_uint, ty_uint, ty_uint);
    declhelper("__shl", ty_int, ty_int, ty_int);
    declhelper("__shr", ty_int, ty_int, ty_int);
    declhelper("__ushr", ty_uint, ty_uint, ty_int);
    declhelper("__xor", ty_int, ty_int, ty_int);
    declhelper("__sx", ty_int, ty_int, 0);
    declhelper("__utof", ty_float, ty_uint, 0);
    declhelper("__ftou", ty_uint, ty_float, 0);
    declhelper("__ladd", ty_long, ty_long, ty_long);
    declhelper("__lsub", ty_long, ty_long, ty_long);
    declhelper("__lmul", ty_long, ty_long, ty_long);
    declhelper("__ldiv", ty_long, ty_long, ty_long);
    declhelper("__lmod", ty_long, ty_long, ty_long);
    declhelper("__uldiv", ty_ulong, ty_ulong, ty_ulong);
    declhelper("__ulmod", ty_ulong, ty_ulong, ty_ulong);
    declhelper("__land", ty_long, ty_long, ty_long);
    declhelper("__lor", ty_long, ty_long, ty_long);
    declhelper("__lxor", ty_long, ty_long, ty_long);
    declhelper("__lshl", ty_long, ty_long, ty_int);
    declhelper("__lshr", ty_long, ty_long, ty_int);
    declhelper("__ulshr", ty_ulong, ty_ulong, ty_int);
    declhelper("__lneg", ty_long, ty_long, 0);
    declhelper("__lnot", ty_long, ty_long, 0);
    declhelper("__lcmp", ty_int, ty_long, ty_long);
    declhelper("__ulcmp", ty_int, ty_ulong, ty_ulong);
    declhelper("__itol", ty_long, ty_int, 0);
    declhelper("__utol", ty_long, ty_uint, 0);
    declhelper("__ltoi", ty_int, ty_long, 0);
    declhelper("__ltof", ty_float, ty_long, 0);
    declhelper("__ultof", ty_float, ty_ulong, 0);
    declhelper("__ftol", ty_long, ty_float, 0);
    declhelper("__ftoul", ty_ulong, ty_float, 0);
}

#pragma segment PARSE

void pragma(char *s)
{
    char name[MAXNAME];
    int n;
    while (*s == ' ' || *s == '\t')
        s++;
    if (strncmp(s, "segment", 7) == 0) {
        s = s + 7;
        while (*s == ' ' || *s == '\t')
            s++;
        n = 0;
        while (*s && *s != ' ' && *s != '\t' && n < 8) {
            name[n] = *s >= 'a' && *s <= 'z' ? *s - 32 : *s;
            n++;
            s++;
        }
        name[n] = 0;
        if (strcmp(name, "MAIN") == 0)
            name[0] = 0;
        cursegname = pstrdup(name);
        segexplicit = 1;
    } else if (strncmp(s, "nofltused", 9) == 0)
        nofltused = 1;              /* library modules: float use does not link %f */
}

int compile(char *src, char *ir, char *mod)
{
    FILE *fp;
    /* fresh parser state: @FILE runs several compiles in one execution,
       and the previous module's symbols pointed into released memory */
    htab = (struct Sym **)calloc(HSIZE, sizeof(struct Sym *));
    ttab = (struct Sym **)calloc(HSIZE, sizeof(struct Sym *));
    scopes = (struct Sym **)calloc(40, sizeof(struct Sym *));
    pnames = (char **)calloc(32, sizeof(char *));
    functypes = 0;
    npnames = 0;
    if (!htab || !ttab || !scopes || !pnames)
        fatal(2 /* out of memory */, 0);
    level = 0;
    labels = 0;
    curfn = 0;
    curft = 0;
    tentative = 0;
    globinit = 0;
    swvals = 0;
    swlabs = 0;
    swn = 0;
    swmax = 0;
    segexplicit = 0;
    curfnseg = 0;
    modname = mod;
    usesfloat = 0;
    nofltused = 0;
    fp = fopen(src, "r");
    if (!fp)
        fatal(25 /* cannot open */, src);
    ir_open(ir, modname);
    scanrefs(src);
    typeinit();
    helpers();
    cursegname = "";
    lexinit(fp);
    next();
    while (tok != T_EOF) {
        external();
    }
    ir_initflush();
    {
        /* the module's exported variables: name, size, initialised or common */
        struct Sym *g;
        int h;
        for (h = 0; h < HSIZE; h++)
            for (g = htab[h]; g; g = g->next)
                if (g->kind == S_GLOBAL && !g->isstatic && g->defined) {
                    if (g->type->size < 0)
                        error(73 /* incomplete type */, g->name);
                    ir_data(g->name, (g->type->size + 1) / 2, g->defined == 2);
                }
    }
    if (usesfloat && !nofltused)
        ir_use("__fltused");
    ir_close(globoff);
    fclose(fp);
    return nerrors == 0;
}
