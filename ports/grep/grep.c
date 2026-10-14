/* grep.c -- GREP [-i] [-n] PATTERN [VOL: | #n: | PATH]FILES ...: the lines of
   text files that match PATTERN, each as VOL:NAME:LINE: text (VOL:/A/NAME:...
   in a sub-volume).  -n: the lines that do not match (-in and -ni are all
   right).

   Case is ignored unless -i is given (-i: exact case; the other way round
   from Unix grep).  FILES takes the shell's wildcards (* or = any
   characters, ? any one) and is looked for on every disk on line unless
   it names a volume (VOL: or #n:) or a sub-volume (A/*.C, #5:/A/B/*.H; on
   the shell's path, after a CD into a sub-volume, FILES is there).  Only
   text files are searched.

   PATTERN is a regular expression:
     c        the character c        .      any one character
     *        any characters (none or more, as in file names)
     [abc]    one of them            [^abc] any other ([a-z]: a range)
     x+       x 1 or more times      x?     x 0 or 1 time
     ^        the line's start       $      the line's end
     \c       c itself (\. \* \[ ...) \s    a blank or a tab (the shell
                                             splits its words at blanks)

   Run it from the shell: GREP printf *.C, GREP -i ^int #9:*.H,
   GREP vi*pageup TOOLSRC:=.C, GREP fopen\s?\( TOOLSRC:VI*.C */
#pragma segment MAIN	/* calls within the program: 2-byte CGPs, no NOP after them */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <psys.h>
#include <dirs.h>

int exact;                              /* -i: case counts */
int invert;                             /* -n: the lines without the pattern */
long nlines;                            /* lines found */
int nfiles;                             /* files with a line found */
int nsearched;                          /* files searched */

/* ---- the regular expression ---- */

char fold[256];                         /* each character as it compares: itself, or its capital */

int same(int a, int b)
{
    return fold[a & 255] == fold[b & 255];
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
    int n, ok;
    for (;;) {
        if (*re == 0)
            return 1;
        if (*re == '$' && re[1] == 0)
            return *s == 0;
        if (*re == '*')                 /* any characters, as in file names */
            return matchrep(".", 1, 0, re + 1, s);
        ok = matchone(re, *s, &n);      /* its length, and whether it matches */
        if (re[n] == '+')
            return matchrep(re, n, 1, re + n + 1, s);
        if (re[n] == '?')
            return (*s && ok && matchhere(re + n + 1, s + 1))
                   || matchhere(re + n + 1, s);
        if (*s == 0 || !ok)
            return 0;
        re = re + n;
        s++;
    }
}

/* the pattern starts with this character (and its other case): the only places a match can begin;
   0 when it starts with something else (a class, ., *, \\, ^, $, a blank, or an optional element) */
char first1;
char first2;
char ftab[256];                         /* the characters a match can begin with */
int usetab;

void setfirst(char *re)
{
    char c;
    int i, n;
    c = *re;
    first1 = 0;
    first2 = 0;
    usetab = 0;
    if (c == 0 || c == '^' || c == '$' || c == '*')
        return;
    matchone(re, 'a', &n);
    if (re[n] == '?')                   /* the first element is optional */
        return;
    for (i = 1; i < 256; i++)
        ftab[i] = matchone(re, i, &n);
    ftab[0] = 0;
    usetab = 1;
    if (c == ' ' || c == '.' || c == '[' || c == '\\')
        return;
    first1 = c;
    first2 = c;
    if (!exact) {
        if (islower(c))
            first2 = toupper(c);
        else if (isupper(c))
            first2 = tolower(c);
    }
}

/* does re match anywhere in s */
int match(char *re, char *s)
{
    if (*re == '^')
        return matchhere(re + 1, s);
    do {
        if (!usetab || ftab[*s & 255])
            if (matchhere(re, s))
                return 1;
    } while (*s++);
    return 0;
}

/* ---- disks, directories, file names ---- */

/* ---- a text file, read by its blocks on the disk (a volume's or a
   sub-volume's file alike): 1 KB pages from block 2, lines ending in CR,
   DLE n = n - 32 blanks, NULs filling the rest of a page ---- */

char page[1024];
char line[1100];

/* the text file name (where: "VOL:" or "VOL:/A/"), nb blocks from block
   first of unit u */
