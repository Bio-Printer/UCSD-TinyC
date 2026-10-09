/* stdio.c -- Tiny-C library: the code behind <stdio.h> */
#include "libint.h"
#pragma nofltused
FILE __files[FOPEN_MAX + 3];
char __conout[80];
int __conlen;
char __conin[130];
int __coninlen;
int __coninpos;
void __conflush(void)
{
    if (__conlen > 0) {
        __cspv(6, 1, __conout, 0, __conlen, 0, 0);
        __conlen = 0;
    }
}

void __conputc(int c)
{
    if (c == '\n')
        c = 13;
    __conout[__conlen++] = c;
    if (c == 13 || __conlen >= 80)
        __conflush();
}

int __conrawgetc(void)
{
    char c;
    __conflush();
    __cspv(5, 2, &c, 0, 1, 0, 0);
    return c & 255;
}

int __congetc(void)
{
    int c;
    char e[3];
    if (__coninpos < __coninlen)
        return __conin[__coninpos++] & 255;
    __coninlen = 0;
    __coninpos = 0;
    for (;;) {
        c = __conrawgetc();
        if (c == 3 && __coninlen == 0)
            return EOF;
        if (c == 8 || c == 127) {
            if (__coninlen > 0) {
                __coninlen--;
                e[0] = 8;
                e[1] = ' ';
                e[2] = 8;
                __cspv(6, 1, e, 0, 3, 0, 0);
            }
            continue;
        }
        if (c == 13 || c == 10) {
            __conin[__coninlen++] = '\n';
            e[0] = 13;
            __cspv(6, 1, e, 0, 1, 0, 0);
            break;
        }
        if (__coninlen < 127) {
            __conin[__coninlen++] = c;
            e[0] = c;
            __cspv(6, 1, e, 0, 1, 0, 0);
        }
    }
    return __conin[__coninpos++] & 255;
}

void __ptitle(char *name, char *p)
{
    int n;
    n = 0;
    while (name[n] && n < 30) {
        p[n + 1] = name[n] >= 'a' && name[n] <= 'z' ? name[n] - 32 : name[n];
        n++;
    }
    p[0] = n;
}

/* a UCSD text file (2-block header, 1K pages, DLE blank compression):
   NAME.TEXT, and C sources NAME.C and headers NAME.H */
int __istext(char *name)
{
    int n;
    char *s;
    n = strlen(name);
    if (n >= 2) {
        s = name + n - 2;
        if (s[0] == '.' && ((s[1] | 32) == 'c' || (s[1] | 32) == 'h'))
            return 1;
    }
    if (n < 5)
        return 0;
    s = name + n - 5;
    return s[0] == '.' && (s[1] | 32) == 't' && (s[2] | 32) == 'e' && (s[3] | 32) == 'x' && (s[4] | 32) == 't';
}

int __blockio(FILE *f, int nblocks, int block, int doread)
{
    int n;
    n = __cxp0i(28, f->fib, f->buf, 0, nblocks, block, doread, 0, 0);
    if (__cspi(34) != 0)
        f->flags = f->flags | __F_ERR;
    return n;
}

int __flushbuf(FILE *f)
{
    int n;
    if (!(f->flags & __F_DIRTY))
        return 0;
    f->flags = f->flags & ~__F_DIRTY;
    if (f->flags & __F_TEXT) {
        memset(f->buf + f->pos, 0, 1024 - f->pos);
        n = __blockio(f, 2, f->blk, 0);
        return n == 2 ? 0 : EOF;
    }
    n = __blockio(f, 1, f->blk, 0);
    return n == 1 ? 0 : EOF;
}

int __loadblock(FILE *f, int b)
{
    int n;
    if (f->blk == b)
        return 0;
    if (__flushbuf(f))
        return EOF;
    f->blk = b;
    f->len = 0;
    if (f->flags & __F_READ) {
        n = __blockio(f, 1, b, 1);
        f->len = n > 0 ? 512 : 0;
    }
    if (f->len == 0)
        memset(f->buf, 0, 512);
    return 0;
}

