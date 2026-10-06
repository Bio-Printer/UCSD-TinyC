/* tc.h -- Tiny-C compiler for the UCSD Pascal II.0 P-machine: shared declarations.
 *
 * Written in the C subset Tiny-C itself accepts, so it builds both with a
 * host compiler (gcc, MSVC: the bootstrap) and with Tiny-C on the P-System.
 * Every value that reaches the output is computed with 16-bit semantics
 * (see W16), so both builds write byte-identical files.
 */
#ifndef TC_H
#define TC_H

#ifdef __TINYC__
#define NO_FLOAT_PRINTF             /* the compiler prints no floats */
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef __TINYC__
#define W16(x) (x)                  /* int is 16 bits already */
#else
#define W16(x) ((((x) & 65535) ^ 32768) - 32768)   /* wrap to a signed 16-bit value */
#endif

/* ---- limits ---- */
#define MAXLINE   512      /* longest source line */
#define MAXEXP    1024     /* longest line after macro expansion */
#define MAXINCL   8        /* #include nesting */
#define MAXIF     32       /* #if nesting */
/* The code generator's buffers for one procedure, allocated for every
   code generation pass: about 1.5 times the most any procedure of the
   compiler, its library, the demos and tests needs (code 1928 bytes,
   labels 119, fixups 138, relocations 211); more is "function too large" */
#define MAXCODE   3000     /* code bytes in one procedure */
#define MAXLABEL  200      /* labels in one procedure */
#define MAXFIX    250      /* jump fixups in one procedure */
#define MAXLONGJ  60       /* jump-table entries in one procedure (II.0 limit) */
#define MAXREL    320      /* relocations in one procedure */
#define MAXSEGS   10       /* segment 1 and 7..15 */
#define MAXNAME   64

/* ---- token kinds (single-character punctuators are their own code) ---- */
#define T_EOF     0
#define T_ID      256
#define T_NUM     257       /* integer constant: tokval, toklong for long */
#define T_FNUM    258       /* floating constant: tokreal */
#define T_STR     259       /* string literal: tokstr, toklen (incl. NUL) */
#define T_CHR     260       /* character constant (T_NUM with tokval) */
#define T_ARROW   261
#define T_INC     262
#define T_DEC     263
#define T_SHL     264
#define T_SHR     265
#define T_LE      266
#define T_GE      267
#define T_EQ      268
#define T_NE      269
#define T_ANDAND  270
#define T_OROR    271
#define T_ADDA    272       /* += ... in the same order as the binary operators */
#define T_SUBA    273
#define T_MULA    274
#define T_DIVA    275
#define T_MODA    276
#define T_ANDA    277
#define T_ORA     278
#define T_XORA    279
#define T_SHLA    280
#define T_SHRA    281
#define T_ELLIPSIS 282
/* keywords */
#define K_FIRST   300
#define K_AUTO    300
#define K_BREAK   301
#define K_CASE    302
#define K_CHAR    303
#define K_CONST   304
#define K_CONTINUE 305
#define K_DEFAULT 306
#define K_DO      307
#define K_DOUBLE  308
#define K_ELSE    309
#define K_ENUM    310
#define K_EXTERN  311
#define K_FLOAT   312
#define K_FOR     313
#define K_GOTO    314
#define K_IF      315
#define K_INT     316
#define K_LONG    317
#define K_REGISTER 318
#define K_RETURN  319
#define K_SHORT   320
#define K_SIGNED  321
#define K_SIZEOF  322
#define K_STATIC  323
#define K_STRUCT  324
#define K_SWITCH  325
#define K_TYPEDEF 326
#define K_UNION   327
#define K_UNSIGNED 328
#define K_VOID    329
#define K_VOLATILE 330
#define K_WHILE   331
#define K_LAST    331

/* ---- types ---- */
#define TY_VOID    0
#define TY_CHAR    1        /* plain/signed char */
#define TY_UCHAR   2
#define TY_INT     3        /* int, short, enum */
#define TY_UINT    4
#define TY_LONG    5
#define TY_ULONG   6
#define TY_FLOAT   7
#define TY_DOUBLE  8
#define TY_LDOUBLE 9
#define TY_PTR     10
#define TY_ARRAY   11
#define TY_STRUCT  12
#define TY_UNION   13
#define TY_FUNC    14

