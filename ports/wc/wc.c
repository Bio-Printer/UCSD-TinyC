/* wc.c -- WC [-clmwL] [FILE ...]: the lines, words and bytes of the files, as wc(1)
   writes them (the long options, --lines and so on, are not here):

     -l  the lines (newlines)    -w  the words (runs of non-blanks)
     -c  the bytes               -m  the characters (the same: one byte each)
     -L  the length of the longest line (a tab goes to the next multiple of 8)
   No option is -lwc.  The counts come in the order lines, words, characters,
   bytes, longest line, right-aligned in a width of the digits of the largest
   count, and the file name after them; with more than one FILE a line "total"
   ends the list.  No FILE or a FILE "-" is the standard input.

   .TEXT, .C, .H and .BAS files are text files (a line is what ends in a CR on the disk,
   the DLE blanks are put back); any other file is read as the bytes it holds,
   whole 512-byte blocks.  Run it from the shell: WC A.C, WC -l A.C B.C,
   DIR | WC -l.

   A FILE may have the wild cards of DIR: * or = (any characters) and ? (any one),
   in the name after an optional VOL:, #n: or path (WC *.C, WC #5:*.TEXT, WC A/*.C,
   WC #5:/A/B/*.H: in a sub-volume); the shell does not expand them, WC does, and
   the names it counts are the files that match. */
#pragma segment MAIN	/* calls within the program: 2-byte CGPs, no NOP after them */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <psys.h>
#include <dirs.h>

int optl, optw, optc, optm, optL;
char ibuf[512];

int dir[1024];
char fname[MAXDIR + 1][48];             /* the names the wild cards gave */
long cl[MAXDIR + 2], cw[MAXDIR + 2], cb[MAXDIR + 2], cL[MAXDIR + 2];
char *names[MAXDIR + 2];


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

int count(char *name, int k)
{
    FILE *f;
    int n, i, c, inword;
    long lines, words, bytes, width, longest;
    if (strcmp(name, "-") == 0)
        f = stdin;
    else {
        f = fopen(name, "r");
        if (!f) {
            fprintf(stderr, "wc: %s: cannot open it\n", name);
            return 1;
        }
    }
    lines = words = bytes = width = longest = 0;
    inword = 0;
    while ((n = more(f)) > 0) {
        bytes += n;
        for (i = 0; i < n; i++) {
            c = ibuf[i] & 255;
            if (c == ' ' || c == '\n' || c == '\t' || c == '\r' || c == 11 || c == 12) {
                inword = 0;
            } else if (!inword) {
                inword = 1;
                words++;
            }
            if (c == '\n') {
                lines++;
                if (width > longest)
                    longest = width;
                width = 0;
            } else if (c == '\t')
                width = (width / 8 + 1) * 8;
            else if (c == '\r' || c == 12)
                ;
            else if (c >= 32 && c != 127)
                width++;
        }
    }
    if (width > longest)
        longest = width;
    if (f != stdin)
        fclose(f);
    cl[k] = lines;
    cw[k] = words;
    cb[k] = bytes;
    cL[k] = longest;
    names[k] = name;
    return 0;
}

int digits(long n)
{
    int d;
    d = 1;
    while (n >= 10) {
        n /= 10;
        d++;
    }
    return d;
}

void show(int k, int w)
{
    int first;
    first = 1;
    if (optl) { printf("%*ld", w, cl[k]); first = 0; }
    if (optw) { printf(first ? "%*ld" : " %*ld", w, cw[k]); first = 0; }
    if (optm) { printf(first ? "%*ld" : " %*ld", w, cb[k]); first = 0; }
    if (optc) { printf(first ? "%*ld" : " %*ld", w, cb[k]); first = 0; }
    if (optL) { printf(first ? "%*ld" : " %*ld", w, cL[k]); first = 0; }
    if (names[k])
        printf(" %s", names[k]);
    printf("\n");
}