FILE *fopen(char *name, char *mode)
{
    FILE *f;
    char title[32];
    int i;
    int old;
    int n;
    for (i = 3; i < FOPEN_MAX + 3; i++)
        if (__files[i].flags == 0)
            break;
    if (i >= FOPEN_MAX + 3)
        return NULL;
    f = &__files[i];
    memset(f, 0, sizeof(FILE));
    f->ungot = -1;
    f->blk = -1;
    if (mode[0] == 'r')
        f->flags = __F_READ;
    else if (mode[0] == 'w' || mode[0] == 'a')
        f->flags = __F_WRITE;
    else
        return NULL;
    if (mode[1] == '+' || (mode[1] && mode[2] == '+'))
        f->flags = __F_READ | __F_WRITE;
    if (__istext(name))
        f->flags = f->flags | __F_TEXT;
    /* a text file is written a 1K page at a time (a line may not cross a
       page), but read a block at a time: half the memory for the buffer */
    f->bufsize = (f->flags & __F_TEXT) && (f->flags & __F_WRITE) ? 1024 : 512;
    f->buf = malloc(f->bufsize);
    f->fib = malloc(80);
    if (!f->buf || !f->fib) {
        if (f->buf)
            free(f->buf);
        f->flags = 0;
        return NULL;
    }
    __ptitle(name, title);
    __cxp0v(3, f->fib, 0, -1);                  /* FINIT(fib, NIL, untyped) */
    old = mode[0] == 'r' || mode[0] == 'a';
    __cxp0v(5, f->fib, title, old, 0);          /* FOPEN */
    if (__cspi(34) != 0) {
        free(f->buf);
        free(f->fib);
        f->flags = 0;
        return NULL;
    }
    if (!old)
        f->flags = f->flags | __F_NEW;
    if (f->flags & __F_TEXT) {
        if (!old) {
            memset(f->buf, 0, 1024);
            __blockio(f, 2, 0, 0);              /* the text file header */
        }
        f->blk = 2;
        f->pos = 0;
        f->len = 0;
        if (mode[0] == 'a') {
            /* find the last page and the end of its text */
            for (;;) {
                n = __blockio(f, 2, f->blk, 1);
                if (n < 2)
                    break;
                f->blk = f->blk + 2;
            }
            if (f->blk > 2) {
                f->blk = f->blk - 2;
                __blockio(f, 2, f->blk, 1);
                f->pos = 0;
                while (f->pos < 1024 && f->buf[f->pos])
                    f->pos++;
                f->linestart = f->pos;
            } else
                memset(f->buf, 0, 1024);
            f->flags = (f->flags | __F_DIRTY) & ~__F_READ;
        } else if (f->flags & __F_READ) {
            f->len = 0;
            f->pos = 0;
            f->blk = 2 - f->bufsize / 512;  /* the text starts at block 2 */
        }
    } else if (mode[0] == 'a') {
        n = 0;
        while (__blockio(f, 1, n, 1) == 1)
            n++;
        f->blk = -1;
        __loadblock(f, n);
        f->pos = 0;
    }
    return f;
}

int __textgetc(FILE *f)
{
    int c;
    int n;
    for (;;) {
        if (f->dle > 0) {
            f->dle--;
            return ' ';
        }
        if (f->pos >= f->len) {
            if (f->flags & __F_EOF)
                return EOF;
            n = f->bufsize / 512;           /* blocks per read: 1, or 2 (r+) */
            f->blk = f->blk + n;
            n = __blockio(f, n, f->blk, 1);
            if (n <= 0) {
                f->flags = f->flags | __F_EOF;
                f->len = 0;
                return EOF;
            }
            f->len = n * 512;
            f->pos = 0;
        }
        c = f->buf[f->pos++];
        if (f->dle < 0) {                   /* the count after a DLE (maybe */
            f->dle = c - 32;                /* in the next block) */
            continue;
        }
        if (c == 0) {
            f->pos = f->len;        /* the rest of the block is padding */
            continue;
        }
        if (c == 16) {
            f->dle = -1;
            continue;
        }
        if (c == 13)
            return '\n';
        return c;
    }
}

