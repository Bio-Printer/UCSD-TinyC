/* sed.c -- SED [-nEsur] [-i[SUFFIX]] [-e SCRIPT] [-f SCRIPTFILE] [SCRIPT] [FILE ...]: a stream editor.

   The script is applied to every line of the files (the standard input when there is no FILE or it is -):
   the line goes into the pattern space, the commands whose addresses select it are run in order, the
   pattern space is written out (unless -n) and the next line is read.

   Options   -n  do not print the pattern space at the end of a cycle (only p does)
             -e SCRIPT  add the commands (several -e: one script, a line each)   -f FILE  commands from a file
             -E, -r  extended regular expressions (ERE); else basic (BRE)
             -s  the files are separate: line numbers and $ are for each   -i[SUFFIX]  edit the files in place
             (a copy of the original goes to NAME with the SUFFIX before its extension: B.TEXT, .BAK -> B.BAK.TEXT;
             implies -s)   -u  ignored
             Without -e and -f the first word that is not an option is the script.
   Addresses N  $  /re/  /re/I  first~step  addr1,addr2  addr1,+N  addr1,~N  0,/re/   then ! to negate
   Commands  {  }  =  a  b  c  d  D  g  G  h  H  i  l  n  N  p  P  q  Q  r  s  t  T  w  x  y  z  :  #
   s/re/replacement/flags   flags g p N i I w FILE;  & the match, \1..\9, \n, \L \U \l \u \E
   y/abc/xyz/   transliterate;  a TEXT, i TEXT, c TEXT (or a\ and the lines);  b LABEL, t LABEL, T LABEL, :LABEL
   Regular expressions are in SEDRE.C.  Run it from the shell: SED s/a/b/g A.TEXT, DIR | SED -n /C$/p. */
#pragma segment MAIN	/* calls within the program: 2-byte CGPs, no NOP after them */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "sed.h"

#define MAXL 500                        /* the longest line */
#define NCMD 70
#define POOL 2400
#define NFILES 20
#define NW 8                            /* files for w */

int optn, opts, opti, ere;
char *suffix;
char *script;
int slen;

int ct[NCMD];                           /* the command character */
int a1t[NCMD], a1v[NCMD], a1w[NCMD];    /* address 1: type (0 none, 1 line, 2 $, 3 /re/, 4 first~step, 5 0,/re/), value */
int a2t[NCMD], a2v[NCMD];               /* address 2: type (0 none, 1 line, 2 $, 3 /re/, 6 +N, 7 ~N) */
int neg[NCMD];
int rstate[NCMD], rend[NCMD];           /* a range: on, and the line it ends at */
int tgt[NCMD];                          /* b t T: the command to go to; {: the matching } */
int sarg[NCMD];                         /* text in the pool: a i c r w file, s replacement, y from */
int sarg2[NCMD];                        /* y: to */
int sre[NCMD];                          /* the regular expression of an s */
int sflag[NCMD];                        /* s flags: 1 g, 2 p, 4 w */
int snth[NCMD];
int sfile[NCMD];                        /* w: the output file */
int ncmd;
char pool[POOL];
int npool;
char *labels[40];
int labcmd[40];
int nlabels;

FILE *wfile[NW];
char *wname[NW];
int nw;

char *ps, *hs, *tmp, *look;             /* pattern space, hold space, work space, lookahead line */
int havelook;
FILE *out;                              /* where the output goes */
FILE *fin;
char *files[NFILES];
int nfiles, curfile;                    /* the next file to open */
long lineno;
int lastre = -1;
int status;
int quitting;

void die(char *msg, char *what)
{
    fprintf(stderr, "sed: %s%s%s\n", msg, what ? ": " : "", what ? what : "");
    exit(1);
}

int pooladd(char *s, int n)
{
    int at;
    if (npool + n + 1 > POOL)
        die("the script is too big", 0);
    at = npool;
    memcpy(pool + npool, s, n);
    pool[npool + n] = 0;
    npool = npool + n + 1;
    return at;
}

/* ---- the script ---- */

char *sp;                               /* where the parser is */

