/* less.c -- LESS [-NSiF] [+N] [FILE ...]: the files a screen at a time, forwards and backwards
   (the standard input when it was redirected: DIR | LESS, LESS <FILE).

   -N  line numbers    -S  chop long lines (else they wrap)    -i  searches ignore case
   -F  quit at once if the file fits on the screen             +N  start at line N

   Keys (a number before a command is its count):
     SPACE f ^F z PgDn   forward a screen      b ^B w PgUp   back a screen
     RETURN j e ^N Down  forward a line        k y ^Y ^P Up  back a line   (mouse wheel: 3 lines)
     d ^D  half a screen forward               u ^U          half a screen back
     g < Home  the first line (Ng: line N)     G > End       the last line (NG: line N)
     p %  N% of the way into the file
     /text  ?text   search forward / back (the line is put at the top), n and N again
     :n :p :x  the next / previous / first file       = ^G :f  where you are
     r ^R ^L  draw again       h  help       q Q :q ZZ  quit
   The file is read as far as the keys need it and a sparse index of line starts lets you go
   back anywhere at once: any size of file, a screen of memory.  .TEXT, .C and .H files are text;
   any other file is read as the bytes it holds (a line ends at a 0A).  Tabs go to a multiple of 8,
   control characters show as ^X, bytes of 128 and up as M-x.  When the output is not the screen
   (less >FILE) it is just the files.  Run it from the shell: LESS A.C, DIR | LESS -N. */
#pragma segment MAIN	/* calls within the program: 2-byte CGPs, no NOP after them */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <conio.h>
#include <psys.h>

#define MAXL 1090
#define WIN 128                         /* line starts remembered around the screen */
#define NIDX 256                        /* sparse index of line starts */
#define MAXROWS 64

FILE *f;
char *names[20];
int nfiles, cur;
int istext;
int fblocks;                            /* the file's blocks (header included) */
int rows = 24, cols = 80, th;           /* th: the text rows */
int optN, optS, optI, optF;

unsigned char pg[1024];
long pgno = -1;
int pglen;
char buf[MAXL + 8];
int blen;
char disp[2 * MAXL + 40];

/* ---- the file: pages, lines ---- */

/* page n (2 blocks: 1 KB) into pg; 0 at the end of the file */
int pgread(long n)
{
    int nb;
    unsigned char *z;
    if (n == pgno)
        return 1;
    nb = __cxp0i(28, f->fib, pg, 0, 2, (int)((istext ? 2 : 0) + 2 * n), 1, 0, 0);
    if (nb <= 0 || (long)(istext ? 2 : 0) + 2 * n >= fblocks) {
        pgno = -1;
        return 0;
    }
    if (nb > 2)
        nb = 2;
    pgno = n;
    pglen = nb * 512;
    if (istext) {
        z = memchr(pg, 0, pglen);       /* the NULs fill the rest of a page */
        if (z)
            pglen = z - pg;
    }
    return 1;
}

/* the page and offset of the line at p, moving on over empty pages; 0 if there is no line */
long where(long p, int *off)
{
    long n;
    for (;;) {
        n = p / 1024;
        *off = (int)(p % 1024);
        if (!pgread(n))
            return -1;
        if (*off < pglen)
            return n;
        p = (n + 1) * 1024;
    }
}

/* the start of the line after the one at p; -1 if there is no line at p */
long skipline(long p)
{
    long n;
    int off;
    unsigned char *e;
    n = where(p, &off);
    if (n < 0)
        return -1;
    e = memchr(pg + off, istext ? 13 : 10, pglen - off);
    return e ? n * 1024 + (e - pg) + 1 : (n + 1) * 1024;
}