int fgetc(FILE *f)
{
    int c;
    if (f->ungot >= 0) {
        c = f->ungot;
        f->ungot = -1;
        return c;
    }
    if (f->flags & __F_CON) {
        if (f->flags & __F_EOF)
            return EOF;
        c = __congetc();
        if (c == EOF)
            f->flags = f->flags | __F_EOF;
        return c;
    }
    if (!(f->flags & __F_READ) || (f->flags & __F_EOF))
        return EOF;
    if (f->flags & __F_TEXT)
        return __textgetc(f);
    if (f->blk < 0 || f->pos >= 512) {
        if (__loadblock(f, f->blk < 0 ? 0 : f->blk + (f->pos >= 512)))
            return EOF;
        if (f->pos >= 512)
            f->pos = 0;
        if (f->len == 0) {
            f->flags = f->flags | __F_EOF;
            return EOF;
        }
    }
    return f->buf[f->pos++];
}

/* A text page is full (pos 1023): write it and begin the next one with the
   line not yet finished (no line crosses a page; one that fills a whole
   page is split).  When the line begins in the page's second block -- any
   line shorter than 512 -- the page goes out a block at a time and the line
   moves into the first block, already written: no memory needed, so a
   program that has used it all can still save its file.  A longer line is
   kept in a copy; without the memory for it the write fails (EOF), it never
   loses the line. */
static int __textpage(FILE *f)
{
    unsigned char *b;
    unsigned char *line;
    int l;
    int n;
    int ok;
    b = f->buf;
    l = f->linestart;
    if (l == 0)
        l = f->pos;
    n = f->pos - l;
    if (l >= 512) {
        f->flags = f->flags & ~__F_DIRTY;
        ok = __blockio(f, 1, f->blk, 0) == 1;
        memmove(b, b + l, n);
        memset(b + l, 0, 1024 - l);
        f->buf = b + 512;
        ok = ok && __blockio(f, 1, f->blk + 1, 0) == 1;
        f->buf = b;
        if (!ok)
            return EOF;
    } else {
        line = malloc(n);
        if (!line) {
            f->flags = f->flags | __F_ERR;
            return EOF;
        }
        memcpy(line, b + l, n);
        f->pos = l;
        f->flags = f->flags | __F_DIRTY;
        if (__flushbuf(f)) {
            free(line);
            return EOF;
        }
        memcpy(b, line, n);
        free(line);
    }
    f->blk = f->blk + 2;
    f->pos = n;
    f->linestart = 0;
    return 0;
}

int fputc(int c, FILE *f)
{
    if (f->flags & __F_CON) {
        __conputc(c);
        return c & 255;
    }
    if (!(f->flags & __F_WRITE))
        return EOF;
    if (f->flags & __F_TEXT) {
        if (c == '\n')
            c = 13;
        if (f->pos >= 1023 && __textpage(f))
            return EOF;
        f->buf[f->pos++] = c;
        if (c == 13)
            f->linestart = f->pos;
        f->flags = f->flags | __F_DIRTY;
        return c == 13 ? '\n' : c & 255;
    }
    if (f->blk < 0 || f->pos >= 512) {
        if (__loadblock(f, f->blk < 0 ? 0 : f->blk + (f->pos >= 512)))
            return EOF;
        if (f->pos >= 512)
            f->pos = 0;
    }
    f->buf[f->pos++] = c;
    if (f->pos > f->len)
        f->len = f->pos;
    f->flags = f->flags | __F_DIRTY;
    return c & 255;
}