void skipws(void)
{
    while (*sp == ' ' || *sp == '\t')
        sp++;
}

int getnum(void)
{
    int n;
    n = 0;
    while (isdigit(*sp))
        n = n * 10 + *sp++ - '0';
    return n;
}

/* up to the delimiter d (not escaped), into s; the \d goes in as d, \n stays; returns 0 if there is none */
int delimited(int d, char *s, int max)
{
    int n, c;
    n = 0;
    while (*sp && *sp != d) {
        c = *sp++;
        if (c == '\\' && *sp) {
            if (*sp == d)
                c = *sp++;
            else if (*sp == '\n' && d != '\n') {
                sp++;
                c = '\n';
            } else {
                if (n < max - 1)
                    s[n++] = c;
                c = *sp++;
            }
        }
        if (n < max - 1)
            s[n++] = c;
    }
    if (*sp != d)
        return 0;
    sp++;
    s[n] = 0;
    return 1;
}

int newre(char *pat, int icase)
{
    int r;
    if (!*pat)
        return -1;                      /* empty: the last one used */
    r = re_compile(pat, ere, icase);
    if (r < 0)
        die(re_err ? re_err : "bad regular expression", pat);
    return r;
}

/* an address; fills *t, *v; returns 0 if there is none */
int address(int *t, int *v, int *w)
{
    char pat[200];
    int icase;
    if (isdigit(*sp)) {
        *v = getnum();
        *t = 1;
        *w = 0;
        if (*sp == '~') {
            sp++;
            *w = getnum();
            *t = 4;
        }
        return 1;
    }
    if (*sp == '$') {
        sp++;
        *t = 2;
        return 1;
    }
    if (*sp == '/' || *sp == '\\') {
        int d;
        d = '/';
        if (*sp == '\\') {
            sp++;
            d = *sp;
        }
        sp++;
        if (!delimited(d, pat, 200))
            die("unterminated address regex", 0);
        icase = 0;
        while (*sp == 'I' || *sp == 'M') {
            if (*sp == 'I')
                icase = 1;
            sp++;
        }
        *v = newre(pat, icase);
        *t = 3;
        return 1;
    }
    return 0;
}

/* the text of a, i, c: after the command character */
int gettext(void)
{
    char t[MAXL];
    int n, c;
    skipws();
    if (*sp == '\\') {
        sp++;
        skipws();
        if (*sp == '\n')
            sp++;
    }
    n = 0;
    while (*sp && *sp != '\n') {
        c = *sp++;
        if (c == '\\' && *sp) {
            c = *sp++;
            if (c == '\n') {
                if (n < MAXL - 2)
                    t[n++] = '\n';
                continue;
            }
        }
        if (n < MAXL - 2)
            t[n++] = c;
    }
    t[n++] = '\n';
    return pooladd(t, n);
}

/* a file name to the end of the line */
int getfname(void)
{
    char t[40];
    int n;
    skipws();
    n = 0;
    while (*sp && *sp != '\n' && n < 39)
        t[n++] = *sp++;
    while (n > 0 && t[n - 1] == ' ')
        n--;
    t[n] = 0;
    return pooladd(t, n);
}

int openw(char *name)
{
    int i;
    for (i = 0; i < nw; i++)
        if (strcmp(wname[i], name) == 0)
            return i;
    if (nw >= NW)
        die("too many w files", 0);
    if (strcmp(name, "/dev/stdout") == 0)
        wfile[nw] = stdout;
    else if (strcmp(name, "/dev/stderr") == 0)
        wfile[nw] = stderr;
    else {
        wfile[nw] = fopen(name, "w");
        if (!wfile[nw])
            die("cannot open it", name);
    }
    wname[nw] = pool + pooladd(name, strlen(name));
    return nw++;
}