struct Field {
    char *name;
    struct Type *type;
    int offset;             /* bytes */
    struct Field *next;
};

/* parameter names are not part of a function type (types with the same
   signature are shared); a definition's names are in pnames (decl.c) */
struct Param {
    struct Type *type;
    struct Param *next;
};

/* the small fields are bytes (unsigned: a signed char costs a sign
   extension on every load): there are hundreds of types and symbols in
   the compile pass */
struct Type {
    unsigned char kind;
    unsigned char variadic;
    unsigned char oldstyle; /* f() -- parameters unknown */
    int size;               /* bytes; -1 = incomplete */
    int align;              /* -1 = a temporary type (see mktype) */
    struct Type *base;      /* pointer/array element, function result */
    int len;                /* array length, -1 = unknown */
    struct Field *fields;   /* struct/union */
    struct Param *params;   /* function */
    char *tag;
    struct Type *ptrto;     /* cached pointer-to-this type */
    struct Type *next;      /* function types: the list of distinct ones */
};

/* ---- symbols ---- */
#define S_GLOBAL   1
#define S_LOCAL    2        /* locals and parameters: word offset in frame */
#define S_FUNC     3
#define S_TYPEDEF  4
#define S_ENUMC    5
#define S_TAG      6        /* struct/union/enum tag */
#define S_LABEL    7

struct Sym {
    char *name;
    unsigned char kind;
    unsigned char level;    /* scope level (0 = file) */
    unsigned char isstatic;
    unsigned char defined;  /* functions: has a body; globals: 1 common, 2 initialised */
    struct Type *type;
    int offset;             /* word offset (globals, locals), enum value, label number */
    char *lname;            /* link name when it differs (static functions) */
    char *seg;              /* functions: the segment, when known (0 = unknown) */
    struct Sym *next;       /* hash chain */
    struct Sym *scopenext;  /* symbols of one scope */
};

/* ---- expression trees ---- */
#define N_NUM     1         /* val (int), val/val2 for long */
#define N_FNUM    2         /* fimg[] */
#define N_STR     3         /* str, slen */
#define N_VAR     4         /* sym */
#define N_FUNC    5         /* sym: function designator */
#define N_CALL    6         /* a = function, args = list */
#define N_INDEX   7
#define N_MEMBER  8         /* a.field (a is an lvalue struct); val = offset */
#define N_DEREF   9
#define N_ADDR    10
#define N_NEG     11
#define N_NOT     12        /* ! */
#define N_BNOT    13        /* ~ */
#define N_CAST    14
#define N_ASSIGN  15
#define N_OPASSIGN 16       /* val = binary op */
#define N_PREINC  17        /* val = +1/-1 */
#define N_POSTINC 18
#define N_COND    19        /* a ? b : c */
#define N_COMMA   20
#define N_ANDAND  21
#define N_OROR    22
#define N_ADD     23
#define N_SUB     24
#define N_MUL     25
#define N_DIV     26
#define N_MOD     27
#define N_AND     28
#define N_OR      29
#define N_XOR     30
#define N_SHL     31
#define N_SHR     32
#define N_EQ      33
#define N_NE      34
#define N_LT      35
#define N_LE      36
#define N_GT      37
#define N_GE      38
#define N_VASTART 39
#define N_VAARG   40
#define N_INTRIN  41        /* val = intrinsic number, args */
#define N_SIZEOFX 42

struct Node {
    int op;
    struct Type *type;
    struct Node *a;
    struct Node *b;
    struct Node *c;
    struct Node *next;      /* argument lists */
    struct Sym *sym;
    int val;
    int val2;
    char *str;
    int slen;
    unsigned char *fimg;
};

/* ---- globals shared between the parts ---- */
extern int nerrors;
extern int z80calls;            /* -z, /Z: calls through function pointers for the Z80 interpreter (no CSP 138) */
extern char *curfile;
extern int curline;

/* Each group of prototypes below is declared under the segment its
   functions are in: a call from the same segment is then a 2-byte CGP
   instead of a 3-byte CXP (the linker checks it: message 116).  Every
   module sets its own #pragma segment after its #includes. */
