/* more.c -- MORE [-N] [-s] [-i] [+N] [FILE ...]: the files a screen at a time (the standard
   input when there is no FILE or it is "-").

   -N  N lines to a screen (else the screen's height less the prompt line)
   -s  squeeze several empty lines into one
   -i  searches ignore case
   +N  start at line N

   At the --More-- prompt:  SPACE, f, ^F, z, PgDn: the next screen    RETURN, j, Down, mouse wheel
   (3 lines): the next line    d, ^D: half a screen    s: skip a screen's lines (forward)
   /text: the next line with the text (n: again)    =: the line number    h: help    q, Q, ESC: quit
   The percentage is how far into the file the screen is (files, and a standard input that was
   redirected from a file).  Tabs go to the next multiple of 8, control characters show as ^X and
   bytes of 128 and up as M-x; long lines wrap.  When the output is not the screen (more >FILE) it
   is just the files, as CAT.  Run it from the shell: MORE A.C, DIR | MORE, MORE -20 +100 A.C. */
#pragma segment MAIN	/* calls within the program: 2-byte CGPs, no NOP after them */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <conio.h>
#include <psys.h>

int rows = 24, cols = 80;
int perpage;                            /* lines to a screen */
int squeeze, nocase;
long startline;
long lineno;                            /* the number of the line shown last */
int shown;                              /* lines shown since the prompt */
int quit;
char pat[80];
int tty;                                /* the output is the screen */
char line[1100];
char disp[1400];
int lastempty;

void putstr(char *s)
{
    fputs(s, stdout);
}

/* the line as it is shown: tabs, control characters; returns its length */
int expand(char *s, char *d)
{
    int c, n;
    n = 0;
    while ((c = *s++) != 0) {
        c = c & 255;
        if (c == '\t') {
            do
                d[n++] = ' ';
            while (n % 8 != 0);
            continue;
        }
        if (c >= 128) {
            d[n++] = 'M';
            d[n++] = '-';
            c = c - 128;
        }
        if (c < 32) {
            d[n++] = '^';
            d[n++] = c + 64;
        } else if (c == 127) {
            d[n++] = '^';
            d[n++] = '?';
        } else
            d[n++] = c;
    }
    d[n] = 0;
    return n;
}

/* the line on the screen, wrapped at the width less 1; returns the rows it took */
int showline(char *s)
{
    int n, r, w, k;
    char c;
    n = expand(s, disp);
    w = cols - 1;
    r = 1;
    for (k = 0; k < n; k = k + w) {
        c = 0;
        if (n - k > w) {
            c = disp[k + w];
            disp[k + w] = 0;
        }
        putstr(disp + k);
        putstr("\n");
        if (c) {
            disp[k + w] = c;
            r++;
        }
    }
    if (n == 0)
        putstr("\n");
    return r;
}

void reverse(int on)
{
    int i;
    for (i = 0; i < 4; i++)
        putc(on ? 15 : 14, stdout);
}

/* the prompt of n characters is wiped out, the cursor where it began */
void clearprompt(int n)
{
    int i, bs;
    bs = SYSCOM->crtctrl.backspace ? SYSCOM->crtctrl.backspace : 8;
    for (i = 0; i < n; i++)
        putc(bs, stdout);
    for (i = 0; i < n; i++)
        putc(' ', stdout);
    for (i = 0; i < n; i++)
        putc(bs, stdout);
}

/* a key; the arrow keys, PgDn and the wheel as the letters that do the same */
int key(void)
{
    int c;
    fflush(stdout);
    c = getch();
    if (c == SYSCOM->crtinfo.down)
        return 'j';
    if (c == KEY_PGDN)
        return ' ';
    if (c == KEY_WHEELDN)
        return 3000;
    return c;
}

/* the prompt; what the user wants: the lines to show before the next prompt (-1: quit) */
int prompt(char *what, FILE *f)
{
    int c, n, i;
    char more[60];
    for (;;) {
        sprintf(more, "--More--");
        if (what)
            sprintf(more, "--More--(%s)", what);
        else if (f && !(f->flags & __F_CON) && f->fib && ((int *)f->fib)[17] - ((int *)f->fib)[16] > 2)
            sprintf(more, "--More--(%d%%)", (int)((long)f->blk * 100 / (((int *)f->fib)[17] - ((int *)f->fib)[16])));
        reverse(1);
        putstr(more);
        reverse(0);
        c = key();
        clearprompt(strlen(more));
        if (c == 'q' || c == 'Q' || c == 27 || c == 3)
            return -1;
        if (c == ' ' || c == 'f' || c == 6 || c == 'z')
            return perpage;
        if (c == '\r' || c == '\n' || c == 'j' || c == 10)
            return 1;
        if (c == 3000)
            return 3;
        if (c == 'd' || c == 4)
            return perpage / 2;
        if (c == 's')
            return -2;                  /* skip a screen's lines */
        if (c == '/' || c == 'n') {
            if (c == '/') {
                putstr("/");
                n = 0;
                for (;;) {
                    fflush(stdout);
                    c = getch();
                    if (c == '\r' || c == '\n' || c == 10)
                        break;
                    if (c == 27) {
                        n = -1;
                        break;
                    }
                    if ((c == 8 || c == 127) && n > 0) {
                        n--;
                        putc(8, stdout);
                        putc(' ', stdout);
                        putc(8, stdout);
                    } else if (c >= 32 && c < 127 && n < 78) {
                        pat[n++] = c;
                        putc(c, stdout);
                    }
                }
                clearprompt(n < 0 ? 1 + 78 : n + 1);
                if (n < 0)
                    continue;
                if (n > 0)
                    pat[n] = 0;
            }
            if (!pat[0]) {
                putstr("No previous regular expression\n");
                continue;
            }
            return -3;                  /* search */
        }
        if (c == '=') {
            sprintf(more, "line %ld", lineno);
            putstr(more);
            c = key();
            clearprompt(strlen(more));
            continue;
        }
        if (c == 'h' || c == '?') {
            putstr("SPACE next screen  RETURN next line  d half screen  s skip  /text search  n again  = line number  q quit\n");
            continue;
        }
    }
}