/* the replacement of an s into the pool: 1 n a group, 2 \U 3 \L 4 \E 5 \u 6 \l */
int getrepl(int d)
{
    char t[MAXL];
    int n, c;
    n = 0;
    while (*sp && *sp != d) {
        c = *sp++;
        if (c == '\\' && *sp) {
            c = *sp++;
            if (c >= '0' && c <= '9') {
                t[n++] = 1;
                t[n++] = c;
                continue;
            }
            if (c == 'n')
                c = '\n';
            else if (c == 't')
                c = '\t';
            else if (c == 'U' || c == 'L' || c == 'E' || c == 'u' || c == 'l') {
                t[n++] = c == 'U' ? 2 : (c == 'L' ? 3 : (c == 'E' ? 4 : (c == 'u' ? 5 : 6)));
                continue;
            }
        } else if (c == '&') {
            t[n++] = 1;
            t[n++] = '0';
            continue;
        }
        if (n < MAXL - 3)
            t[n++] = c;
    }
    if (*sp != d)
        die("unterminated s command", 0);
    sp++;
    return pooladd(t, n);
}

int getsflags(int i)
{
    int icase;
    icase = 0;
    for (;;) {
        if (*sp == 'g')
            sflag[i] |= 1;
        else if (*sp == 'p')
            sflag[i] |= 2;
        else if (*sp == 'i' || *sp == 'I')
            icase = 1;
        else if (*sp == 'm' || *sp == 'M' || *sp == 'e')
            ;
        else if (isdigit(*sp)) {
            snth[i] = getnum();
            continue;
        } else if (*sp == 'w') {
            sp++;
            sflag[i] |= 4;
            sfile[i] = openw(pool + getfname());
            break;
        } else
            break;
        sp++;
    }
    return icase;
}

void parsescript(void)
{
    char pat[200];
    char t[MAXL];
    int i, d, k, blk[20], nb, c, icase, r;
    sp = script;
    nb = 0;
    if (sp[0] == '#' && sp[1] == 'n' && (sp[2] == '\n' || !sp[2]))
        optn = 1;
    for (;;) {
        while (*sp == ' ' || *sp == '\t' || *sp == '\n' || *sp == ';')
            sp++;
        if (!*sp)
            break;
        if (*sp == '#') {
            while (*sp && *sp != '\n')
                sp++;
            continue;
        }
        if (ncmd >= NCMD)
            die("the script has too many commands", 0);
        i = ncmd;
        sre[i] = -1;
        if (address(&a1t[i], &a1v[i], &a1w[i])) {
            if (a1t[i] == 1 && a1v[i] == 0 && *sp == ',') {
                a1t[i] = 5;             /* 0,/re/ */
                rstate[i] = 1;
            }
            skipws();
            if (*sp == ',') {
                sp++;
                skipws();
                if (*sp == '+' || *sp == '~') {
                    a2t[i] = *sp == '+' ? 6 : 7;
                    sp++;
                    a2v[i] = getnum();
                } else if (!address(&a2t[i], &a2v[i], &d))
                    die("unexpected `,'", 0);
                if (a2t[i] == 4)
                    die("bad second address", 0);
            }
        }
        skipws();
        while (*sp == '!') {
            neg[i] = 1;
            sp++;
            skipws();
        }
        c = *sp++;
        ct[i] = c;
        switch (c) {
        case '{':
            blk[nb++] = i;
            break;
        case '}':
            if (nb == 0)
                die("unexpected `}'", 0);
            tgt[blk[--nb]] = i;
            break;
        case '=': case 'd': case 'D': case 'g': case 'G': case 'h': case 'H': case 'n': case 'N':
        case 'p': case 'P': case 'x': case 'z': case 'l':
            if (c == 'l')
                while (isdigit(*sp) || *sp == ' ')
                    sp++;
            break;
        case 'q': case 'Q':
            skipws();
            a1w[i] = isdigit(*sp) ? getnum() : 0;
            break;
        case 'a': case 'i': case 'c':
            sarg[i] = gettext();
            break;
        case 'r': case 'w':
            sarg[i] = getfname();
            if (c == 'w')
                sfile[i] = openw(pool + sarg[i]);
            break;
        case ':':
            skipws();
            k = 0;
            while (*sp && *sp != '\n' && *sp != ';' && *sp != ' ' && k < 39)
                t[k++] = *sp++;
            t[k] = 0;
            if (nlabels >= 40)
                die("too many labels", 0);
            labels[nlabels] = pool + pooladd(t, k);
            labcmd[nlabels++] = i;
            break;
        case 'b': case 't': case 'T':
            skipws();
            k = 0;
            while (*sp && *sp != '\n' && *sp != ';' && *sp != '}' && *sp != ' ' && k < 39)
                t[k++] = *sp++;
            t[k] = 0;
            sarg[i] = pooladd(t, k);
            break;
        case 's':
            d = *sp++;
            if (!d || d == '\n' || d == '\\')
                die("bad s command", 0);
            if (!delimited(d, pat, 200))
                die("unterminated s command", 0);
            sre[i] = -1;
            sarg2[i] = pooladd(pat, strlen(pat));
            sarg[i] = getrepl(d);
            icase = getsflags(i);
            r = newre(pool + sarg2[i], icase);
            sre[i] = r;
            break;
        case 'y':
            d = *sp++;
            if (!delimited(d, pat, 200))
                die("unterminated y command", 0);
            sarg[i] = pooladd(pat, strlen(pat));
            if (!delimited(d, pat, 200))
                die("unterminated y command", 0);
            sarg2[i] = pooladd(pat, strlen(pat));
            if (strlen(pool + sarg[i]) != strlen(pool + sarg2[i]))
                die("strings for y command are different lengths", 0);
            break;
        default:
            die("unknown command", "?");
        }
        ncmd++;
        skipws();
        if (*sp && *sp != '\n' && *sp != ';' && *sp != '}' && c != '{' && c != 'a' && c != 'i' && c != 'c')
            if (c != 's' && c != 'y' && c != 'r' && c != 'w' && c != ':' && c != 'b' && c != 't' && c != 'T')
                die("extra characters after command", 0);
    }
    if (nb)
        die("unmatched `{'", 0);
    for (i = 0; i < ncmd; i++)
        if (ct[i] == 'b' || ct[i] == 't' || ct[i] == 'T') {
            tgt[i] = ncmd;              /* no label: the end of the script */
            if (pool[sarg[i]])
                for (k = 0; k < nlabels; k++)
                    if (strcmp(labels[k], pool + sarg[i]) == 0) {
                        tgt[i] = labcmd[k];
                        break;
                    }
            if (pool[sarg[i]] && tgt[i] == ncmd)
                die("can't find label for jump to", pool + sarg[i]);
        }
}

