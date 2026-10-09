/* stdio.h -- Tiny-C standard I/O for the UCSD Pascal II.0 P-System.
 *
 * Console: UNITWRITE to unit 1, UNITREAD from unit 2 (no echo; stdin is
 * line-buffered here with its own echo and backspace handling).  '\n' is
 * a carriage return on the P-System.  ^C at the start of a line is EOF.
 *
 * Disk files: the operating system's FINIT/FOPEN/FBLOCKIO/FCLOSE on an
 * untyped file; Tiny-C buffers blocks itself.  A name ending in .TEXT
 * is a UCSD text file: 2 header blocks, then 1K pages of whole lines
 * ended by CR and padded with NULs, leading blanks as DLE + count.  Other
 * files are binary and always a whole number of 512-byte blocks.
 *
 * #define NO_FLOAT_PRINTF before including this file to leave %f, %e and
 * %g out of printf (about 1.5K of code less).
 */
#ifndef __STDIO_H
#define __STDIO_H
#include <stddef.h>
#include <stdarg.h>

#define EOF (-1)
#define BUFSIZ 512
#define FOPEN_MAX 6
#define FILENAME_MAX 24
#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2

#define __F_READ  1
#define __F_WRITE 2
#define __F_TEXT  4
#define __F_CON   8
#define __F_EOF   16
#define __F_ERR   32
#define __F_DIRTY 64
#define __F_NEW   128

typedef struct __file {
    int flags;
    int blk;                    /* block number of buf[0] (-1: nothing loaded) */
    int pos;                    /* position in buf */
    int len;                    /* valid bytes in buf */
    int ungot;                  /* ungetc character, -1 none */
    int dle;                    /* text read: blanks still to deliver */
    int linestart;              /* text write: start of the current line in buf */
    int bufsize;                /* 512 binary, 1024 text */
    unsigned char *buf;
    char *fib;                  /* the operating system's file information block (80 bytes) */
} FILE;

extern FILE __files[FOPEN_MAX + 3];
#define stdin (&__files[0])
#define stdout (&__files[1])
#define stderr (&__files[2])

int fflush(FILE *f);
int fputc(int c, FILE *f);
int fgetc(FILE *f);

/* ---- console ---- */






/* ---- disk files ---- */

/* UCSD title from a C string: length byte + upper case characters */




/* binary: make block b current */

FILE *fopen(char *name, char *mode);


int fgetc(FILE *f);

int fputc(int c, FILE *f);

int fflush(FILE *f);

int fclose(FILE *f);

int remove(char *name);

int feof(FILE *f);

int ferror(FILE *f);

void clearerr(FILE *f);

int ungetc(int c, FILE *f);

int fseek(FILE *f, long off, int whence);

long ftell(FILE *f);

void rewind(FILE *f);

size_t fread(void *p, size_t size, size_t n, FILE *f);

size_t fwrite(void *p, size_t size, size_t n, FILE *f);

int getc(FILE *f);

int putc(int c, FILE *f);

int getchar(void);

int putchar(int c);

char *fgets(char *s, int n, FILE *f);

char *gets(char *s);

int fputs(char *s, FILE *f);

int puts(char *s);

void perror(char *s);


/* ---- formatted output ---- */

/* output goes to a FILE, or to a string when f is NULL */



/* the digits of an unsigned long in base b, backwards to the end of a buffer */


#ifndef NO_FLOAT_PRINTF
/* floating point: f, e or g style */

#endif


int vfprintf(FILE *f, char *fmt, va_list ap);

int vsprintf(char *s, char *fmt, va_list ap);

int printf(char *fmt, ...);

int fprintf(FILE *f, char *fmt, ...);

int sprintf(char *s, char *fmt, ...);

/* ---- formatted input ---- */





int vfscanf(FILE *f, char *fmt, va_list ap);

int scanf(char *fmt, ...);

int fscanf(FILE *f, char *fmt, ...);

int sscanf(char *s, char *fmt, ...);

/* the console streams (set up by the initialiser procedure) */


#endif