/* one FILE argument: a name as it is, or the wild card's matches (VOL:PAT, #n:PAT,
   PATH/PAT, PAT) added to files[]; returns the new count, -1 when there is no room */
int expand(char *arg, char **files, int nf, int *status)
{
    char name[16];
    char pat[16];
    int u, base, i, n, before;
    if (strpbrk(arg, "*=?") == NULL) {
        if (nf >= MAXDIR + 1)
            return -1;
        files[nf] = arg;
        return nf + 1;
    }
    if (!pathdir(arg, dir, &u, &base, pat)) {
        fprintf(stderr, "wc: %s: no such disk or sub-volume on line\n", arg);
        *status = 1;
        return nf;
    }
    n = strlen(arg) - strlen(pat);          /* what goes before each name: VOL:, A/ ... */
    if (n > 31)
        n = 31;
    before = nf;
    for (i = 1; i <= dir[8]; i++) {
        direntname(dir, i, name);
        if (!wildmatch(pat, name))
            continue;
        if (nf >= MAXDIR + 1)
            return -1;
        memcpy(fname[nf], arg, n);
        fname[nf][n] = 0;
        strcat(fname[nf], name);
        files[nf] = fname[nf];
        nf++;
    }
    if (nf == before) {
        fprintf(stderr, "wc: %s: no file matches\n", arg);
        *status = 1;
    }
    return nf;
}

int main(int argc, char **argv)
{
    int i, j, endopt, nf, k, w, status;
    long mx;
    char *a;
    char *files[MAXDIR + 2];
    endopt = 0;
    nf = 0;
    status = 0;
    for (i = 1; i < argc; i++) {
        a = argv[i];
        if (!endopt && a[0] == '-' && a[1]) {
            if (a[1] == '-' && a[2] == 0) {
                endopt = 1;
                continue;
            }
            for (j = 1; a[j]; j++)
                switch (a[j]) {
                case 'l': optl = 1; break;
                case 'w': optw = 1; break;
                case 'c': optc = 1; break;
                case 'm': optm = 1; break;
                case 'L': optL = 1; break;
                default:
                    fprintf(stderr, "wc: invalid option -- '%c'\n", a[j]);
                    fprintf(stderr, "use: WC [-clmwL] [FILE ...]   (no FILE or -: the standard input)\n");
                    return 1;
                }
        } else {
            nf = expand(a, files, nf, &status);
            if (nf < 0) {
                fprintf(stderr, "wc: more than %d files\n", MAXDIR + 1);
                return 1;
            }
        }
    }
    if (!(optl || optw || optc || optm || optL))
        optl = optw = optc = 1;
    if (nf == 0 && status) {
        return status;                  /* only wild cards, none matched */
    }
    if (nf == 0) {
        files[0] = "-";
        nf = 1;
        if (count("-", 0))
            return 1;
        names[0] = 0;
    } else
        for (k = 0; k < nf; k++)
            if (count(files[k], k)) {
                status = 1;
                cl[k] = cw[k] = cb[k] = cL[k] = -1;     /* not there: not listed */
            }
    cl[nf] = cw[nf] = cb[nf] = cL[nf] = 0;
    names[nf] = "total";
    mx = 0;
    for (k = 0; k < nf; k++)
        if (cl[k] >= 0) {
            cl[nf] += cl[k];
            cw[nf] += cw[k];
            cb[nf] += cb[k];
            if (cL[k] > cL[nf])
                cL[nf] = cL[k];
        }
    if (optl && cl[nf] > mx) mx = cl[nf];
    if (optw && cw[nf] > mx) mx = cw[nf];
    if ((optc || optm) && cb[nf] > mx) mx = cb[nf];
    if (optL && cL[nf] > mx) mx = cL[nf];
    w = digits(mx);
    for (k = 0; k < nf; k++)
        if (cl[k] >= 0)
            show(k, w);
    if (nf > 1)
        show(nf, w);
    return status;
}