/* ---- the input ---- */

/* the next line of the input into buf (no newline); 0 at the end of it all */
int fillraw(char *buf)
{
    int n;
    for (;;) {
        if (!fin) {
            if (curfile >= nfiles)
                return 0;
            if (strcmp(files[curfile], "-") == 0)
                fin = stdin;
            else {
                fin = fopen(files[curfile], "r");
                if (!fin) {
                    fprintf(stderr, "sed: %s: cannot open it\n", files[curfile]);
                    status = 2;
                    curfile++;
                    continue;
                }
            }
            curfile++;
        }
        if (fgets(buf, MAXL, fin)) {
            n = strlen(buf);
            if (n > 0 && buf[n - 1] == '\n')
                buf[n - 1] = 0;
            return 1;
        }
        if (fin != stdin)
            fclose(fin);
        fin = 0;
        if (opts)
            return 0;                   /* separate files: this one is over */
    }
}

int getline_(char *buf)
{
    if (havelook) {
        strcpy(buf, look);
        havelook = 0;
        return 1;
    }
    return fillraw(buf);
}

int islast(void)
{
    if (!havelook)
        havelook = fillraw(look);
    return !havelook;
}

/* ---- running ---- */

char *aq[40];                           /* what a, r and R append at the end of the cycle: text, or a file name */
int aqf[40];                            /* 1 a text, 2 r file */
int naq;

void flushappends(void)
{
    int i, c;
    FILE *f;
    for (i = 0; i < naq; i++) {
        if (aqf[i] == 1)
            fputs(aq[i], out);
        else {
            f = fopen(aq[i], "r");
            if (f) {
                while ((c = fgetc(f)) != EOF)
                    fputc(c, out);
                fclose(f);
            }
        }
    }
    naq = 0;
}