void search(char *re, int u, int first, int nb, char *vol, char *name)
{
    int b, i, n, c, e, k, m, end, found;
    char *z, *cr, *d;
    long lineno;
    nsearched++;
    found = 0;
    lineno = 1;
    n = 0;
    for (b = 2; b < nb; b = b + 2) {
        m = nb - b < 2 ? 1 : 2;
        __cspv(5, u, page, 0, m * 512, first + b, 0);     /* UNITREAD */
        if (__cspi(34) != 0) {
            printf("%s%s: disk error %d\n", vol, name, __cspi(34));
            break;
        }
        if (m == 1)
            memset(page + 512, 0, 512);
        z = memchr(page, 0, 1024);      /* the NULs fill the rest of the page */
        end = z ? z - page : 1024;
        i = 0;
        while (i < end) {               /* a line (or the rest of the page) at a time */
            cr = memchr(page + i, '\r', end - i);
            e = cr ? cr - page : end;
            /* the pattern starts with a letter or character: a line without it cannot match
               (a DLE only stands for blanks, so it is not looked for) */
            if (first1 && !invert && n == 0 && cr && !memchr(page + i, first1, e - i)
                && (first2 == first1 || !memchr(page + i, first2, e - i))) {
                lineno++;
                i = e + 1;
                continue;
            }
            while (i < e) {             /* the line's text, DLE n = n - 32 blanks */
                d = memchr(page + i, 16, e - i);
                k = d ? d - page - i : e - i;
                if (n + k > 1024)
                    k = 1024 - n;
                memcpy(line + n, page + i, k);
                n = n + k;
                i = d ? d - page : e;
                if (d) {
                    if (i + 1 < e) {
                        c = (page[i + 1] & 255) - 32;
                        while (c-- > 0 && n < 1024)
                            line[n++] = ' ';
                        i = i + 2;
                    } else {
                        if (n < 1024)
                            line[n++] = 16;
                        i++;
                    }
                }
            }
            if (cr) {
                line[n] = 0;
                if (match(re, line) != invert) {
                    printf("%s%s:%ld: %s\n", vol, name, lineno, line);
                    nlines++;
                    found = 1;
                }
                lineno++;
                n = 0;
                i = e + 1;
            }
        }
    }
    if (n > 0) {                        /* a last line without its CR */
        line[n] = 0;
        if (match(re, line) != invert) {
            printf("%s%s:%ld: %s\n", vol, name, lineno, line);
            nlines++;
            found = 1;
        }
    }
    nfiles = nfiles + found;
}

/* the text files of dir (at block base of unit u; vol: where it is) that
   match pat, searched: how many */
int dirfiles(char *re, int u, int base, int *dir, char *pat, char *vol)
{
    char name[16];
    int i, n;
    n = 0;
    for (i = 1; i <= dir[8]; i++) {
        direntname(dir, i, name);
        if ((dir[13 * i + 2] & 15) != 3 || !wildmatch(*pat ? pat : "*", name))
            continue;                   /* not text, or not the name */
        n++;
        search(re, u, base + dir[13 * i], dir[13 * i + 1] - dir[13 * i], vol, name);
    }
    return n;
}

/* [VOL: | #n: | PATH]PAT: every text file that matches, on that volume or
   sub-volume, or on every disk */
void files(char *re, char *spec)
{
    int dir[1024];
    char want[20];
    char vol[80];
    char path[80];
    char leaf[16];
    char *pat;
    char *c;
    int k, u, base, one, n;
    if (strchr(spec, '/') || (__svpath[0] && !strchr(spec, ':') && spec[0] != '#')) {
        if (strlen(spec) > 70 || !pathdir(spec, dir, &u, &base, leaf)) {   /* a sub-volume: there only */
            printf("grep: no sub-volume for %s\n", spec);
            return;
        }
        strcpy(path, spec);
        path[strlen(spec) - strlen(leaf)] = 0;
        if (!svcanon(path, vol)) {
            printf("grep: no sub-volume for %s\n", spec);
            return;
        }
        if (vol[strlen(vol) - 1] != ':')
            strcat(vol, "/");
        if (dirfiles(re, u, base, dir, leaf, vol) == 0)
            printf("grep: no text file %s on %s\n", spec, vol);
        return;
    }
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
        if (!dirread(u, dir))
            continue;
        direntname(dir, 0, vol);
        if (one) {
            if (want[0] == '#' ? atoi(want + 1) != u
                : want[0] == '*' ? u != SYSCOM->sysunit
                : !wildmatch(want, vol) || strlen(want) != strlen(vol))
                continue;
        }
        strcat(vol, ":");
        n = n + dirfiles(re, u, 0, dir, pat, vol);
    }
    if (n == 0)
        printf("grep: no text file %s on %s\n", spec, one ? want : "any disk on line");
}

int main(int argc, char **argv)
{
    int a, k;
    a = 1;
    while (a < argc && argv[a][0] == '-' && argv[a][1] && strspn(argv[a] + 1, "iIn") == strlen(argv[a] + 1)) {
        for (k = 1; argv[a][k]; k++)
            if (argv[a][k] == 'n')
                invert = 1;
            else
                exact = 1;
        a++;
    }
    if (argc - a < 1) {
        printf("use: GREP [-i] [-n] PATTERN [VOL: | #n:]FILES ...   (from the shell)\n");
        printf("  case is ignored unless -i; -n: the lines without the pattern; FILES: * = ? wildcards, every disk\n");
        printf("  unless a volume is named; PATTERN: * any characters, . any one,\n");
        printf("  [] [^] a class, x+ x?, ^ $ start/end, \\c c itself, \\s a blank\n");
        printf("  no FILES: the lines of the standard input (GREP PAT <FILE, DIR | GREP PAT)\n");
        return 2;
    }
    for (k = 0; k < 256; k++)
        fold[k] = exact ? k : toupper(k);
    setfirst(argv[a]);
    if (argc - a == 1) {                /* no files: the standard input, its lines as they are */
        while (fgets(line, 1090, stdin)) {
            k = strlen(line);
            if (k > 0 && line[k - 1] == '\n')
                line[k - 1] = 0;
            if (match(argv[a], line) != invert) {
                puts(line);
                nlines++;
            }
        }
        return nlines ? 0 : 1;
    }
    for (k = a + 1; k < argc; k++)      /* argv[a]: the pattern */
        files(argv[a], argv[k]);
    printf("%ld line%s in %d of %d file%s\n", nlines, nlines == 1 ? "" : "s", nfiles, nsearched,
           nsearched == 1 ? "" : "s");
    return nlines ? 0 : 1;
}