int fflush(FILE *f)
{
    if (!f) {
        int i;
        for (i = 0; i < FOPEN_MAX + 3; i++)
            if (__files[i].flags)
                fflush(&__files[i]);
        return 0;
    }
    if (f->flags & __F_CON) {
        __conflush();
        return 0;
    }
    if (f->flags & __F_TEXT)
        return 0;               /* pages are written when full or on close */
    return __flushbuf(f);
}

int fclose(FILE *f)
{
    int r;
    if (!f || !f->flags)
        return EOF;
    if (f->flags & __F_CON) {
        __conflush();
        return 0;
    }
    r = __flushbuf(f);
    __cxp0v(6, f->fib, (f->flags & __F_NEW) ? 1 : 0);   /* FCLOSE, LOCK new files */
    if (__cspi(34) != 0)
        r = EOF;
    free(f->buf);
    free(f->fib);
    f->flags = 0;
    return r;
}

int remove(char *name)
{
    char fib[80];
    char title[32];
    __ptitle(name, title);
    __cxp0v(3, fib, 0, -1);
    __cxp0v(5, fib, title, 1, 0);
    if (__cspi(34) != 0)
        return -1;
    __cxp0v(6, fib, 2);                         /* FCLOSE(PURGE) */
    return __cspi(34) == 0 ? 0 : -1;
}

int feof(FILE *f)
{
    return (f->flags & __F_EOF) != 0 && f->ungot < 0;
}

int ferror(FILE *f)
{
    return (f->flags & __F_ERR) != 0;
}

void clearerr(FILE *f)
{
    f->flags = f->flags & ~(__F_EOF | __F_ERR);
}

int ungetc(int c, FILE *f)
{
    if (c == EOF)
        return EOF;
    f->ungot = c & 255;
    f->flags = f->flags & ~__F_EOF;
    return c;
}

int fseek(FILE *f, long off, int whence)
{
    long p;
    int n;
    if (f->flags & (__F_TEXT | __F_CON))
        return -1;
    f->ungot = -1;
    if (whence == SEEK_CUR)
        p = (f->blk < 0 ? 0L : (long)f->blk * 512) + f->pos + off;
    else if (whence == SEEK_END) {
        n = 0;
        while (__blockio(f, 1, n, 1) == 1)
            n++;
        f->blk = -1;
        p = (long)n * 512 + off;
    } else
        p = off;
    if (p < 0)
        return -1;
    if (__loadblock(f, (int)(p / 512)))
        return -1;
    f->pos = (int)(p % 512);
    f->flags = f->flags & ~__F_EOF;
    return 0;
}

long ftell(FILE *f)
{
    if (f->flags & (__F_TEXT | __F_CON))
        return -1L;
    if (f->blk < 0)
        return 0L;
    return (long)f->blk * 512 + f->pos;
}

void rewind(FILE *f)
{
    if (f->flags & __F_TEXT) {
        f->blk = 2 - f->bufsize / 512;
        f->pos = 0;
        f->len = 0;
        f->dle = 0;
        f->flags = f->flags & ~__F_EOF;
        return;
    }
    fseek(f, 0L, SEEK_SET);
}

size_t fread(void *p, size_t size, size_t n, FILE *f)
{
    unsigned char *b;
    size_t i;
    size_t total;
    int c;
    b = p;
    total = size * n;
    for (i = 0; i < total; i++) {
        c = fgetc(f);
        if (c == EOF)
            break;
        b[i] = c;
    }
    return size ? i / size : 0;
}

size_t fwrite(void *p, size_t size, size_t n, FILE *f)
{
    unsigned char *b;
    size_t i;
    size_t total;
    b = p;
    total = size * n;
    for (i = 0; i < total; i++)
        if (fputc(b[i], f) == EOF)
            break;
    return size ? i / size : 0;
}

int getc(FILE *f)
{
    return fgetc(f);
}

int putc(int c, FILE *f)
{
    return fputc(c, f);
}