void putps(FILE *f, char *s)
{
    fputs(s, f);
    fputc('\n', f);
}

int lre(int r)
{
    if (r < 0)
        return lastre;
    lastre = r;
    return r;
}

int addrmatch(int t, int v, int w)
{
    int r;
    if (t == 1)
        return lineno == v;
    if (t == 2)
        return islast();
    if (t == 3) {
        r = lre(v);
        return r >= 0 && re_exec(r, ps, 0);
    }
    if (t == 4)
        return w == 0 ? lineno == v : (lineno >= v && (lineno - v) % w == 0);
    return 0;
}

int selected(int i)
{
    int hit;
    if (a1t[i] == 0)
        hit = 1;
    else if (a2t[i] == 0)
        hit = addrmatch(a1t[i], a1v[i], a1w[i]);
    else if (rstate[i]) {               /* in a range: does it end here? */
        hit = 1;
        if (a2t[i] == 1 || a2t[i] == 6 || a2t[i] == 7) {
            if (lineno >= rend[i])
                rstate[i] = 0;
        } else if (addrmatch(a2t[i], a2v[i], 0))
            rstate[i] = 0;
    } else if (a1t[i] == 5)
        hit = 0;
    else if (addrmatch(a1t[i], a1v[i], a1w[i])) {
        hit = 1;
        rstate[i] = 1;
        if (a2t[i] == 1)
            rend[i] = a2v[i];
        else if (a2t[i] == 6)
            rend[i] = lineno + a2v[i];
        else if (a2t[i] == 7)
            rend[i] = a2v[i] > 0 ? ((lineno + a2v[i] - 1) / a2v[i]) * a2v[i] : lineno;
        if ((a2t[i] == 1 || a2t[i] == 6 || a2t[i] == 7) && lineno >= rend[i])
            rstate[i] = 0;              /* one line */
        if (a2t[i] == 2 && islast())
            rstate[i] = 0;
    } else
        hit = 0;
    return neg[i] ? !hit : hit;
}

/* the replacement of s command i for the match, appended to tmp at *n */
void addrepl(int i, int *n, int ms, int me, int *casemode, int *once)
{
    char *r;
    int c, g, s, e, k;
    r = pool + sarg[i];
    while (*r) {
        c = *r++ & 255;
        if (c == 1) {
            g = *r++ - '0';
            if (g == 0) {
                s = ms;
                e = me;
            } else {
                s = re_cs[g];
                e = re_ce[g];
            }
            if (g > 9 || s < 0)
                continue;
            for (k = s; k < e && *n < MAXL - 1; k++) {
                c = ps[k] & 255;
                if (*once) {
                    c = *once == 5 ? toupper(c) : tolower(c);
                    *once = 0;
                } else if (*casemode)
                    c = *casemode == 2 ? toupper(c) : tolower(c);
                tmp[(*n)++] = c;
            }
        } else if (c == 2 || c == 3) {
            *casemode = c;
        } else if (c == 4) {
            *casemode = 0;
            *once = 0;
        } else if (c == 5 || c == 6) {
            *once = c;
        } else if (*n < MAXL - 1) {
            if (*once) {
                c = *once == 5 ? toupper(c) : tolower(c);
                *once = 0;
            } else if (*casemode)
                c = *casemode == 2 ? toupper(c) : tolower(c);
            tmp[(*n)++] = c;
        }
    }
}

