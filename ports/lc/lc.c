/* lc.c -- LC [-1xFsrSC] [VOL: | #n:][PATTERN ...]: the file names of a disk in
   columns (Plan 9's lc; the UCSD directory has no subdirectories).

     -1  one name on a line         -x  fill the rows first, not the columns
     -F  a * after the names of CODE files
     -s  the size in blocks before each name
     -r  the order reversed         -S  the biggest files first (by size)
     -C  columns also when the output goes to a file or a pipe
   The names are sorted (blanks and case do not matter) and written down the
   columns as wide as the console (SYSCOM's crtinfo.width); to a file or a pipe
   they come one on a line unless -C is there.  A PATTERN has * or = (any
   characters) and ? (any one); with a VOL: or #n: before it the disk is that
   one, otherwise the prefix volume.  No PATTERN is every file.  With more than
   one PATTERN each is listed on its own under a line with the disk's name.
   The status is 1 when a PATTERN matched no file, 2 when a disk is not on line.

   Run it from the shell: LC, LC -F *.CODE, LC #5: #4:*.TEXT, LC -1 | WC -l. */
#pragma segment MAIN	/* calls within the program: 2-byte CGPs, no NOP after them */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <psys.h>

int opt1, optx, optF, opts, optr, optS, optC;
int dir[1024];
char names[MAXDIR + 1][20];             /* the name, then * perhaps */
int size[MAXDIR + 1];
int ord[MAXDIR + 1];

int same(char *a, char *b)
{
    while (*a && toupper(*a) == toupper(*b)) {
        a++;
        b++;
    }
    return *a == 0 && *b == 0;
}

/* unit u's directory (blocks 2..5) into dir: 0 if no disk is there */
int readdir(int u)
{
    unsigned char *d;
    d = (unsigned char *)dir;
    __cspv(5, u, dir, 0, 2048, 2, 0);   /* UNITREAD */
    if (__cspi(34) != 0)
        return 0;
    return d[6] >= 1 && d[6] <= 7 && dir[8] >= 0 && dir[8] <= 77;
}

/* a directory's volume name, or entry i's file name */
void dirname(int i, char *s)
{
    unsigned char *e;
    int n;
    e = (unsigned char *)dir + 26 * i + 6;
    for (n = 0; n < e[0] && n < 15; n++)
        s[n] = e[1 + n];
    s[n] = 0;
}

/* the unit of a volume: "#n", "n", "*" (the system volume) or a name
   (":" after it or not); 0 when no disk on line has it */
int unitof(char *vol)
{
    char name[20];
    char got[16];
    int n, k, u;
    strncpy(name, vol, 19);
    name[19] = 0;
    n = strlen(name);
    if (n > 0 && name[n - 1] == ':')
        name[--n] = 0;
    if (n == 0)
        return 0;
    if (strcmp(name, "*") == 0)
        return SYSCOM->sysunit;
    if (name[0] == '#' || isdigit(name[0])) {
        u = atoi(name[0] == '#' ? name + 1 : name);
        return u > 0 && u <= 14 && readdir(u) ? u : 0;
    }
    for (k = 0; k < 8; k++)
        if (readdir(units[k])) {
            dirname(0, got);
            if (same(got, name))
                return units[k];
        }
    return 0;
}

/* the prefix volume's name (the OS's DKVID) */
void prefix(char *s)
{
    unsigned char *v;
    int n;
    v = (unsigned char *)__osvaraddr(OS_DKVID);
    for (n = 0; n < v[0] && n < 7; n++)
        s[n] = v[1 + n];
    s[n] = 0;
}

/* does name match the pattern: * or = any characters, ? any one */
int match(char *pat, char *name)
{
    if (*pat == 0)
        return *name == 0;
    if (*pat == '*' || *pat == '=') {
        for (;;) {
            if (match(pat + 1, name))
                return 1;
            if (*name == 0)
                return 0;
            name++;
        }
    }
    if (*name == 0)
        return 0;
    if (*pat != '?' && toupper(*pat) != toupper(*name))
        return 0;
    return match(pat + 1, name + 1);
}

/* [VOL: | #n:][PAT]: the unit (its directory read into dir, its volume's
   name into vol) and *pat; 0 (and a message) when no disk has it */
int target(char *arg, char *vol, char **pat)
{
    char *colon;
    int u, i;
    colon = strchr(arg, ':');
    if (colon) {
        i = colon - arg;
        if (i > 19)
            i = 19;
        memcpy(vol, arg, i);
        vol[i] = 0;
        *pat = colon + 1;
    } else if (arg[0] == '#') {
        strcpy(vol, arg);
        *pat = "";
    } else {
        prefix(vol);
        *pat = arg;
    }
    u = unitof(vol);
    if (u == 0 || !readdir(u)) {
        fprintf(stderr, "lc: no disk %s on line\n", *vol ? vol : ":");
        return 0;
    }
    dirname(0, vol);
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
        dirname(i, name);
        if (*pat && !match(pat, name))
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
        printf("%s:\n", vol);
    }
    if (n)
        show(n, SYSCOM->crtinfo.width);
    return n;
}

int main(int argc, char **argv)
{
    int i, j, endopt, na, u, status, first;
    char *a, *pat;
    char vol[20];
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
                    fprintf(stderr, "use: LC [-1xFsrSC] [VOL: | #n:][PATTERN ...]\n");
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