int getchar(void)
{
    return fgetc(stdin);
}

int putchar(int c)
{
    return fputc(c, stdout);
}

char *fgets(char *s, int n, FILE *f)
{
    int i;
    int c;
    i = 0;
    while (i < n - 1) {
        c = fgetc(f);
        if (c == EOF)
            break;
        s[i++] = c;
        if (c == '\n')
            break;
    }
    if (i == 0)
        return NULL;
    s[i] = 0;
    return s;
}

char *gets(char *s)
{
    int i;
    int c;
    i = 0;
    for (;;) {
        c = fgetc(stdin);
        if (c == EOF) {
            if (i == 0)
                return NULL;
            break;
        }
        if (c == '\n')
            break;
        s[i++] = c;
    }
    s[i] = 0;
    return s;
}

int fputs(char *s, FILE *f)
{
    while (*s)
        if (fputc(*s++, f) == EOF)
            return EOF;
    return 0;
}

int puts(char *s)
{
    fputs(s, stdout);
    return fputc('\n', stdout);
}

void perror(char *s)
{
    if (s && *s) {
        fputs(s, stderr);
        fputs(": ", stderr);
    }
    fputs("I/O error\n", stderr);
}

void __stdio_exit(void)
{
    int i;
    for (i = 3; i < FOPEN_MAX + 3; i++)
        if (__files[i].flags)
            fclose(&__files[i]);
    __conflush();
}

/* printf's flags (%-0+ #, and l), one bit each in one word */
#define F_LEFT 1
#define F_ZERO 2
#define F_PLUS 4
#define F_SPACE 8
#define F_ALT 16
#define F_LONG 32

FILE *__of;
char *__os;
int __on;
void __oc(int c)
{
    if (__of)
        fputc(c, __of);
    else if (__os)
        *__os++ = c;
    __on++;
}

void __opad(int n, int c)
{
    while (n-- > 0)
        __oc(c);
}

/* the digits of v in base b, written backwards to end just before end:
   the first digit's address */
char *__udigits(unsigned long v, int base, int upper, char *end)
{
    int d;
    do {
        d = (int)(v % base);
        *--end = d < 10 ? '0' + d : (upper ? 'A' : 'a') + d - 10;
        v = v / base;
    } while (v != 0);
    return end;
}

/* a field: pre (a sign, 0x), zeros more '0's, then body's n characters,
   padded to width; fl: F_LEFT (pad after), F_ZERO (pad with '0's) */
void __ofield(char *pre, char *body, int n, int zeros, int width, int fl)
{
    int pad;
    pad = width - strlen(pre) - zeros - n;
    if (!(fl & (F_LEFT | F_ZERO)))
        __opad(pad, ' ');
    while (*pre)
        __oc(*pre++);
    if ((fl & (F_LEFT | F_ZERO)) == F_ZERO)
        __opad(pad, '0');
    __opad(zeros, '0');
    while (n-- > 0)
        __oc(*body++);
    if (fl & F_LEFT)
        __opad(pad, ' ');
}

/* a number's sign: "-", or the "+" or " " the flags ask for */
char *__osign(int neg, int fl)
{
    if (neg)
        return "-";
    if (fl & F_PLUS)
        return "+";
    if (fl & F_SPACE)
        return " ";
    return "";
}

/* float formatting lives in fltfmt.c: it is linked (and installs itself
   here) only in programs that use floating point */
int (*__fltfmt)(float v, int prec, int style, int alt, char *out);

#ifndef NO_FLOAT_PRINTF
/* %f %e %g %E %G (c): a procedure of its own, so that its buffer and
   numbers are on the stack only while a number is formatted; *ap is
   moved past the argument (a double with F_LONG) */