#pragma segment MAIN

/* util */
void error(int n, char *arg);
void fatal(int n, char *arg);
void memfail(int n);
char *itoa10(int n, char *b);
void say(char *s);
void sayn(int n);
void sayw(char *s, int w);
void saynw(int n, int w);
void warn(int n, char *arg);
char *palloc(int n);            /* permanent */
char *falloc(int n);            /* until end of the current function */
char *xalloc(int n);            /* until end of the current statement */
void freset(void);
int xmark(void);
void xrelease(int m);
char *pstrdup(char *s);
void resetpools(void);
struct PMark { void *heap[2]; char *pcur; int pleft; };
#pragma segment LINK
void pmark(struct PMark *m);
void prelease(struct PMark *m);
#pragma segment MAIN
void xsetsize(int n);
int hashstr(char *s);

/* lexer */
extern int tok;
extern int insys;
extern int tokval;
extern int tokval2;
extern int toklong;
extern char *tokname;           /* [MAXNAME], allocated by lexinit */
extern char *tokstr;
extern int toklen;
extern unsigned char tokreal[4];
extern char *toknum;
#pragma segment PARSE
void lexinit(FILE *fp);
void next(void);
int peek(void);
char *peekname(void);

/* types */
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
struct Type *ptrto(struct Type *t);
#pragma segment MAIN
int isintegral(struct Type *t);
int isfloatty(struct Type *t);
int isdblty(struct Type *t);
int islongty(struct Type *t);
int isunsignedty(struct Type *t);
int isword(struct Type *t);
int isptrlike(struct Type *t);
int isscalar(struct Type *t);
int isaggregate(struct Type *t);
int twords(struct Type *t);
int retwords(struct Type *ft);

/* code generation (gen.c) */
#pragma segment GEN
#define N_LVREF   43
#define N_HEAPSTR 44        /* string literal copied to the heap (global initialisers) */        /* value of the lvalue being updated (compound assignment) */
extern int curlocal;
extern int maxlocal;
extern int nparamwords;
extern int scratch;
extern char *cursegname;
extern FILE *objout;
int newlabel(void);
void setlabel(int l);
void jump(int l);
void branch(struct Node *n, int l, int jumpif);
void gen_value(struct Node *n);
void gen_discard(struct Node *n);
void gen_return(struct Node *n, struct Type *ft, int sretoff);
int newtemp(int words);
void gen_funcbegin(void);
void gen_funcend(char *name, struct Type *ft, int exitlabel, int isstatic, char *seg);
void gen_initbegin(void);
void gen_initend(void);
void gen_initflush(void);
void gen_switch(int tempoff, int *vals, int *labs, int n, int deflab);
void gen_stl(int off);
void gen_objheader(char *modname);
void gen_objend(int globalwords);

/* intermediate file (ir.c) */
#pragma segment PARSE
void ir_open(char *name, char *modname);
void ir_close(int globalwords);
void ir_funcbegin(void);
int ir_newlabel(void);
void ir_setlabel(int l);
void ir_jump(int l);
void ir_branch(struct Node *n, int l, int jumpif);
void ir_discard(struct Node *n);
void ir_valuestl(struct Node *n, int t);
void ir_return(struct Node *n, struct Type *ft, int sretoff);
void ir_switch(int t, int *vals, int *labs, int n, int deflab);
void ir_funcend(char *name, struct Type *ft, int exitlab, int isstatic, char *seg);
void ir_initbegin(void);
void ir_initend(void);
void ir_initflush(void);
void ir_data(char *name, int words, int strong);
void ir_use(char *name);
#pragma segment GEN
int gencode(char *ir, char *obj);
void gen_objdata(char *name, int words, int strong);
void gen_objuse(char *name);

/* passes */
#pragma segment PP
int preprocess(char *src, char *out);
#pragma segment PARSE
int compile(char *src, char *obj, char *modname);
#pragma segment LINK
int link(char **objs, int nobjs, char *code, char *progname);
int join(char **objs, int nobjs, char *out);
#pragma segment MAIN

#endif
