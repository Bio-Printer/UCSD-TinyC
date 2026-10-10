/* parse.h -- private declarations shared by the parser modules
   (psym.c, expr.c, decl.c, stmt.c) */
#ifndef PARSE_H
#define PARSE_H
/* the prototypes are declared under their functions' segments (see tc.h) */
#pragma segment PARSE

#define HSIZE 32
#define I_DVI     1
#define I_MDI     2
#define I_VASTART 3
#define I_CSPV    4
#define I_CSPI    5
#define I_CSPF    6
#define I_CXP0V   7
#define I_CXP0I   8
#define I_OSVAR   9
#define I_EXITP   10
#define I_OSVARA  11
#define I_CSPD    12        /* __cspd(n, ...): CSP n leaving a double (8 bytes) */
#define RBITS 4096
struct Dcl {
    int n;
    int kind[12];
    int len[12];
    struct Type *ft[12];
    char name[MAXNAME];
};


extern struct Type *ty_void;
extern struct Type *ty_char;
extern struct Type *ty_uchar;
extern struct Type *ty_int;
extern struct Type *ty_uint;
extern struct Type *ty_long;
extern struct Type *ty_ulong;
extern struct Type *ty_float;
extern struct Type *ty_double;
extern struct Type *ty_ldouble;
extern struct Type *ty_charp;
extern struct Sym **htab;
extern struct Sym **ttab;
extern struct Sym **scopes;
extern int level;
extern struct Sym *labels;
extern char *modname;
extern int nofltused;
extern int usesfloat;
extern int segexplicit;
extern int curfnseg;
extern int cursegi;
extern int globoff;
extern struct Sym *curfn;
extern struct Type *curft;
extern int exitlab;
extern int sretoff;
extern int vaoff;
/* The case values of the switch being parsed (to find a duplicate): in
   chunks, newest first, SWCHUNK to a chunk, no table to grow and copy;
   the labels go to the intermediate file as they come (ir_case) and the
   code generator collects them.  swn: how many so far, -1 outside a switch. */
#define SWCHUNK 24
struct SwVals {
    struct SwVals *next;
    int v[SWCHUNK];
};
extern struct SwVals *swvals;
extern int swn;
extern int swdef;
extern char *intrnames[];
extern int tentative;
extern struct Type *functypes;
extern char **pnames;
extern int npnames;
extern unsigned char *refbits;
extern int globinit;

struct Type *mktype(int kind, int size, int align);
#pragma segment CINIT
void typeinit(void);
#pragma segment PARSE
struct Type *ptrto(struct Type *t);
struct Type *arrayof(struct Type *t, int n);
struct Type *permtype(struct Type *t);
struct Type *functype(struct Type *f);
int refhash2(char *s);
int isref(char *name);
int sametype(struct Type *a, struct Type *b);
struct Sym *lookup(char *name);
struct Sym *lookuptag(char *name);
struct Sym *addsym(char *name, int kind, struct Type *t);
void pushscope(void);
void popscope(void);
int allocglobal(struct Type *t);
int alloclocal(struct Type *t);
void expect(int t, char *what);
int istypename(void);
#pragma segment REALLIT
void dblimage(char *text, unsigned char *img);
void real2dbl(unsigned char *f, unsigned char *img);
#pragma segment PARSE
struct Node *mknode(int op, struct Type *t, struct Node *a, struct Node *b);
struct Node *mknum(int v, struct Type *t);
int isconst(struct Node *n);
int islvalue(struct Node *n);
struct Node *decay(struct Node *n);
struct Sym *helper(char *name);
struct Node *call1(char *name, struct Node *a, struct Node *b);
struct Node *helpercall(char *name, struct Node *a, struct Node *b);
struct Node *cast(struct Node *n, struct Type *t);
struct Type *arith(struct Type *a, struct Type *b);
int fold(int op, int a, int b, int uns);
char *lhelper(int op, int uns);
struct Node *binop(int op, struct Node *a, struct Node *b);
struct Node *fzero(void);
struct Node *cond(struct Node *n);
struct Node *arglist(struct Type *ft, int *nargs);
struct Node *intrinsic(int code);
struct Node *primary(void);
struct Field *findfield(struct Type *t, char *name, int *off);
struct Node *member(struct Node *n, char *name);
struct Node *deref(struct Node *n);
struct Node *postfix(void);
struct Node *unary(void);
struct Node *condtail(struct Node *c);
int binprec(int t, int *op);
struct Node *binexpr(int minprec);
struct Node *condexpr(void);
struct Node *assign(void);
struct Node *expr(void);
int constexpr(void);
int isnested(void);
void dcl(struct Dcl *d);
struct Type *applydcl(struct Type *t, struct Dcl *d);
struct Type *declarator(struct Type *base, char *name);
struct Type *typename(void);
struct Param *paramlist(int *variadic, int *oldstyle);
struct Type *structspec(int isunion);
struct Type *enumspec(void);
struct Type *declspec(int *sclass);
struct Node *elem(struct Node *lv, int off, struct Type *t);
void initializer(struct Node *lv, struct Type *t, int global);
void init1(struct Node *lv, struct Type *t, int global);
struct Sym *label(char *name);
void localdecl(void);
void compound(int brk, int cont);
struct Node *condparen(void);
void statement(int brk, int cont);
void funcdef(struct Sym *fs, int isstatic);
void external(void);
int fmtfam(char *name);
#pragma segment REFSCAN
void fmtfix(int fam, struct Node *args);
void addref(char *name);
void scanrefs(char *src);
#pragma segment PARSE
void pragma(char *s);
int compile(char *src, char *ir, char *mod);
#pragma segment CINIT
int compileend(void);
#pragma segment PARSE
#endif
