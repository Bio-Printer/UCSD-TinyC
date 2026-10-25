/* cat.c -- CAT [-AbeEnstTuv] [FILE ...]: the files, one after the other, on the
   standard output; no FILE or a FILE "-" is the standard input.  The options of
   cat(1) (the long ones, --show-all and so on, are not here):

     -A  show all: -vET           -b  number the non-empty output lines (beats -n)
     -e  -vE                      -E  a $ at the end of each line
     -n  number all output lines  -s  one empty line for several in a row
     -t  -vT                      -T  show the tabs as ^I
     -u  ignored                  -v  show control characters as ^X (DEL as ^?)
                                      and bytes of 128 and up as M-x (M-^X, M-^?)
   Several options in one word (-bn, -vET), -- ends the options.  Line numbers are
   6 wide, then a tab, and go on from one file to the next.

   .TEXT, .C, .H and .BAS files are text files (a line is what ends in a CR on the disk,
   the DLE blanks are put back); any other file is read as the bytes it holds,
   whole 512-byte blocks.  Run it from the shell: CAT -n A.C, CAT A.C B.C >ALL.TEXT,
   DIR | CAT -n. */
#pragma segment MAIN	/* calls within the program: 2-byte CGPs, no NOP after them */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int optb, optn, opts, optE, optT, optv;
long lineno = 1;
int startline = 1;                      /* the next character starts an output line */
int prevempty;                          /* the line before was an empty one */
char obuf[1100];
int on;
char ibuf[512];

void flush(void)
{
    if (on) {
        fwrite_blk(obuf, 1, on, stdout);
        on = 0;
    }
}

void put(int c)
{
    if (on >= 1090)
        flush();
    obuf[on++] = c;
}

void number(void)
{
    char t[16];
    char *p;
    sprintf(t, "%6ld\t", lineno++);
    for (p = t; *p; p++)
        put(*p);
}

/* a character that is not a newline, as the options want it shown */
void show(int c)
{
    int meta;
    meta = 0;
    c = c & 255;
    if (optv && c >= 128) {
        put('M');
        put('-');
        c = c - 128;
        meta = 1;
    }
    if (c == '\t') {
        if (optT || meta) {
            put('^');
            put('I');
        } else
            put('\t');
        return;
    }
    if (optv && c < 32) {
        put('^');
        put(c + 64);
        return;
    }
    if (optv && c == 127) {
        put('^');
        put('?');
        return;
    }
    put(c);
}

void emit(int c)
{
    if (startline) {
        if (c == '\n') {                /* an empty line */
            if (opts && prevempty)
                return;
            prevempty = 1;
            if (optn && !optb)
                number();
            if (optE)
                put('$');
            put('\n');
            return;
        }
        prevempty = 0;
        if (optn || optb)
            number();
        startline = 0;
    }
    if (c == '\n') {
        if (optE)
            put('$');
        put('\n');
        startline = 1;
        return;
    }
    show(c);
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

int catfile(char *name)
{
    FILE *f;
    int n, i, plain;
    plain = !(optb || optn || opts || optE || optT || optv);
    if (strcmp(name, "-") == 0)
        f = stdin;
    else {
        f = fopen(name, "r");
        if (!f) {
            flush();
            fprintf(stderr, "cat: %s: cannot open it\n", name);
            return 1;
        }
    }
    while ((n = more(f)) > 0) {
        if (plain) {
            flush();
            fwrite_blk(ibuf, 1, n, stdout);
        } else
            for (i = 0; i < n; i++)
                emit(ibuf[i] & 255);
    }
    flush();
    if (f != stdin)
        fclose(f);
    return 0;
}

int isopt(char *a, int endopt)
{
    return !endopt && a[0] == '-' && a[1] != 0;
}

int main(int argc, char **argv)
{
    int i, j, endopt, nf, status;
    char *a;
    endopt = 0;
    nf = 0;
    status = 0;
    for (i = 1; i < argc; i++) {        /* the options */
        a = argv[i];
        if (!isopt(a, endopt))
            continue;
        if (a[1] == '-' && a[2] == 0) {
            endopt = 1;
            continue;
        }
        for (j = 1; a[j]; j++)
            switch (a[j]) {
            case 'A': optv = optE = optT = 1; break;
            case 'b': optb = 1; break;
            case 'e': optv = optE = 1; break;
            case 'E': optE = 1; break;
            case 'n': optn = 1; break;
            case 's': opts = 1; break;
            case 't': optv = optT = 1; break;
            case 'T': optT = 1; break;
            case 'u': break;
            case 'v': optv = 1; break;
            default:
                fprintf(stderr, "cat: invalid option -- '%c'\n", a[j]);
                fprintf(stderr, "use: CAT [-AbeEnstTuv] [FILE ...]   (no FILE or -: the standard input)\n");
                return 1;
            }
    }
    endopt = 0;
    for (i = 1; i < argc; i++) {        /* the files */
        a = argv[i];
        if (isopt(a, endopt)) {
            if (a[1] == '-' && a[2] == 0)
                endopt = 1;
            continue;
        }
        nf++;
        status = status | catfile(a);
    }
    if (nf == 0)
        status = catfile("-");
    return status;
}