/* the s command: 1 if it replaced */
int subst(int i)
{
    int r, pos, count, n, len, did, lastend, casemode, once, ms, me;
    r = lre(sre[i]);
    if (r < 0)
        die("no previous regular expression", 0);
    len = strlen(ps);
    pos = 0;
    count = 0;
    n = 0;
    did = 0;
    lastend = -1;
    casemode = 0;
    once = 0;
    while (pos <= len && re_exec(r, ps, pos)) {
        ms = re_ms;
        me = re_me;
        if (ms == me && ms == lastend) {        /* an empty match right after a match: not one */
            if (ms >= len)
                break;
            while (pos <= ms && n < MAXL - 1)
                tmp[n++] = ps[pos++];
            continue;
        }
        count++;
        while (pos < ms && n < MAXL - 1)
            tmp[n++] = ps[pos++];
        if (count >= (snth[i] ? snth[i] : 1)) {
            addrepl(i, &n, ms, me, &casemode, &once);
            did = 1;
        } else
            for (pos = ms; pos < me && n < MAXL - 1; pos++)
                tmp[n++] = ps[pos];
        pos = me;
        lastend = me;
        if (did && !(sflag[i] & 1))
            break;
        if (ms == me) {
            if (me >= len)
                break;
            if (n < MAXL - 1)
                tmp[n++] = ps[pos];
            pos++;
        }
    }
    if (!did)
        return 0;
    while (pos <= len && ps[pos] && n < MAXL - 1)
        tmp[n++] = ps[pos++];
    tmp[n] = 0;
    strcpy(ps, tmp);
    return 1;
}

void listps(void)
{
    int c, col;
    char *p;
    char b[8];
    col = 0;
    for (p = ps; *p; p++) {
        c = *p & 255;
        if (c == '\\')
            strcpy(b, "\\\\");
        else if (c == '\n')
            strcpy(b, "\\n");
        else if (c == '\t')
            strcpy(b, "\\t");
        else if (c == '\b')
            strcpy(b, "\\b");
        else if (c == '\f')
            strcpy(b, "\\f");
        else if (c == '\r')
            strcpy(b, "\\r");
        else if (c == 7)
            strcpy(b, "\\a");
        else if (c == 11)
            strcpy(b, "\\v");
        else if (c < 32 || c >= 127)
            sprintf(b, "\\%03o", c);
        else {
            b[0] = c;
            b[1] = 0;
        }
        if (col + strlen(b) > 69) {
            fputs("\\\n", out);
            col = 0;
        }
        fputs(b, out);
        col = col + strlen(b);
    }
    fputs("$\n", out);
}

