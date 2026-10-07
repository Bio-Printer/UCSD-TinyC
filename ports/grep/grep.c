/* grep.c -- GREP [-i] PATTERN [VOL: | #n:]FILES ...: the lines of text
   files that match PATTERN, each as VOL:NAME:LINE: text.

   Case is ignored unless -i is given (-i: exact case; the other way round
   from Unix grep).  FILES takes the shell's wildcards (* or = any
   characters, ? any one) and is looked for on every disk on line unless
   it names a volume (VOL: or #n:).  Only text files are searched.

   PATTERN is a regular expression:
     c        the character c        .      any character
     [abc]    one of them            [^abc] any other ([a-z]: a range)
     x*       x 0 or more times      x+     1 or more     x?  0 or 1
     ^        the line's start       $      the line's end
     \c       c itself (\. \* \[ ...) \s    a blank or a tab (the shell
                                             splits its words at blanks)

   Run it from the shell: GREP printf *.C, GREP -i ^int #9:*.H,
   GREP fopen\s*\( TOOLSRC:VI*.C */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <psys.h>

int units[] = { 4, 5, 9, 10, 11, 12, 13, 14 };      /* the disk units */
int exact;                              /* -i: case counts */
long nlines;                            /* lines found */
int nfiles;                             /* files with a line found */
int nsearched;                          /* files searched */

/* ---- the regular expression ---- */

int same(int a, int b)
{
    if (exact)
        return a == b;
    return toupper(a) == toupper(b);
}

/* the element at re (one character, ., [class], \c): its length in *len;
   does it match c */
int matchone(char *re, int c, int *len)
{
    int neg, ok, lo, hi;
    char *p;
    if (*re == '\\' && re[1]) {
        *len = 2;
        if (re[1] == 's')
            return c == ' ' || c == '\t';
        if (re[1] == 't')
            return c == '\t';
        return same(re[1], c);
    }
    if (*re == '.') {
        *len = 1;
        return 1;
    }
    if (*re == '[') {
        p = re + 1;
        neg = *p == '^';
        if (neg)
            p++;
        ok = 0;
        do {                            /* a ] first is one of the class */
            lo = *p;
            if (lo == '\\' && p[1])
                lo = *++p;
            hi = lo;
            if (p[1] == '-' && p[2] && p[2] != ']') {
                hi = p[2];
                p = p + 2;
            }
            if (exact ? (c >= lo && c <= hi)
                      : ((toupper(c) >= toupper(lo) && toupper(c) <= toupper(hi))
                         || (tolower(c) >= tolower(lo) && tolower(c) <= tolower(hi))))
                ok = 1;
            p++;
        } while (*p && *p != ']');
        *len = (*p ? p + 1 : p) - re;
        return ok != neg;
    }
    *len = 1;
    return same(*re, c);
}

int matchhere(char *re, char *s);

/* element e (length n) repeated: at least lo times, as many as it can,
   then the rest */
int matchrep(char *e, int n, int lo, char *rest, char *s)
{
    char *t;
    int k;
    t = s;
    while (*t && matchone(e, *t, &k))
        t++;
    for (; t >= s + lo; t--)
        if (matchhere(rest, t))
            return 1;
    return 0;
}

/* does re match at the start of s */
int matchhere(char *re, char *s)
{
    int n;
    for (;;) {
        if (*re == 0)
            return 1;
        if (*re == '$' && re[1] == 0)
            return *s == 0;
        matchone(re, 'a', &n);          /* the element's length */
        if (re[n] == '*')
            return matchrep(re, n, 0, re + n + 1, s);
        if (re[n] == '+')
            return matchrep(re, n, 1, re + n + 1, s);
        if (re[n] == '?')
            return (*s && matchone(re, *s, &n) && matchhere(re + n + 1, s + 1))
                   || matchhere(re + n + 1, s);
        if (*s == 0 || !matchone(re, *s, &n))
            return 0;
        re = re + n;
        s++;
    }
}

/* does re match anywhere in s */
int match(char *re, char *s)
{
    if (*re == '^')
        return matchhere(re + 1, s);
    do {
        if (matchhere(re, s))
            return 1;
    } while (*s++);
    return 0;
}

/* ---- disks, directories, file names ---- */

/* unit u's directory (blocks 2..5) into dir: 0 if no disk is there */
int readdir(int u, int *dir)
{
    unsigned char *d;
    d = (unsigned char *)dir;
    __cspv(5, u, dir, 0, 2048, 2, 0);   /* UNITREAD */
    if (__cspi(34) != 0)
        return 0;
    return d[6] >= 1 && d[6] <= 7 && dir[8] >= 0 && dir[8] <= 77;
}

/* a directory's volume name (i = 0), or entry i's file name */
void dirname(int *dir, int i, char *s)
{
    unsigned char *e;
    int n;
    e = (unsigned char *)dir + 26 * i + 6;
    for (n = 0; n < e[0] && n < 15; n++)
        s[n] = e[1 + n];
    s[n] = 0;
}