void __ofloat(int c, int fl, int width, int prec, va_list *ap)
{
    char buf[48];
    char *s;
    int n;
    int i;
    int neg;
    float dv;
    double dd;
    s = buf;
    if (fl & F_LONG) {
        /* a double (8 bytes): the engine formats it (CSP 133,
           P-Code mode); %g's default precision is 10, not 6 */
        dd = va_arg(*ap, double);
        if (prec < 0)
            prec = c == 'g' || c == 'G' ? 10 : 6;
        __cspv(133, dd, c == 'E' ? 'e' : (c == 'G' ? 'g' : c), prec, buf);
        neg = *s == '-';
        if (neg)
            s++;
        n = strlen(s);
    } else {
        dv = va_arg(*ap, float);
        neg = dv < 0.0;
        if (neg)
            dv = -dv;
        if (__fltfmt)
            n = __fltfmt(dv, prec < 0 ? 6 : prec, c == 'E' ? 'e' : (c == 'G' ? 'g' : c), (fl & F_ALT) != 0, buf);
        else {
            buf[0] = '?';
            n = 1;
        }
    }
    if (c == 'E' || c == 'G')
        for (i = 0; i < n; i++)
            s[i] = toupper(s[i]);
    __ofield(__osign(neg, fl), s, n, 0, width, fl);
}
#endif

int __vformat(char *fmt, va_list ap)
{
    char buf[12];                       /* a long's digits: 11 in octal */
    char *s;
    int fl;
    int width;
    int prec;
    int c;
    int n;
    long lv;
    __on = 0;
    while ((c = *fmt++) != 0) {
        if (c != '%') {
            __oc(c);
            continue;
        }
        fl = 0;
        for (;;) {
            c = *fmt;
            if (c == '-')
                fl = fl | F_LEFT;
            else if (c == '0')
                fl = fl | F_ZERO;
            else if (c == '+')
                fl = fl | F_PLUS;
            else if (c == ' ')
                fl = fl | F_SPACE;
            else if (c == '#')
                fl = fl | F_ALT;
            else
                break;
            fmt++;
        }
        width = 0;
        if (*fmt == '*') {
            width = va_arg(ap, int);
            fmt++;
            if (width < 0) {
                fl = fl | F_LEFT;
                width = -width;
            }
        } else
            while (*fmt >= '0' && *fmt <= '9')
                width = width * 10 + *fmt++ - '0';
        prec = -1;
        if (*fmt == '.') {
            fmt++;
            prec = 0;
            if (*fmt == '*') {
                prec = va_arg(ap, int);
                fmt++;
            } else
                while (*fmt >= '0' && *fmt <= '9')
                    prec = prec * 10 + *fmt++ - '0';
        }
        while (*fmt == 'l' || *fmt == 'h' || *fmt == 'L') {
            if (*fmt == 'l' || *fmt == 'L')
                fl = fl | F_LONG;       /* %ld; %lf %Lf: a double */
            fmt++;
        }
        c = *fmt++;
        switch (c) {
        case 'd':
        case 'i':
            lv = fl & F_LONG ? va_arg(ap, long) : va_arg(ap, int);
            s = __udigits(lv < 0 ? -lv : lv, 10, 0, buf + 12);
            n = buf + 12 - s;
            if (prec >= 0)
                fl = fl & ~F_ZERO;
            __ofield(__osign(lv < 0, fl), s, n, prec > n ? prec - n : 0, width, fl);
            break;
        case 'u':
        case 'x':
        case 'X':
        case 'o':
            lv = fl & F_LONG ? va_arg(ap, unsigned long) : va_arg(ap, unsigned);
            s = __udigits(lv, c == 'u' ? 10 : (c == 'o' ? 8 : 16), c == 'X', buf + 12);
            n = buf + 12 - s;
            if (prec >= 0)
                fl = fl & ~F_ZERO;
            __ofield(!(fl & F_ALT) || c == 'u' ? "" : (c == 'o' ? "0"
                     : (lv == 0 ? "" : (c == 'X' ? "0X" : "0x"))),
                     s, n, prec > n ? prec - n : 0, width, fl);
            break;
        case 'p':
            s = __udigits(va_arg(ap, unsigned), 16, 0, buf + 12);
            n = buf + 12 - s;
            __ofield("", s, n, 4 - n, width, fl & F_LEFT);
            break;
        case 'c':
            buf[0] = va_arg(ap, int);
            __ofield("", buf, 1, 0, width, fl & F_LEFT);
            break;
        case 's':
            s = va_arg(ap, char *);
            if (!s)
                s = "(null)";
            n = strlen(s);
            if (prec >= 0 && n > prec)
                n = prec;
            __ofield("", s, n, 0, width, fl & F_LEFT);
            break;
#ifndef NO_FLOAT_PRINTF
        case 'f':
        case 'e':
        case 'E':
        case 'g':
        case 'G':
            __ofloat(c, fl, width, prec, &ap);
            break;
#endif
        case 'n':
            *va_arg(ap, int *) = __on;
            break;
        case '%':
            __oc('%');
            break;
        default:
            __oc('%');
            if (c)
                __oc(c);
            else
                fmt--;
            break;
        }
    }
    return __on;
}