/* the line at p into buf (blen bytes); its next line in *np; 0 at the end */
int rdline(long p, long *np)
{
    long n;
    int off, i, c, k;
    n = where(p, &off);
    if (n < 0)
        return 0;
    blen = 0;
    i = off;
    while (i < pglen) {
        c = pg[i++];
        if (istext) {
            if (c == 13)
                break;
            if (c == 16 && i < pglen) {     /* DLE n: n - 32 blanks */
                k = pg[i++] - 32;
                while (k-- > 0 && blen < MAXL)
                    buf[blen++] = ' ';
                continue;
            }
        } else if (c == 10)
            break;
        if (blen < MAXL)
            buf[blen++] = c;
    }
    buf[blen] = 0;
    *np = n * 1024 + i;
    return 1;
}

/* ---- the line index: starts of lines ---- */

long idx[NIDX];                         /* idx[k]: the start of line k * step */
int step;
long cl, cp;                            /* the frontier: line cl starts at cp; none beyond is known */
int eof;
long total;                             /* the lines, when eof */
long wpos[WIN];                         /* lines wlo..whi: their starts, at wpos[n % WIN] */
long wlo, whi;

void newfile(void)
{
    step = 1;
    idx[0] = 0;
    cl = 0;
    cp = 0;
    eof = 0;
    total = 0;
    wlo = 0;
    whi = -1;
    pgno = -1;
}

/* the start of line n (from 0); -1 if the file has no such line */
long linepos(long n)
{
    long k, p, q;
    int i;
    if (n < 0)
        n = 0;
    if (n >= wlo && n <= whi)
        return wpos[n % WIN];
    if (eof && n >= total)
        return -1;
    if (whi >= 0 && n > whi && n - whi < 600) {
        k = whi;
        p = wpos[whi % WIN];
    } else if (n >= cl) {
        k = cl;
        p = cp;
    } else {
        k = (n / step) * step;
        p = idx[n / step];
    }
    for (;;) {
        if (k == whi + 1 && k - wlo < WIN)
            whi = k;
        else if (k == whi + 1) {
            whi = k;
            wlo = k - WIN + 1;
        } else if (k < wlo || k > whi + 1) {
            wlo = k;
            whi = k;
        }
        wpos[k % WIN] = p;
        q = skipline(p);
        if (q < 0) {                    /* no line k: the file has k lines */
            eof = 1;
            total = k;
            if (whi == k)
                whi = k - 1;
            return -1;
        }
        if (k >= cl) {
            cl = k;
            cp = p;
        }
        if (k == n)
            return p;
        k++;
        p = q;
        if (k > cl) {
            cl = k;
            cp = p;
            if (k % step == 0) {
                if (k / step >= NIDX) {         /* the index is full: every other entry, a double step */
                    for (i = 0; i < NIDX / 2; i++)
                        idx[i] = idx[2 * i];
                    step = step * 2;
                }
                if (k % step == 0)
                    idx[k / step] = p;
            }
        }
    }
}

/* the file's line count: reads to the end */
long nlines(void)
{
    if (!eof)
        linepos(1000000000L);
    return total;
}

/* ---- the screen ---- */

unsigned rowsum[MAXROWS];
int prevok, prevwrap;
long prevtop;

void gotoxy_(int x, int y)
{
    putc(1, stdout);
    putc(x + 32, stdout);
    putc(y + 32, stdout);
}

void reverse(int on)
{
    int i;
    for (i = 0; i < 4; i++)
        putc(on ? 15 : 14, stdout);
}

/* buf[0..n) as it is shown into disp: tabs, ^X, M-x; returns its length */
int expand(int n)
{
    int c, d, i;
    d = 0;
    for (i = 0; i < n; i++) {
        c = buf[i] & 255;
        if (c == '\t') {
            do
                disp[d++] = ' ';
            while (d % 8 != 0);
            continue;
        }
        if (c >= 128) {
            disp[d++] = 'M';
            disp[d++] = '-';
            c = c - 128;
        }
        if (c < 32) {
            disp[d++] = '^';
            disp[d++] = c + 64;
        } else if (c == 127) {
            disp[d++] = '^';
            disp[d++] = '?';
        } else
            disp[d++] = c;
    }
    disp[d] = 0;
    return d;
}

