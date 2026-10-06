/* compile.c -- the parser's pass: compile() and #pragma.  Split from
   stmt.c so that each compiles in less memory on the P-System (the
   declarations a file uses take its memory, in blocks of PCHUNK bytes);
   segment PARSE, its end (compileend) CINIT.  The runtime helpers are declared when first
   needed (helper, expr.c). */
#include "tc.h"
#include "parse.h"
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
    cursegname = "";
    lexinit(fp);
    next();
    while (tok != T_EOF) {
        external();
    }
    fclose(fp);
    return 1;                       /* compileend (CINIT) finishes the module */
}

/* The end of the module, called after compile has returned: PARSE's code
   is then off the stack, and the heap is at its largest -- the closing of
   the intermediate file (the operating system's frames) was the least
   free memory of many modules. */
#pragma segment CINIT
int compileend(void)
{
    /* the module's exported variables: name, size, initialised or common */
    struct Sym *g;
    int h;
    ir_initflush();
    for (h = 0; h < HSIZE; h++)
        for (g = htab[h]; g; g = g->next)
            if (g->kind == S_GLOBAL && !g->isstatic && g->defined) {
                if (g->type->size < 0)
                    error(73 /* incomplete type */, g->name);
                ir_data(g->name, (g->type->size + 1) / 2, g->defined == 2);
            }
    if (usesfloat && !nofltused)
        ir_use("__fltused");
    ir_close(globoff);
    return nerrors == 0;
}
#pragma segment PARSE
