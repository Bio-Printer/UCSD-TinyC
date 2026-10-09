/* libint.h -- private declarations shared by the library sources */
/* every library function is in the program's main segment: declared so,
   calls between them are 2-byte CGPs (see tc.h; the linker checks) */
#pragma segment MAIN
#include <stddef.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>
#include <conio.h>
#include <io.h>
#include <fcntl.h>

extern char __conout[80];
extern int __conlen;
extern char __conin[130];
extern int __coninlen;
extern int __coninpos;
void __conflush(void);
void __conputc(int c);
int __conrawgetc(void);
int __congetc(void);
void __ptitle(char *name, char *p);
int __istext(char *name);
int __blockio(FILE *f, int nblocks, int block, int doread);
int __flushbuf(FILE *f);
int __loadblock(FILE *f, int b);
int __textgetc(FILE *f);
void __stdio_exit(void);
extern FILE *__of;
extern char *__os;
extern int __on;
void __oc(int c);
void __opad(int n, int c);
char *__udigits(unsigned long v, int base, int upper, char *end);
void __ofield(char *pre, char *body, int n, int zeros, int width, int fl);
char *__osign(int neg, int fl);
int __fdigits(float v, int prec, int style, int alt, char *out);
extern int (*__fltfmt)(float v, int prec, int style, int alt, char *out);
int __vformat(char *fmt, va_list ap);
extern FILE *__if;
extern char *__is;
extern int __in;
int __ic(void);
void __iunc(int c);
int __vscan(char *fmt, va_list ap);
extern unsigned *__freelist;
extern unsigned long __rand;
extern char *__heapmk;
void __qswap(char *a, char *b, size_t n);
void __stdio_exit(void);