/* row r shows s (n characters) */
void emitrow(int r, char *s, int n)
{
    unsigned sum;
    int i;
    char c;
    sum = 0;
    for (i = 0; i < n; i++)
        sum = sum * 31 + (unsigned char)s[i];
    sum = (sum & ~1) + 0;
    if (sum == rowsum[r])
        return;
    rowsum[r] = sum;
    gotoxy_(0, r);
    c = s[n];
    s[n] = 0;
    fputs(s, stdout);
    s[n] = c;
    putc(SYSCOM->crtctrl.eraseeol, stdout);
}

/* the rows a line takes */
int rowsof(int n)
{
    int d, w;
    d = expand(n);
    w = cols - 1 - (optN ? 7 : 0);
    if (optS || d <= w)
        return 1;
    return (d + w - 1) / w;
}

char rowtxt[400];

/* the screen from line top; returns the last line shown (and *full: the end of the file is on it) */
long draw(long top, int *full)
{
    long ln, p, np;
    int r, d, w, k, n, lim, wrapped;
    char num[16];
    r = 0;
    ln = top;
    wrapped = 0;
    *full = 0;
    w = cols - 1 - (optN ? 7 : 0);
    p = linepos(top);
    while (r < th) {
        if (p < 0) {
            *full = 1;
            emitrow(r++, "~", 1);
            continue;
        }
        if (!rdline(p, &np)) {
            p = -1;
            continue;
        }
        d = expand(blen);
        for (k = 0; k == 0 || (k < d && !optS); k = k + w) {
            if (r >= th)
                break;
            n = d - k;
            if (n > w || optS && n > w) {
                n = w;
                if (!optS)
                    wrapped = 1;
            }
            lim = 0;
            if (optN) {
                if (k == 0)
                    sprintf(rowtxt, "%6ld ", ln + 1);
                else
                    strcpy(rowtxt, "       ");
                lim = 7;
            }
            memcpy(rowtxt + lim, disp + k, n);
            rowtxt[lim + n] = 0;
            emitrow(r++, rowtxt, lim + n);
            if (optS)
                break;
        }
        ln++;
        p = np;
        if (p >= 0 && r >= th)
            break;
        if (p >= 0 && linepos(ln) < 0)
            p = -1;
    }
    if (ln > top && (p < 0 || linepos(ln) < 0))
        *full = 1;
    prevwrap = wrapped;
    return ln - 1;
}

char msg[100];
int msgset;

void status(long top, long last, int full)
{
    char s[140];
    long t;
    int pct, n;
    if (msgset)
        strcpy(s, msg);
    else {
        t = eof ? total : 0;
        if (full)
            pct = 100;
        else if (eof && total > 0)
            pct = (int)((top + 1) * 100 / total);
        else {
            n = (fblocks - (istext ? 2 : 0)) / 2;
            pct = n > 0 ? (int)((linepos(top) / 1024) * 100 / n) : 0;
        }
        if (full && last >= top)
            sprintf(s, "%s lines %ld-%ld/%ld (END)", names[cur], top + 1, last + 1, eof ? total : last + 1);
        else if (eof)
            sprintf(s, "%s lines %ld-%ld/%ld %d%%", names[cur], top + 1, last + 1, total, pct);
        else
            sprintf(s, "%s lines %ld-%ld  ~%d%%", names[cur], top + 1, last + 1, pct);
        if (nfiles > 1)
            sprintf(s + strlen(s), "  (file %d of %d)", cur + 1, nfiles);
    }
    n = strlen(s);
    if (n > cols - 2)
        n = cols - 2;
    s[n] = 0;
    gotoxy_(0, rows - 1);
    reverse(1);
    fputs(s, stdout);
    reverse(0);
    putc(SYSCOM->crtctrl.eraseeol, stdout);
    rowsum[MAXROWS - 1] = 1;
}

