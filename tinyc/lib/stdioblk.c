/* stdioblk.c -- Tiny-C library: fread and fwrite_blk a block at a time (their own
   module: a program that does not call them, the compiler for one, does not carry them) */
#include "libint.h"
#pragma nofltused

/* the displacement of the first byte equal to c in n bytes at p, n if none
   (the P-System's SCAN); n is at most 32767 */
static int __scan(unsigned char *p, int n, int c)
{
    return __cspi(11, n, 0, c, p, 0, 0);
}

/* A run of bytes from the block in memory (no reading, no seeking), or 0 if
   the next byte is for the one-at-a-time path (fgetc: block to load, end of
   the file, DLE, NUL).  Binary: what is left of the block.  Text: plain
   characters up to the line's CR, and the CR as '\n' (with stoplf, a LF in
   the text also ends the run, after it). */
static int __readrun(FILE *f, unsigned char *dst, int want, int stoplf)
{
    int r;
    int d;
    int n;
    unsigned char *s;
    if (f->ungot >= 0 || (f->flags & (__F_EOF | __F_CON)) || !(f->flags & __F_READ) || want <= 0)
        return 0;
    if (!(f->flags & __F_TEXT)) {
        if (f->blk < 0 || f->pos >= 512 || f->len == 0)
            return 0;
        n = 512 - f->pos;
        if (n > want)
            n = want;
        memcpy(dst, f->buf + f->pos, n);
        f->pos = f->pos + n;
        return n;
    }
    if (f->dle != 0 || f->pos >= f->len)
        return 0;
    s = f->buf + f->pos;
    n = f->len - f->pos;
    r = __scan(s, n, 13);                   /* the plain run, then CR */
    d = __scan(s, r, 16);
    if (d < r)
        r = d;
    d = __scan(s, r, 0);
    if (d < r)
        r = d;
    if (stoplf) {
        d = __scan(s, r, 10);
        if (d < r) {
            r = d + 1;                      /* through the LF */
            if (r > want)
                r = want;
            memcpy(dst, s, r);
            f->pos = f->pos + r;
            return r;
        }
    }
    if (r > want)
        r = want;
    if (r > 0)
        memcpy(dst, s, r);
    if (r < want && r < n && s[r] == 13) {  /* the CR that ends the line */
        dst[r++] = '\n';
        f->pos = f->pos + r;
        return r;
    }
    f->pos = f->pos + r;
    return r;
}

size_t fread(void *p, size_t size, size_t n, FILE *f)
{
    unsigned char *b;
    size_t i;
    size_t total;
    int c;
    int k;
    b = p;
    total = size * n;
    i = 0;
    while (i < total) {
        k = __readrun(f, b + i, total - i, 0);
        if (k > 0) {
            i = i + k;
            continue;
        }
        c = fgetc(f);
        if (c == EOF)
            break;
        b[i++] = c;
    }
    return size ? i / size : 0;
}


/* fwrite for vi (vi.h: #define fwrite fwrite_blk): to the console the bytes up to a LF at once,
   to a binary file a block at a time */
size_t fwrite_blk(void *p, size_t size, size_t n, FILE *f)
{
    unsigned char *b;
    size_t i;
    size_t total;
    int r;
    int m;
    b = p;
    total = size * n;
    i = 0;
    if (f->flags & __F_CON) {
        while (i < total) {
            m = total - i > 32767 ? 32767 : total - i;
            r = __scan(b + i, m, 10);
            if (r > 80 - __conlen)
                r = 80 - __conlen;
            if (r > 0) {
                memcpy(__conout + __conlen, b + i, r);
                __conlen = __conlen + r;
                i = i + r;
                if (__conlen >= 80)
                    __conflush();
            } else {
                __conputc(b[i++]);          /* the LF: a CR, and a flush */
            }
        }
    } else if ((f->flags & (__F_WRITE | __F_TEXT)) == __F_WRITE) {
        while (i < total) {
            if (f->blk < 0 || f->pos >= 512) {
                if (__loadblock(f, f->blk < 0 ? 0 : f->blk + (f->pos >= 512)))
                    break;
                if (f->pos >= 512)
                    f->pos = 0;
            }
            m = 512 - f->pos;
            if (m > total - i)
                m = total - i;
            memcpy(f->buf + f->pos, b + i, m);
            f->pos = f->pos + m;
            if (f->pos > f->len)
                f->len = f->pos;
            f->flags = f->flags | __F_DIRTY;
            i = i + m;
        }
    } else {
        for (; i < total; i++)
            if (fputc(b[i], f) == EOF)
                break;
    }
    return size ? i / size : 0;
}

