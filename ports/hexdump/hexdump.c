/* hexdump.c -- HEXDUMP [-i] [FILE ...]: the bytes of the files (one after the other, as one
   stream; no FILE or "-": the standard input) as a hex dump:

     00000000  48 65 6C 6C 6F 20 77 6F   72 6C 64 0A 00 01 02 03  Hello world.....

   the offset in hex, 16 bytes in hex with a blank between them (and two more after the 8th,
   to read them in two groups), then the same bytes as characters, a . for the ones that are
   not printable; the last line is filled up with blanks so the characters line up.

   -i: Intel Hex, a record a line:  :LLAAAATTDD...CC  (LL the data bytes, AAAA the address in
   the 64 KB segment, TT 00 data, 04 extended linear address (every 64 KB), 01 end of file; CC
   the checksum: the two's complement of the bytes' sum), 16 data bytes a record, and the end
   record :00000001FF.

   .TEXT, .C and .H files are text (a line ends in a CR on the disk and is a 0A here, the DLE
   blanks are put back); any other file is read as the bytes it holds, whole 512-byte blocks.
   Run it from the shell: HEXDUMP A.CODE, HEXDUMP -i A.CODE >A.HEX, HEXDUMP A.C | GREP 0D. */
#pragma segment MAIN	/* calls within the program: 2-byte CGPs, no NOP after them */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int intel;
long offset;                            /* of the next byte in the stream */
unsigned lo;                            /* the address in its 64 KB segment (Intel Hex) */
int seg;                                /* the 64 KB segment (Intel Hex) */
unsigned char row[16];
int nrow;
char ibuf[512];
char line[100];

char *hexdigits = "0123456789ABCDEF";

/* two hex digits of b at p; returns after them */
char *hex2(char *p, int b)
{
    *p++ = hexdigits[(b >> 4) & 15];
    *p++ = hexdigits[b & 15];
    return p;
}

/* the Intel Hex record: type, address, data (n bytes at d) */
void record(int type, unsigned addr, unsigned char *d, int n)
{
    char *p;
    int i, sum;
    p = line;
    *p++ = ':';
    sum = n + (addr >> 8) + (addr & 255) + type;
    p = hex2(p, n);
    p = hex2(p, addr >> 8);
    p = hex2(p, addr & 255);
    p = hex2(p, type);
    for (i = 0; i < n; i++) {
        p = hex2(p, d[i]);
        sum = sum + d[i];
    }
    p = hex2(p, (256 - (sum & 255)) & 255);
    *p = 0;
    puts(line);
}

/* the row of nrow bytes, as a line of the dump (the last row may be short) */
void flushrow(void)
{
    char *p;
    int i, c;
    unsigned char seg4[2];
    if (nrow == 0)
        return;
    if (intel) {
        if (lo == 0 && seg > 0) {       /* the first row of the next 64 KB */
            seg4[0] = seg >> 8;
            seg4[1] = seg & 255;
            record(4, 0, seg4, 2);
        }
        record(0, lo, row, nrow);
        lo = lo + nrow;
        if (lo == 0)
            seg++;                      /* the address wrapped: the next segment */
    } else {
        sprintf(line, "%08lX  ", offset);
        p = line + 10;
        for (i = 0; i < 16; i++) {
            if (i < nrow)
                p = hex2(p, row[i]);
            else {
                *p++ = ' ';
                *p++ = ' ';
            }
            *p++ = ' ';
            if (i == 7) {
                *p++ = ' ';
                *p++ = ' ';
            }
        }
        *p++ = ' ';
        for (i = 0; i < nrow; i++) {
            c = row[i];
            *p++ = c >= 32 && c < 127 ? c : '.';
        }
        *p = 0;
        puts(line);
    }
    offset = offset + nrow;
    nrow = 0;
}

void byte(int c)
{
    row[nrow++] = c;
    if (nrow == 16)
        flushrow();
}

/* the next bytes of f into ibuf (a line at most from the keyboard); 0 at the end */
int more(FILE *f)
{
    int n, c;
    if (f->flags & __F_CON) {
        n = 0;
        while (n < 512 && (c = fgetc(f)) != EOF) {
            ibuf[n++] = c;
            if (c == '\n')
                break;
        }
        return n;
    }
    return fread(ibuf, 1, 512, f);
}

int dumpfile(char *name)
{
    FILE *f;
    int n, i;
    if (strcmp(name, "-") == 0)
        f = stdin;
    else {
        f = fopen(name, "r");
        if (!f) {
            fprintf(stderr, "hexdump: %s: cannot open it\n", name);
            return 1;
        }
    }
    while ((n = more(f)) > 0)
        for (i = 0; i < n; i++)
            byte(ibuf[i] & 255);
    if (f != stdin)
        fclose(f);
    return 0;
}

int main(int argc, char **argv)
{
    int i, nf, status;
    char *a;
    nf = 0;
    status = 0;
    for (i = 1; i < argc; i++) {
        a = argv[i];
        if (strcmp(a, "-i") == 0 || strcmp(a, "-I") == 0)
            intel = 1;
        else if (a[0] == '-' && a[1] != 0) {
            fprintf(stderr, "hexdump: invalid option -- '%s'\n", a + 1);
            fprintf(stderr, "use: HEXDUMP [-i] [FILE ...]   (-i: Intel Hex; no FILE or -: the standard input)\n");
            return 1;
        }
    }
    for (i = 1; i < argc; i++) {
        a = argv[i];
        if (a[0] == '-' && a[1] != 0)
            continue;
        nf++;
        status = status | dumpfile(a);
    }
    if (nf == 0)
        status = dumpfile("-");
    flushrow();
    if (intel)
        puts(":00000001FF");
    return status;
}