/* the screen after the top line moved: the console scrolls the rows that stay (no wrapped lines) */
void screen(long top, long *last, int *full)
{
    long d;
    int k, n;
    if (prevok && !prevwrap && !optN || prevok && !prevwrap) {
        d = top - prevtop;
        if (d != 0 && d > -th && d < th) {
            n = (int)(d < 0 ? -d : d);
            printf("\033[1;%dr\033[%d%c\033[r", th, n, d < 0 ? 'T' : 'S');
            if (d > 0)
                memmove(rowsum, rowsum + n, (th - n) * sizeof(unsigned));
            else
                memmove(rowsum + n, rowsum, (th - n) * sizeof(unsigned));
            for (k = 0; k < n; k++)
                rowsum[d > 0 ? th - 1 - k : k] = 1;
        }
    }
    *last = draw(top, full);
    prevtop = top;
    prevok = 1;
}

void invalidate(void)
{
    int i;
    for (i = 0; i < MAXROWS; i++)
        rowsum[i] = 1;
    prevok = 0;
}

/* ---- keys and commands ---- */

int key(void)
{
    int c;
    fflush(stdout);
    c = getch();
    if (c == SYSCOM->crtinfo.down)
        return 'j';
    if (c == SYSCOM->crtinfo.up)
        return 'k';
    if (c == KEY_PGDN)
        return 'f';
    if (c == KEY_PGUP)
        return 'b';
    if (c == KEY_HOME)
        return 'g';
    if (c == KEY_END)
        return 'G';
    if (c == KEY_WHEELDN)
        return 0x101;
    if (c == KEY_WHEELUP)
        return 0x102;
    return c;
}

char pat[80];
int patdir = 1;

int contains(char *s, int n, char *p)
{
    int m, i, j;
    m = strlen(p);
    for (i = 0; i + m <= n; i++) {
        for (j = 0; j < m; j++)
            if (optI ? tolower(s[i + j]) != tolower(p[j]) : s[i + j] != p[j])
                break;
        if (j == m)
            return 1;
    }
    return 0;
}

/* the first line after (dir 1) or before (-1) line from with the pattern; -1 if none */
long search(long from, int dir)
{
    long n, p, np;
    for (n = from; n >= 0; n = n + dir) {
        p = linepos(n);
        if (p < 0)
            return -1;
        rdline(p, &np);
        if (contains(buf, blen, pat))
            return n;
    }
    return -1;
}

/* the line numbered from the screen row: reads a line of text after prompt ch into s; 0 if ESC */
int readline_(int ch, char *s, int max)
{
    int n, c;
    gotoxy_(0, rows - 1);
    putc(ch, stdout);
    putc(SYSCOM->crtctrl.eraseeol, stdout);
    n = 0;
    for (;;) {
        fflush(stdout);
        c = getch();
        if (c == '\r' || c == '\n' || c == 10)
            break;
        if (c == 27)
            return 0;
        if ((c == 8 || c == 127) && n > 0) {
            n--;
            putc(8, stdout);
            putc(' ', stdout);
            putc(8, stdout);
        } else if (c >= 32 && c < 127 && n < max - 1) {
            s[n++] = c;
            putc(c, stdout);
        }
    }
    s[n] = 0;
    return 1;
}

int openfile(int i)
{
    FILE *g;
    if (names[i][0] == '-' && names[i][1] == 0)
        g = stdin;
    else
        g = fopen(names[i], "r");
    if (!g)
        return 0;
    if (f && f != stdin && f != g)
        fclose(f);
    f = g;
    istext = (f->flags & __F_TEXT) != 0;
    fblocks = ((int *)f->fib)[17] - ((int *)f->fib)[16];
    newfile();
    return 1;
}

/* the last line to start a screen whose end is the file's: lines from the end up to a screenful */
long maxtop(void)
{
    long t, p, np;
    int rws, r;
    nlines();
    t = total - 1;
    rws = 0;
    while (t >= 0) {
        p = linepos(t);
        if (p < 0)
            break;
        rdline(p, &np);
        r = rowsof(blen);
        if (rws + r > th)
            break;
        rws = rws + r;
        t--;
    }
    return t + 1 > 0 ? t + 1 : 0;
}

