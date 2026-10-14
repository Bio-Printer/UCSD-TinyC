/* lc.c -- LC [-1xFsrSC] [VOL: | #n: | PATH][PATTERN ...]: the file names of a
   disk or a sub-volume in columns (Plan 9's lc).

     -1  one name on a line         -x  fill the rows first, not the columns
     -F  a * after the names of CODE files
     -s  the size in blocks before each name
     -r  the order reversed         -S  the biggest files first (by size)
     -C  columns also when the output goes to a file or a pipe
   The names are sorted (blanks and case do not matter) and written down the
   columns as wide as the console (SYSCOM's crtinfo.width); to a file or a pipe
   they come one on a line unless -C is there.  A PATTERN has * or = (any
   characters) and ? (any one); with a VOL:, #n: or path (A/, #5:/A/B/) before
   it the files are there, otherwise on the shell's path or the prefix volume.
   No PATTERN is every file; a PATTERN that is a sub-volume's name (LC A) is
   every file in it.  With more than one PATTERN each is listed on its own
   under a line with the disk's (or sub-volume's) name.
   The status is 1 when a PATTERN matched no file, 2 when a disk is not on line.

   Run it from the shell: LC, LC -F *.CODE, LC #5: #4:*.TEXT, LC -1 | WC -l. */
#pragma segment MAIN	/* calls within the program: 2-byte CGPs, no NOP after them */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <psys.h>
#include <dirs.h>

int opt1, optx, optF, opts, optr, optS, optC;
int dir[1024];
char names[MAXDIR + 1][20];             /* the name, then * perhaps */
int size[MAXDIR + 1];
int ord[MAXDIR + 1];

char patbuf[16];

/* [VOL: | #n: | PATH][PAT]: the unit (its directory read into dir; into vol
   where that is: "VOL:" or "VOL:/A/") and *pat; 0 (and a message) when there
   is no such disk or sub-volume.  PAT a sub-volume's name: every file in it */
int target(char *arg, char *vol, char **pat)
{
    char path[80];
    int u, base, n;
    strncpy(path, arg, 70);
    path[70] = 0;
    slashes(path);                      /* (A\: A/) */
    if (path[0] == '#' && !strchr(path, ':'))
        strcat(path, ":");              /* #5: the unit */
    if (!pathdir(path, dir, &u, &base, patbuf)) {
        if (strchr(path, '/'))
            fprintf(stderr, "lc: no sub-volume %s\n", arg);
        else {
            if (strchr(path, ':'))
                *strchr(path, ':') = 0;         /* the disk's name, without its : */
            fprintf(stderr, "lc: no disk %s on line\n", *path ? path : ":");
        }
        return 0;
    }
    if (patbuf[0] && !strpbrk(patbuf, "*=?")) {
        strcat(path, "/");              /* a sub-volume?  then what is in it */
        if (!pathdir(path, dir, &u, &base, patbuf)) {
            path[strlen(path) - 1] = 0;
            pathdir(path, dir, &u, &base, patbuf);
        }
    }
    n = strlen(path) - strlen(patbuf);
    path[n] = 0;
    if (!svcanon(path, vol)) {
        fprintf(stderr, "lc: no disk %s on line\n", *arg ? arg : ":");
        return 0;
    }
    if (vol[strlen(vol) - 1] != ':')
        strcat(vol, "/");
    *pat = patbuf;
    return u;
}

/* name a before name b (case does not matter) */
int before(int a, int b)
{
    int c;
    if (optS && size[a] != size[b])
        return size[a] > size[b];
    c = 0;
    {
        char *p, *q;
        p = names[a];
        q = names[b];
        while (*p && toupper(*p) == toupper(*q)) {
            p++;
            q++;
        }
        c = toupper(*p) - toupper(*q);
    }
    return c < 0;
}

void put(char *s, int blank)
{
    printf("%s", s);
    while (blank-- > 0)
        putchar(' ');
}