int contains(char *s, char *p)
{
    int n, m, i, j;
    n = strlen(s);
    m = strlen(p);
    for (i = 0; i + m <= n; i++) {
        for (j = 0; j < m; j++)
            if (nocase ? tolower(s[i + j]) != tolower(p[j]) : s[i + j] != p[j])
                break;
        if (j == m)
            return 1;
    }
    return 0;
}

/* the next line of f into line (without its newline); 0 at the end */
int getline(FILE *f)
{
    int n;
    if (!fgets(line, 1090, f))
        return 0;
    n = strlen(line);
    if (n > 0 && line[n - 1] == '\n')
        line[n - 1] = 0;
    return 1;
}

void more(FILE *f, char *name, int last)
{
    int room, r, want, searching, kind;
    char what[50];
    room = perpage;
    searching = 0;
    lineno = 0;
    lastempty = 0;
    while (!quit && getline(f)) {
        lineno++;
        if (lineno < startline)
            continue;
        if (squeeze && line[0] == 0) {
            if (lastempty)
                continue;
            lastempty = 1;
        } else
            lastempty = 0;
        if (searching) {
            if (!contains(line, pat))
                continue;
            searching = 0;
            putstr("...skipping\n");
            room = perpage - 1;
        } else if (!tty) {
            putstr(line);
            putstr("\n");
            continue;
        }
        if (room <= 0) {
            want = prompt(0, f);
            if (want == -1) {
                quit = 1;
                break;
            }
            if (want == -3) {
                searching = 1;
                if (!contains(line, pat)) {
                    continue;
                }
                searching = 0;
                putstr("...skipping\n");
                want = perpage - 1;
            }
            if (want == -2)
                want = perpage;
            room = want;
        }
        r = showline(line);
        room = room - r;
    }
    startline = 0;
}

int main(int argc, char **argv)
{
    int i, nf, n;
    FILE *f;
    char *a;
    char *names[20];
    SYSCOM->expansion[1] = PX_KEYS_AHEAD;     /* PgDn and the wheel as codes (emulator 2.04) */
    if (SYSCOM->crtinfo.height > 0)
        rows = SYSCOM->crtinfo.height;
    if (SYSCOM->crtinfo.width > 0)
        cols = SYSCOM->crtinfo.width;
    perpage = rows - 1;
    tty = (stdout->flags & __F_CON) != 0;
    nf = 0;
    for (i = 1; i < argc; i++) {
        a = argv[i];
        if (a[0] == '-' && isdigit(a[1]))
            perpage = atoi(a + 1);
        else if (strcmp(a, "-s") == 0)
            squeeze = 1;
        else if (strcmp(a, "-i") == 0)
            nocase = 1;
        else if (a[0] == '+' && isdigit(a[1]))
            startline = atol(a + 1);
        else if (a[0] == '-' && a[1] != 0) {
            fprintf(stderr, "use: MORE [-N] [-s] [-i] [+N] [FILE ...]\n");
            SYSCOM->expansion[1] = 0;
            return 1;
        } else if (nf < 20)
            names[nf++] = a;
    }
    if (perpage < 1)
        perpage = 1;
    if (nf == 0) {
        if (stdin->flags & __F_CON) {
            fprintf(stderr, "more: no file (MORE FILE, or DIR | MORE)\n");
            SYSCOM->expansion[1] = 0;
            return 1;
        }
        more(stdin, "(standard input)", 1);
    }
    for (i = 0; i < nf && !quit; i++) {
        f = strcmp(names[i], "-") == 0 ? stdin : fopen(names[i], "r");
        if (!f) {
            fprintf(stderr, "more: %s: cannot open it\n", names[i]);
            continue;
        }
        if (nf > 1 && tty) {
            putstr("::::::::::::::\n");
            putstr(names[i]);
            putstr("\n::::::::::::::\n");
        }
        more(f, names[i], i == nf - 1);
        if (f != stdin)
            fclose(f);
    }
    SYSCOM->expansion[1] = 0;
    return 0;
}