void help(void)
{
    static char *h[] = {
        "SPACE f ^F z  forward a screen     b ^B w  back a screen",
        "RETURN j e    forward a line       k y     back a line     (a number first: that many)",
        "d ^D / u ^U   half a screen        g < / G >   first / last line    Ng NG: line N",
        "/text ?text   search forward/back, n N again     N%  that far into the file",
        ":n :p :x      next / previous / first file       = ^G    where you are",
        "r ^R ^L       draw again           h  this help           q  quit",
        0
    };
    int i;
    invalidate();
    gotoxy_(0, 0);
    putc(SYSCOM->crtctrl.eraseeos, stdout);
    for (i = 0; h[i]; i++) {
        gotoxy_(0, i);
        fputs(h[i], stdout);
    }
    gotoxy_(0, rows - 1);
    reverse(1);
    fputs("Press any key to go on", stdout);
    reverse(0);
    fflush(stdout);
    getch();
    gotoxy_(0, 0);
    putc(SYSCOM->crtctrl.eraseeos, stdout);
}

void dumpfile(void)                     /* not the screen: the whole file */
{
    long p, np;
    while (rdline(0, &np)) {
        fputs(buf, stdout);
        fputs("\n", stdout);
        break;
    }
    p = np;
    while (rdline(p, &np)) {
        fputs(buf, stdout);
        fputs("\n", stdout);
        p = np;
    }
}