int vfprintf(FILE *f, char *fmt, va_list ap)
{
    __of = f;
    __os = NULL;
    return __vformat(fmt, ap);
}

int vsprintf(char *s, char *fmt, va_list ap)
{
    int n;
    __of = NULL;
    __os = s;
    n = __vformat(fmt, ap);
    *__os = 0;
    return n;
}

int printf(char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    return vfprintf(stdout, fmt, ap);
}

int fprintf(FILE *f, char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    return vfprintf(f, fmt, ap);
}

int sprintf(char *s, char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    return vsprintf(s, fmt, ap);
}

FILE *__if;
char *__is;
int __in;
int __ic(void)
{
    int c;
    if (__if)
        c = fgetc(__if);
    else {
        c = *__is ? *__is++ & 255 : EOF;
    }
    if (c != EOF)
        __in++;
    return c;
}

void __iunc(int c)
{
    if (c == EOF)
        return;
    __in--;
    if (__if)
        ungetc(c, __if);
    else
        __is--;
}

/* scanf's %f: keep the character c (up to 40) and read the next */
static int __nc(int c, char *nb, int *nn)
{
    if (*nn < 40)
        nb[(*nn)++] = c;
    return __ic();
}

int __vscan(char *fmt, va_list ap)
{
    int count;
    int c;
    int f;
    int width;
    int lng;
    int skip;
    int neg;
    int base;
    int d;
    int any;
    long v;
    float dv;
    float sc;
    char *s;
    char nb[41];
    int nn;
    count = 0;
    __in = 0;
    while ((f = *fmt++) != 0) {
        if (f == ' ' || f == '\t' || f == '\n') {
            c = __ic();
            while (c == ' ' || c == '\t' || c == '\n')
                c = __ic();
            __iunc(c);
            continue;
        }
        if (f != '%') {
            c = __ic();
            if (c != f) {
                __iunc(c);
                return count ? count : (c == EOF ? EOF : 0);
            }
            continue;
        }
        skip = 0;
        if (*fmt == '*') {
            skip = 1;
            fmt++;
        }
        width = 0;
        while (*fmt >= '0' && *fmt <= '9')
            width = width * 10 + *fmt++ - '0';
        if (width == 0)
            width = 32767;
        lng = 0;
        while (*fmt == 'l' || *fmt == 'h' || *fmt == 'L') {
            if (*fmt == 'l' || *fmt == 'L')
                lng = 1;
            fmt++;
        }
        f = *fmt++;
        if (f == '%') {
            c = __ic();
            if (c != '%')
                return count;
            continue;
        }
        if (f == 'n') {
            if (!skip)
                *va_arg(ap, int *) = __in;
            continue;
        }
        c = __ic();
        if (f != 'c')
            while (c == ' ' || c == '\t' || c == '\n')
                c = __ic();
        if (c == EOF)
            return count ? count : EOF;
        if (f == 'c') {
            if (!skip) {
                s = va_arg(ap, char *);
                *s = c;
                count++;
            }
            continue;
        }
        if (f == 's') {
            s = skip ? NULL : va_arg(ap, char *);
            while (c != EOF && c != ' ' && c != '\t' && c != '\n' && width-- > 0) {
                if (s)
                    *s++ = c;
                c = __ic();
            }
            __iunc(c);
            if (s) {
                *s = 0;
                count++;
            }
            continue;
        }
        if (f == 'd' || f == 'i' || f == 'u' || f == 'x' || f == 'X' || f == 'o') {
            neg = 0;
            if (c == '-' || c == '+') {
                neg = c == '-';
                c = __ic();
                width--;
            }
            base = f == 'x' || f == 'X' ? 16 : (f == 'o' ? 8 : 10);
            v = 0;
            any = 0;
            while (width-- > 0) {
                if (c >= '0' && c <= '9')
                    d = c - '0';
                else if (c >= 'a' && c <= 'f')
                    d = c - 'a' + 10;
                else if (c >= 'A' && c <= 'F')
                    d = c - 'A' + 10;
                else
                    break;
                if (d >= base)
                    break;
                v = v * base + d;
                any = 1;
                c = __ic();
            }
            __iunc(c);
            if (!any)
                return count;
            if (neg)
                v = -v;
            if (!skip) {
                if (lng)
                    *va_arg(ap, long *) = v;
                else
                    *va_arg(ap, int *) = (int)v;
                count++;
            }
            continue;
        }
        if (f == 'f' || f == 'e' || f == 'g') {
            neg = 0;
            nn = 0;
            if (c == '-' || c == '+') {
                neg = c == '-';
                c = __nc(c, nb, &nn);
            }
            dv = 0.0;
            any = 0;
            while (c >= '0' && c <= '9') {
                dv = dv * 10.0 + (c - '0');
                any = 1;
                c = __nc(c, nb, &nn);
            }
            if (c == '.') {
                c = __nc(c, nb, &nn);
                sc = 0.1;
                while (c >= '0' && c <= '9') {
                    dv = dv + (c - '0') * sc;
                    sc = sc / 10.0;
                    any = 1;
                    c = __nc(c, nb, &nn);
                }
            }
            if (any && (c == 'e' || c == 'E')) {
                int e;
                int en;
                c = __nc(c, nb, &nn);
                en = 0;
                if (c == '-' || c == '+') {
                    en = c == '-';
                    c = __nc(c, nb, &nn);
                }
                e = 0;
                while (c >= '0' && c <= '9') {
                    e = e * 10 + c - '0';
                    c = __nc(c, nb, &nn);
                }
                while (e-- > 0)
                    dv = en ? dv / 10.0 : dv * 10.0;
            }
            __iunc(c);
            if (!any)
                return count;
            if (neg)
                dv = -dv;
            if (!skip) {
                if (lng) {
                    nb[nn] = 0;         /* %lf %Lf: the text read -> double */
                    __cspi(135, nb, va_arg(ap, double *));
                } else
                    *va_arg(ap, float *) = dv;
                count++;
            }
            continue;
        }
        return count;
    }
    return count;
}

int vfscanf(FILE *f, char *fmt, va_list ap)
{
    __if = f;
    __is = NULL;
    return __vscan(fmt, ap);
}

int scanf(char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    return vfscanf(stdin, fmt, ap);
}

int fscanf(FILE *f, char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    return vfscanf(f, fmt, ap);
}

int sscanf(char *s, char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    __if = NULL;
    __is = s;
    return __vscan(fmt, ap);
}

FILE __files[FOPEN_MAX + 3] = { { 9, -1, 0, 0, -1 }, { 10, -1, 0, 0, -1 }, { 10, -1, 0, 0, -1 } };
