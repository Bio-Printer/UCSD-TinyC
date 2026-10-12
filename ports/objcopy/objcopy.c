/* objcopy.c -- OBJCOPY [-I ihex|hexdump] [-O binary] [INFILE [OUTFILE]]: a HEXDUMP listing (or
   Intel Hex) back into the bytes it describes.

   -I hexdump  the lines of HEXDUMP (and of other hex dumps in that style):  the offset in hex, then
               up to 16 bytes as two hex digits with blanks between (more blanks after the 8th may
               be); what follows the last byte (the characters) is ignored.  The offsets place the
               bytes: a gap is filled with 0 bytes, going back is an error.  The first offset is
               where the file starts (it is not written as zeros).
   -I ihex     Intel Hex records :LLAAAATTDD..CC (data 00, end 01, extended segment 02, extended
               linear address 04, start addresses 03 and 05 ignored), the checksums checked; the
               addresses place the bytes as above (the lowest one is where the file starts).
   without -I  the first non-blank character decides: ':' is Intel Hex.
   -O binary   the only output format (the default).

   No INFILE (or -): the standard input; no OUTFILE: the standard output.  A file that is not
   .TEXT, .C or .H is bytes in whole 512-byte blocks: the UCSD file system does not keep the length,
   so the last block is filled up with 0 bytes.  Run it from the shell:
   OBJCOPY A.HEX A.CODE, HEXDUMP A.CODE | OBJCOPY - B.CODE. */
#pragma segment MAIN	/* calls within the program: 2-byte CGPs, no NOP after them */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

FILE *out;
long pos;                               /* the address of the next byte to write */
long base;                              /* the address of the first byte: the file's start */
int started;
long lineno;
int failed;
char line[300];
char obuf[512];
int on;

void flush(void)
{
    if (on) {
        fwrite_blk(obuf, 1, on, out);
        on = 0;
    }
}

void put(int c)
{
    if (on >= 512)
        flush();
    obuf[on++] = c;
}

void fail(char *msg)
{
    fprintf(stderr, "objcopy: line %ld: %s\n", lineno, msg);
    failed = 1;
}

/* the value of a hex digit, -1 if c is none */
int hexv(int c)
{
    if (c >= '0' && c <= '9')
        return c - '0';
    c = c | 32;
    if (c >= 'a' && c <= 'f')
        return c - 'a' + 10;
    return -1;
}

/* n bytes at addr */
void emit(long addr, unsigned char *d, int n)
{
    int i;
    if (!started) {
        started = 1;
        base = pos = addr;
    }
    if (addr < pos) {
        fail("an address below the one before it");
        return;
    }
    if (addr - pos > 1048576L) {
        fail("a gap of more than 1 MB");
        return;
    }
    for (; pos < addr; pos++)
        put(0);
    for (i = 0; i < n; i++)
        put(d[i]);
    pos = pos + n;
}

/* a line of hexdump: its bytes, placed by the offset */
void hexline(char *s)
{
    long addr;
    int v, i, n, a, b, gap;
    unsigned char d[16];
    char *p;
    p = s;
    while (*p == ' ')
        p++;
    if (hexv(*p) < 0)
        return;                         /* not a dump line: ignored */
    addr = 0;
    while ((v = hexv(*p)) >= 0) {
        addr = addr * 16 + v;
        p++;
    }
    if (*p == ':')
        p++;
    n = 0;
    for (i = 0; i < 16; i++) {
        gap = i == 0 ? 8 : (i == 8 ? 3 : 1);    /* the blanks before a byte (at most) */
        v = 0;
        while (*p == ' ' && v < gap) {
            p++;
            v++;
        }
        if (v == 0 || *p == 0)
            break;
        a = hexv(p[0]);
        b = a < 0 ? -1 : hexv(p[1]);
        if (b < 0 || (p[2] != ' ' && p[2] != 0 && p[2] != '|' && p[2] != '\n'))
            break;
        d[n++] = a * 16 + b;
        p = p + 2;
    }
    if (n)
        emit(addr, d, n);
}

long segbase;                           /* the extended segment / linear address, added to AAAA */

/* a line of Intel Hex */
int ihexline(char *s, int *done)
{
    unsigned char r[300];
    int n, i, v, w, sum;
    char *p;
    p = s;
    while (*p == ' ')
        p++;
    if (*p != ':')
        return 0;
    p++;
    n = 0;
    while ((v = hexv(p[0])) >= 0 && (w = hexv(p[1])) >= 0 && n < 270) {
        r[n++] = v * 16 + w;
        p = p + 2;
    }
    if (n < 5 || r[0] + 5 != n) {
        fail("a record of the wrong length");
        return 1;
    }
    sum = 0;
    for (i = 0; i < n; i++)
        sum = sum + r[i];
    if ((sum & 255) != 0) {
        fail("bad checksum");
        return 1;
    }
    switch (r[3]) {
    case 0:
        emit(segbase + r[1] * 256L + r[2], r + 4, r[0]);
        break;
    case 1:
        *done = 1;
        break;
    case 2:
        segbase = (r[4] * 256L + r[5]) * 16L;
        break;
    case 4:
        segbase = (r[4] * 256L + r[5]) * 65536L;
        break;
    }
    return 1;
}

int main(int argc, char **argv)
{
    FILE *in;
    int i, nf, fmt, done, c;
    char *names[2];
    char *a;
    fmt = 0;
    nf = 0;
    for (i = 1; i < argc; i++) {
        a = argv[i];
        if (strcmp(a, "-I") == 0 && i + 1 < argc) {
            a = argv[++i];
            if (strcmp(a, "ihex") == 0)
                fmt = 1;
            else if (strcmp(a, "hexdump") == 0 || strcmp(a, "hex") == 0)
                fmt = 2;
            else {
                fprintf(stderr, "objcopy: unknown input format %s (ihex, hexdump)\n", a);
                return 1;
            }
        } else if (strcmp(a, "-O") == 0 && i + 1 < argc) {
            if (strcmp(argv[++i], "binary") != 0) {
                fprintf(stderr, "objcopy: the only output format is binary\n");
                return 1;
            }
        } else if (a[0] == '-' && a[1] != 0) {
            fprintf(stderr, "use: OBJCOPY [-I ihex|hexdump] [-O binary] [INFILE [OUTFILE]]\n");
            return 1;
        } else if (nf < 2)
            names[nf++] = a;
        else {
            fprintf(stderr, "objcopy: too many files\n");
            return 1;
        }
    }
    in = stdin;
    if (nf > 0 && strcmp(names[0], "-") != 0) {
        in = fopen(names[0], "r");
        if (!in) {
            fprintf(stderr, "objcopy: %s: cannot open it\n", names[0]);
            return 1;
        }
    }
    out = stdout;
    if (nf > 1) {
        out = fopen(names[1], "w");
        if (!out) {
            fprintf(stderr, "objcopy: %s: cannot open it\n", names[1]);
            return 1;
        }
    }
    done = 0;
    while (!done && !failed && fgets(line, 299, in)) {
        lineno++;
        c = strlen(line);
        while (c > 0 && (line[c - 1] == '\n' || line[c - 1] == '\r' || line[c - 1] == ' '))
            line[--c] = 0;
        if (fmt == 0) {
            for (c = 0; line[c] == ' '; c++)
                ;
            if (line[c] == 0)
                continue;
            fmt = line[c] == ':' ? 1 : 2;
        }
        if (fmt == 1)
            ihexline(line, &done);
        else
            hexline(line);
    }
    flush();
    if (out != stdout)
        fclose(out);
    if (in != stdin)
        fclose(in);
    return failed;
}