int main(int argc, char **argv)
{
    int i, c, full, count, quit, k, ok;
    long top, last, t, m, lines;
    char *a;
    char tmp[84];
    long startline;
    startline = 0;
    for (i = 1; i < argc; i++) {
        a = argv[i];
        if (a[0] == '+' && isdigit(a[1]))
            startline = atol(a + 1) - 1;
        else if (a[0] == '-' && a[1] != 0 && a[1] != '-') {
            for (k = 1; a[k]; k++)
                switch (a[k]) {
                case 'N': optN = 1; break;
                case 'S': optS = 1; break;
                case 'i': optI = 1; break;
                case 'F': optF = 1; break;
                case 'e': case 'X': case 'n': case 'q': case 'R': break;
                default:
                    fprintf(stderr, "less: invalid option -- '%c'\n", a[k]);
                    fprintf(stderr, "use: LESS [-NSiF] [+N] [FILE ...]\n");
                    return 1;
                }
        } else if (nfiles < 20)
            names[nfiles++] = a;
    }
    if (nfiles == 0) {
        if (stdin->flags & __F_CON) {
            fprintf(stderr, "less: no file (LESS FILE, or DIR | LESS)\n");
            return 1;
        }
        names[nfiles++] = "-";
    }
    if (SYSCOM->crtinfo.height > 0)
        rows = SYSCOM->crtinfo.height;
    if (SYSCOM->crtinfo.width > 0)
        cols = SYSCOM->crtinfo.width;
    if (rows > MAXROWS)
        rows = MAXROWS;
    th = rows - 1;
    f = 0;
    cur = 0;
    if (!openfile(0)) {
        fprintf(stderr, "less: %s: cannot open it\n", names[0]);
        return 1;
    }
    if (!(stdout->flags & __F_CON)) {   /* not the screen: the files */
        for (cur = 0; cur < nfiles; cur++)
            if (cur == 0 || openfile(cur))
                dumpfile();
        return 0;
    }
    if (optF) {                         /* -F: a file that fits is just shown */
        lines = nlines();
        if (lines <= th) {
            dumpfile();
            return 0;
        }
    }
    SYSCOM->expansion[1] = PX_KEYS_AHEAD;       /* scroll codes, PgUp and the wheel (emulator 2.04) */
    gotoxy_(0, 0);
    putc(SYSCOM->crtctrl.eraseeos, stdout);
    invalidate();
    top = startline;
    if (linepos(top) < 0)
        top = 0;
    quit = 0;
    count = 0;
    msgset = 0;
    while (!quit) {
        if (!SYSCOM->expansion[2]) {            /* keys waiting: no drawing until they are done */
            screen(top, &last, &full);
            status(top, last, full);
            fflush(stdout);
        }
        msgset = 0;
        c = key();
        if (c >= '0' && c <= '9' && !(c == '0' && count == 0 && 0)) {
            count = count * 10 + c - '0';
            continue;
        }
        t = top;
        switch (c) {
        case ' ': case 'f': case 6: case 'z':
            t = top + (count ? count : th - 1);
            break;
        case 'b': case 2: case 'w':
            t = top - (count ? count : th - 1);
            break;
        case '\r': case '\n': case 'j': case 14: case 'e': case 5:
            t = top + (count ? count : 1);
            break;
        case 0x101:
            t = top + 3;
            break;
        case 'k': case 11: case 25: case 'y': case 16:
            t = top - (count ? count : 1);
            break;
        case 0x102:
            t = top - 3;
            break;
        case 'd': case 4:
            t = top + (count ? count : th / 2);
            break;
        case 'u': case 21:
            t = top - (count ? count : th / 2);
            break;
        case 'g': case '<':
            t = count ? count - 1 : 0;
            break;
        case 'G': case '>':
            t = count ? count - 1 : maxtop();
            break;
        case 'p': case '%':
            lines = nlines();
            t = lines * (count > 100 ? 100 : count) / 100;
            break;
        case '/': case '?':
            if (readline_(c, tmp, 80)) {
                if (tmp[0])
                    strcpy(pat, tmp);
                patdir = c == '/' ? 1 : -1;
            } else
                break;
            /* fall through to the search */
        case 'n': case 'N':
            if (!pat[0]) {
                strcpy(msg, "No previous search");
                msgset = 1;
                break;
            }
            k = (c == 'N' ? -patdir : patdir);
            m = search(top + k, k);
            if (m < 0) {
                sprintf(msg, "Pattern not found: %s", pat);
                msgset = 1;
            } else
                t = m;
            break;
        case '=': case 7:
            sprintf(msg, "%s: line %ld of %s%ld", names[cur], top + 1, eof ? "" : "at least ", eof ? total : last + 1);
            msgset = 1;
            break;
        case 'r': case 18: case 12:
            invalidate();
            gotoxy_(0, 0);
            putc(SYSCOM->crtctrl.eraseeos, stdout);
            break;
        case 'h': case 'H':
            help();
            invalidate();
            break;
        case 'q': case 'Q': case 3:
            quit = 1;
            break;
        case 'Z':
            if (key() == 'Z')
                quit = 1;
            break;
        case ':':
            c = key();
            if (c == 'q' || c == 'Q')
                quit = 1;
            else if (c == 'f') {
                sprintf(msg, "%s: line %ld", names[cur], top + 1);
                msgset = 1;
            } else if (c == 'n' || c == 'p' || c == 'x') {
                k = c == 'n' ? cur + 1 : (c == 'p' ? cur - 1 : 0);
                if (k < 0 || k >= nfiles) {
                    strcpy(msg, c == 'n' ? "No next file" : "No previous file");
                    msgset = 1;
                } else if (!openfile(k)) {
                    sprintf(msg, "%s: cannot open it", names[k]);
                    msgset = 1;
                } else {
                    cur = k;
                    top = 0;
                    invalidate();
                    gotoxy_(0, 0);
                    putc(SYSCOM->crtctrl.eraseeos, stdout);
                }
            }
            break;
        }
        count = 0;
        if (t != top) {
            if (t < 0)
                t = 0;
            if (t > top) {                  /* forward: not past the screen that ends with the file */
                if (linepos(t + th) < 0 || eof) {
                    m = maxtop();
                    if (c != 'g' && c != '<' && c != '/' && c != '?' && c != 'n' && c != 'N' && t > m)
                        t = m < top ? top : m;
                }
            }
            if (linepos(t) < 0)
                t = top;
            top = t;
        }
    }
    gotoxy_(0, rows - 1);
    putc(SYSCOM->crtctrl.eraseeol, stdout);
    fflush(stdout);
    SYSCOM->expansion[1] = 0;
    return 0;
}