/* a file name's wildcards: * or = any characters, ? any one */
int wildmatch(char *pat, char *name)
{
    if (*pat == 0)
        return *name == 0;
    if (*pat == '*' || *pat == '=') {
        for (;;) {
            if (wildmatch(pat + 1, name))
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
    return wildmatch(pat + 1, name + 1);
}

/* ---- a text file, read with the OS's untyped file routines: 1 KB pages
   from block 2, lines ending in CR, DLE n = n - 32 blanks, NULs filling
   the rest of a page ---- */

void __ptitle(char *name, char *title);     /* the C library's: a Pascal string */

char fib[80];
char page[1024];
char line[1100];

void search(char *re, int u, char *vol, char *name)
{
    char path[24];
    char title[32];
    int b, i, n, c, found;
    long lineno;
    sprintf(path, "#%d:%s", u, name);
    __ptitle(path, title);
    __cxp0v(3, fib, 0, -1);             /* FINIT(fib, NIL, untyped) */
    __cxp0v(5, fib, title, 1, 0);       /* FOPEN, an old file */
    if (__cspi(34) != 0) {
        printf("%s:%s: cannot open it\n", vol, name);
        return;
    }
    nsearched++;
    found = 0;
    lineno = 1;
    n = 0;
    for (b = 2;; b = b + 2) {
        if (__cxp0i(28, fib, page, 0, 2, b, 1, 0, 0) != 2 || __cspi(34) != 0)    /* FBLOCKIO */
            break;
        for (i = 0; i < 1024; i++) {
            c = page[i] & 255;
            if (c == 0)
                break;                  /* the rest of the page */
            if (c == 16 && i + 1 < 1024) {      /* DLE: blanks */
                c = (page[++i] & 255) - 32;
                while (c-- > 0 && n < 1024)
                    line[n++] = ' ';
                continue;
            }
            if (c == '\r') {
                line[n] = 0;
                if (match(re, line)) {
                    printf("%s:%s:%ld: %s\n", vol, name, lineno, line);
                    nlines++;
                    found = 1;
                }
                lineno++;
                n = 0;
                continue;
            }
            if (n < 1024)
                line[n++] = c;
        }
    }
    if (n > 0) {                        /* a last line without its CR */
        line[n] = 0;
        if (match(re, line)) {
            printf("%s:%s:%ld: %s\n", vol, name, lineno, line);
            nlines++;
            found = 1;
        }
    }
    __cxp0v(6, fib, 0);                 /* FCLOSE */
    nfiles = nfiles + found;
}

/* [VOL: | #n:]PAT: every text file that matches, on that volume or on
   every disk */
void files(char *re, char *spec)
{
    int dir[1024];
    char want[20];
    char vol[16];
    char name[16];
    char *pat;
    char *c;
    int k, u, i, one, n;
    c = strchr(spec, ':');
    one = 0;
    want[0] = 0;
    pat = spec;
    if (c) {
        n = c - spec;
        if (n > 19)
            n = 19;
        memcpy(want, spec, n);
        want[n] = 0;
        pat = c + 1;
        one = 1;
    }
    n = 0;
    for (k = 0; k < 8; k++) {
        u = units[k];
        if (!readdir(u, dir))
            continue;
        dirname(dir, 0, vol);
        if (one) {
            if (want[0] == '#' ? atoi(want + 1) != u
                : want[0] == '*' ? u != SYSCOM->sysunit
                : !wildmatch(want, vol) || strlen(want) != strlen(vol))
                continue;
        }
        for (i = 1; i <= dir[8]; i++) {
            dirname(dir, i, name);
            if ((dir[13 * i + 2] & 15) != 3 || !wildmatch(*pat ? pat : "*", name))
                continue;               /* not text, or not the name */
            n++;
            search(re, u, vol, name);
        }
    }
    if (n == 0)
        printf("grep: no text file %s on %s\n", spec, one ? want : "any disk on line");
}

int main(int argc, char **argv)
{
    int a, k;
    a = 1;
    if (a < argc && (strcmp(argv[a], "-i") == 0 || strcmp(argv[a], "-I") == 0)) {
        exact = 1;
        a++;
    }
    if (argc - a < 2) {
        printf("use: GREP [-i] PATTERN [VOL: | #n:]FILES ...   (from the shell)\n");
        printf("  case is ignored unless -i; FILES: * = ? wildcards, every disk\n");
        printf("  unless a volume is named; PATTERN: . [] [^] * + ? ^ $ \\c \\s\n");
        return 2;
    }
    for (k = a + 1; k < argc; k++)      /* argv[a]: the pattern */
        files(argv[a], argv[k]);
    printf("%ld line%s in %d of %d file%s\n", nlines, nlines == 1 ? "" : "s", nfiles, nsearched,
           nsearched == 1 ? "" : "s");
    return nlines ? 0 : 1;
}