/* the n names in ord[], as the options want them */
void show(int n, int width)
{
    char item[40];
    int len[MAXDIR + 1];
    int i, k, cw, cols, rows, r, c, maxlen, last;
    maxlen = 0;
    for (i = 0; i < n; i++) {
        k = ord[i];
        len[i] = opts ? sprintf(item, "%4d %s", size[k], names[k]) : strlen(names[k]);
        if (len[i] > maxlen)
            maxlen = len[i];
    }
    cols = 1;
    if (!opt1 && (optC || (stdout->flags & __F_CON)))
        cols = width / (maxlen + 2);
    if (cols < 1)
        cols = 1;
    if (cols > n)
        cols = n;
    rows = (n + cols - 1) / cols;
    if (!optx)
        cols = (n + rows - 1) / rows;   /* no empty last column */
    cw = maxlen + 2;
    for (r = 0; r < rows; r++) {
        last = -1;
        for (c = 0; c < cols; c++) {
            i = optx ? r * cols + c : c * rows + r;
            if (i < n)
                last = c;
        }
        for (c = 0; c <= last; c++) {
            i = optx ? r * cols + c : c * rows + r;
            if (i >= n)
                continue;
            k = ord[i];
            if (opts)
                sprintf(item, "%4d %s", size[k], names[k]);
            else
                strcpy(item, names[k]);
            put(item, c < last ? cw - len[i] : 0);
        }
        putchar('\n');
    }
}

/* the matching files of the disk in dir, listed; the number of them */
int list(char *pat, int header, char *vol)
{
    int i, j, n, t, nf, kind;
    int *e;
    char name[16];
    nf = dir[8];
    n = 0;
    for (i = 1; i <= nf && n < MAXDIR + 1; i++) {
        direntname(dir, i, name);
        if (*pat && !wildmatch(pat, name))
            continue;
        e = dir + 13 * i;
        kind = e[2] & 15;
        strcpy(names[n], name);
        if (optF && kind == 2)
            strcat(names[n], "*");
        size[n] = e[1] - e[0];
        ord[n] = n;
        n++;
    }
    for (i = 1; i < n; i++) {           /* an insertion sort */
        t = ord[i];
        for (j = i; j > 0 && before(t, ord[j - 1]); j--)
            ord[j] = ord[j - 1];
        ord[j] = t;
    }
    if (optr)
        for (i = 0; i < n / 2; i++) {
            t = ord[i];
            ord[i] = ord[n - 1 - i];
            ord[n - 1 - i] = t;
        }
    if (header && n) {
        printf("%s\n", vol);
    }
    if (n)
        show(n, SYSCOM->crtinfo.width);
    return n;
}

int main(int argc, char **argv)
{
    int i, j, endopt, na, u, status, first;
    char *a, *pat;
    char vol[80];
    char *args[16];
    endopt = 0;
    na = 0;
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
                case '1': opt1 = 1; break;
                case 'x': optx = 1; break;
                case 'F': optF = 1; break;
                case 's': opts = 1; break;
                case 'r': optr = 1; break;
                case 'S': optS = 1; break;
                case 'C': optC = 1; break;
                default:
                    fprintf(stderr, "lc: invalid option -- '%c'\n", a[j]);
                    fprintf(stderr, "use: LC [-1xFsrSC] [VOL: | #n: | PATH][PATTERN ...]\n");
                    return 2;
                }
        } else if (na < 16)
            args[na++] = a;
        else {
            fprintf(stderr, "lc: more than 16 patterns\n");
            return 2;
        }
    }
    if (na == 0)
        args[na++] = "";
    first = 1;
    for (i = 0; i < na; i++) {
        u = target(args[i], vol, &pat);
        if (!u) {
            status = 2;
            continue;
        }
        if (na > 1 && !first)
            putchar('\n');
        first = 0;
        if (list(pat, na > 1, vol) == 0) {
            fprintf(stderr, "lc: %s: no file matches\n", args[i]);
            if (status == 0)
                status = 1;
        }
    }
    return status;
}
