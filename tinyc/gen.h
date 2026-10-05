/* gen.h -- shared by gen.c (the emitter, procedures, the object file)
 * and genx.c (code for expressions): opcodes, relocation kinds, the
 * emitter, and the emitter routines genx.c uses.
 */
#ifndef GEN_H
#define GEN_H

/* opcodes */
#define O_ABI  0x80
#define O_ADI  0x82
#define O_ADR  0x83
#define O_LAND 0x84
#define O_DVI  0x86
#define O_DVR  0x87
#define O_FLT  0x8A
#define O_LOR  0x8D
#define O_MODI 0x8E
#define O_MPI  0x8F
#define O_IXA  0xA4
#define O_MPR  0x90
#define O_NGI  0x91
#define O_NGR  0x92
#define O_NOT  0x93
#define O_SBI  0x95
#define O_SBR  0x96
#define O_STO  0x9A
#define O_CSP  0x9E
#define O_FJP  0xA1
#define O_INC  0xA2
#define O_IND  0xA3
#define O_LAO  0xA5
#define O_MOV  0xA8
#define O_LDO  0xA9
#define O_SRO  0xAB
#define O_XJP  0xAC
#define O_RNP  0xAD
#define O_EQU  0xAF
#define O_GEQ  0xB0
#define O_GRT  0xB1
#define O_LDA  0xB2
#define O_LDC  0xB3
#define O_LEQ  0xB4
#define O_LES  0xB5
#define O_LOD  0xB6
#define O_NEQ  0xB7
#define O_UJP  0xB9
#define O_LDM  0xBC
#define O_STM  0xBD
#define O_LDB  0xBE
#define O_STB  0xBF
#define O_EQUI 0xC3
#define O_GEQI 0xC4
#define O_GRTI 0xC5
#define O_LLA  0xC6
#define O_LDCI 0xC7
#define O_LEQI 0xC8
#define O_LESI 0xC9
#define O_LDL  0xCA
#define O_NEQI 0xCB
#define O_STL  0xCC
#define O_CXP  0xCD
#define O_CGP  0xCF
#define O_LPA  0xD0
#define O_EFJ  0xD3
#define O_NFJ  0xD4
#define O_SIND0 0xF8

#define CSP_EXIT 4
#define CSP_MVL  2
#define CSP_TNC  23
#define CSP_CALLI 138               /* call through a function pointer: pops seg | proc << 8 (engine) */

/* relocation kinds in the object file */
#define R_CALL    1         /* CXP s,p at pos */
#define R_FNPTR   2         /* LDCI word at pos+1: seg | proc << 8 */
#define R_GSTAT   3         /* 2-byte global operand at pos: module static */
#define R_GNAME   4         /* 2-byte global operand at pos: named variable */
#define R_NEAR    5         /* CGP p at pos: a call within the segment */

struct Emit {
    unsigned char *code;
    int max;
    int pc;
    int *labpos;
    int maxlab;
    int nlab;
    int *fixpos;
    int *fixlab;
    int maxfix;
    int nfix;
    int *relpos;
    int *reltype;
    char **relname;
    int maxrel;
    int nrel;
    int curlocal;
    int maxlocal;
    int nparam;
    int scratch;
};

extern struct Emit *E;          /* the current emitter */
extern int lvtemp;              /* compound assignment: temp holding the address, 0 = simple lvalue */
extern struct Node *lvnode;     /* compound assignment: the lvalue */

void ob(int b);
void big(int v);
void opbig(int op, int v);
void ldc(int v);
void ldl(int off);
void lla(int off);
void ind(int k);
void addconst(int bytes);
void csp(int n);
void reloc(int type, char *name);
void drop(int n);
void jmpop(int op, int l);
void caseword(int l);
int valwords(struct Type *t);
int ischar(struct Type *t);

#endif