/* the cycles for the input there is; returns when it is over */
void cycles(void)
{
    int pc, c, norefill, sub, tflag, k;
    char *nl, *q;
    char ch;
    norefill = 0;
    tflag = 0;
    for (;;) {
        if (!norefill) {
            if (!getline_(ps))
                break;
            lineno++;
            tflag = 0;
        }
        norefill = 0;
        pc = 0;
        while (pc < ncmd) {
            c = ct[pc];
            if (c == ':' || c == '}') {
                pc++;
                continue;
            }
            if (!selected(pc)) {
                pc = c == '{' ? tgt[pc] + 1 : pc + 1;
                continue;
            }
            switch (c) {
            case '{':
                break;
            case '=':
                fprintf(out, "%ld\n", lineno);
                break;
            case 'a':
                if (naq < 40) {
                    aq[naq] = pool + sarg[pc];
                    aqf[naq++] = 1;
                }
                break;
            case 'i':
                fputs(pool + sarg[pc], out);
                break;
            case 'c':
                if (a2t[pc] == 0 || neg[pc] || !rstate[pc])
                    fputs(pool + sarg[pc], out);
                goto delete_;
            case 'd':
delete_:
                flushappends();
                pc = ncmd + 1;
                continue;
            case 'D':
                nl = strchr(ps, '\n');
                if (!nl)
                    goto delete_;
                strcpy(ps, nl + 1);
                norefill = 1;
                flushappends();
                pc = ncmd + 1;
                continue;
            case 'g':
                strcpy(ps, hs);
                break;
            case 'G':
                if (strlen(ps) + strlen(hs) + 2 < MAXL) {
                    strcat(ps, "\n");
                    strcat(ps, hs);
                }
                break;
            case 'h':
                strcpy(hs, ps);
                break;
            case 'H':
                if (strlen(ps) + strlen(hs) + 2 < MAXL) {
                    strcat(hs, "\n");
                    strcat(hs, ps);
                }
                break;
            case 'x':
                strcpy(tmp, ps);
                strcpy(ps, hs);
                strcpy(hs, tmp);
                break;
            case 'l':
                listps();
                break;
            case 'n':
                if (islast()) {
                    if (!optn)
                        putps(out, ps);
                    flushappends();
                    return;
                }
                if (!optn)
                    putps(out, ps);
                flushappends();
                getline_(ps);
                lineno++;
                break;
            case 'N':
                if (islast()) {
                    if (!optn)
                        putps(out, ps);
                    flushappends();
                    return;
                }
                flushappends();
                getline_(tmp);
                lineno++;
                if (strlen(ps) + strlen(tmp) + 2 < MAXL) {
                    strcat(ps, "\n");
                    strcat(ps, tmp);
                }
                break;
            case 'p':
                putps(out, ps);
                break;
            case 'P':
                nl = strchr(ps, '\n');
                if (nl) {
                    *nl = 0;
                    putps(out, ps);
                    *nl = '\n';
                } else
                    putps(out, ps);
                break;
            case 'q':
                if (!optn)
                    putps(out, ps);
                flushappends();
                status = a1w[pc];
                quitting = 1;
                return;
            case 'Q':
                status = a1w[pc];
                quitting = 1;
                return;
            case 'r':
                if (naq < 40) {
                    aq[naq] = pool + sarg[pc];
                    aqf[naq++] = 2;
                }
                break;
            case 's':
                sub = subst(pc);
                if (sub) {
                    tflag = 1;
                    if (sflag[pc] & 2)
                        putps(out, ps);
                    if (sflag[pc] & 4)
                        putps(wfile[sfile[pc]], ps);
                }
                break;
            case 'b':
                pc = tgt[pc];
                continue;
            case 't':
                if (tflag) {
                    tflag = 0;
                    pc = tgt[pc];
                    continue;
                }
                break;
            case 'T':
                if (!tflag) {
                    pc = tgt[pc];
                    continue;
                }
                tflag = 0;
                break;
            case 'w':
                putps(wfile[sfile[pc]], ps);
                break;
            case 'y':
                for (q = ps; *q; q++) {
                    k = 0;
                    while (pool[sarg[pc] + k] && pool[sarg[pc] + k] != *q)
                        k++;
                    if (pool[sarg[pc] + k])
                        *q = pool[sarg2[pc] + k];
                }
                break;
            case 'z':
                ps[0] = 0;
                break;
            }
            pc++;
        }
        if (pc == ncmd) {               /* the end of the script: print, then what was appended */
            if (!optn)
                putps(out, ps);
            flushappends();
        }
    }
}

/* ---- main ---- */

void addscript(char *s)
{
    int n;
    n = strlen(s);
    script = realloc(script, slen + n + 2);
    if (!script)
        die("out of memory", 0);
    memcpy(script + slen, s, n);
    slen = slen + n;
    script[slen++] = '\n';
    script[slen] = 0;
}

void addscriptfile(char *name)
{
    FILE *f;
    char b[MAXL];
    int n;
    f = strcmp(name, "-") == 0 ? stdin : fopen(name, "r");
    if (!f)
        die("cannot open the script file", name);
    while (fgets(b, MAXL, f)) {
        n = strlen(b);
        if (n > 0 && b[n - 1] == '\n')
            b[n - 1] = 0;
        addscript(b);
    }
    if (f != stdin)
        fclose(f);
}

/* the file edited in place: the output went to name.TMP, now the file is written again from it */
void finishinplace(char *name, char *tmpname)
{
    FILE *a, *b;
    int c, k;
    char bak[60];
    a = fopen(tmpname, "r");
    if (!a)
        return;
    if (suffix && *suffix) {
        k = strlen(name);               /* the suffix goes before the extension: B.TEXT, .BAK -> B.BAK.TEXT (still text) */
        while (k > 0 && name[k - 1] != '.' && name[k - 1] != ':')
            k--;
        if (k > 0 && name[k - 1] == '.')
            k--;
        else
            k = strlen(name);
        sprintf(bak, "%.*s%s%s", k, name, suffix, name + k);
        b = fopen(name, "r");
        if (b) {
            FILE *g;
            g = fopen(bak, "w");
            if (g) {
                while ((c = fgetc(b)) != EOF)
                    fputc(c, g);
                fclose(g);
            }
            fclose(b);
        }
    }
    b = fopen(name, "w");
    if (b) {
        while ((c = fgetc(a)) != EOF)
            fputc(c, b);
        fclose(b);
    }
    fclose(a);
    remove(tmpname);
}

int main(int argc, char **argv)
{
    int i, k, haves, k2;
    char *a;
    char tmpname[40];
    FILE *real;
    haves = 0;
    for (i = 1; i < argc; i++) {
        a = argv[i];
        if (a[0] == '-' && a[1] && a[1] != '-' && strcmp(a, "-") != 0) {
            for (k = 1; a[k]; k++)
                switch (a[k]) {
                case 'n': optn = 1; break;
                case 'E': case 'r': ere = 1; break;
                case 's': opts = 1; break;
                case 'u': case 'z': break;
                case 'i':
                    opti = 1;
                    opts = 1;
                    suffix = a + k + 1;
                    k = strlen(a) - 1;
                    break;
                case 'e':
                    if (a[k + 1])
                        addscript(a + k + 1);
                    else if (i + 1 < argc)
                        addscript(argv[++i]);
                    else
                        die("option requires an argument -- 'e'", 0);
                    haves = 1;
                    k = strlen(a) - 1;
                    break;
                case 'f':
                    if (a[k + 1])
                        addscriptfile(a + k + 1);
                    else if (i + 1 < argc)
                        addscriptfile(argv[++i]);
                    else
                        die("option requires an argument -- 'f'", 0);
                    haves = 1;
                    k = strlen(a) - 1;
                    break;
                default:
                    fprintf(stderr, "sed: invalid option -- '%c'\n", a[k]);
                    fprintf(stderr, "use: SED [-nEsur] [-i[SUFFIX]] [-e SCRIPT] [-f FILE] [SCRIPT] [FILE ...]\n");
                    return 1;
                }
        } else if (!haves) {
            addscript(a);
            haves = 1;
        } else if (nfiles < NFILES)
            files[nfiles++] = a;
    }
    if (!haves) {
        fprintf(stderr, "use: SED [-nEsur] [-i[SUFFIX]] [-e SCRIPT] [-f FILE] [SCRIPT] [FILE ...]\n");
        return 1;
    }
    ps = malloc(MAXL + 4);
    hs = malloc(MAXL + 4);
    tmp = malloc(MAXL + 4);
    look = malloc(MAXL + 4);
    if (!ps || !hs || !tmp || !look)
        die("out of memory", 0);
    ps[0] = hs[0] = tmp[0] = look[0] = 0;
    parsescript();
    out = stdout;
    if (nfiles == 0) {
        if (opti)
            die("no input files", 0);
        files[nfiles++] = "-";
    }
    if (opti) {
        real = stdout;
        for (i = 0; i < nfiles && !quitting; i++) {
            sprintf(tmpname, "SEDTMP.TEXT");
            out = fopen(tmpname, "w");
            if (!out)
                die("cannot open the work file", tmpname);
            nfiles = nfiles;
            curfile = i;
            lineno = 0;
            fin = 0;
            havelook = 0;
            k2 = nfiles;
            nfiles = i + 1;
            cycles();
            nfiles = k2;
            if (fin && fin != stdin)
                fclose(fin);
            fin = 0;
            fclose(out);
            finishinplace(files[i], tmpname);
        }
    } else if (opts) {
        for (i = 0; i < nfiles && !quitting; i++) {
            k2 = nfiles;
            curfile = i;
            nfiles = i + 1;
            lineno = 0;
            fin = 0;
            havelook = 0;
            cycles();
            if (fin && fin != stdin)
                fclose(fin);
            fin = 0;
            nfiles = k2;
        }
    } else
        cycles();
    for (i = 0; i < nw; i++)
        if (wfile[i] != stdout && wfile[i] != stderr)
            fclose(wfile[i]);
    return status;
}
