/* man.c -- MAN [-k WORD] [-l] [NAME ...]: the manual pages of the programs on TOOLS:.

   The text is one file, MAN.TEXT, on the volume the MAN program was run from (next to MAN.CODE).
   An entry starts with a line  %% NAME [ALIAS ...]  and goes on to the next such line; its first
   line after the NAME heading is the one-line purpose ("    GREP - lines of text that match").
     MAN NAME ...   show the entries of the programs (case does not matter)
     MAN, MAN -l    the programs and their purposes
     MAN -k WORD    the programs whose entries have WORD in them
   On the screen a long entry stops at --More-- (SPACE a screen, RETURN a line, q quits); when the
   output is redirected or piped (MAN GREP >G.TEXT, MAN VI | LESS) it is just the text.
   Run it from the shell: MAN, MAN GREP, MAN -k hex. */
#pragma segment MAIN	/* calls within the program: 2-byte CGPs, no NOP after them */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <conio.h>
#include <psys.h>

int rows = 24;
int tty;
int room;                               /* lines left on the screen before --More-- */
int quit;
char line[300];
char names[200];
char summary[120];
char word[60];

void reverse(int on)
{
    int i;
    for (i = 0; i < 4; i++)
        putc(on ? 15 : 14, stdout);
}

/* one line out; at the end of a screen asks (tty only) */
void outline(char *s)
{
    int c, bs, i, n;
    if (quit)
        return;
    if (tty && room <= 0) {
        n = 52;
        bs = SYSCOM->crtctrl.backspace ? SYSCOM->crtctrl.backspace : 8;
        reverse(1);
        fputs("--More-- (SPACE a screen, RETURN a line, q quits)", stdout);
        reverse(0);
        fflush(stdout);
        c = getch();
        for (i = 0; i < n; i++)
            putc(bs, stdout);
        for (i = 0; i < n; i++)
            putc(' ', stdout);
        for (i = 0; i < n; i++)
            putc(bs, stdout);
        if (c == 'q' || c == 'Q' || c == 27 || c == 3) {
            quit = 1;
            return;
        }
        room = (c == '\r' || c == '\n') ? 1 : rows - 1;
    }
    fputs(s, stdout);
    fputs("\n", stdout);
    room--;
}

/* does s hold w (case ignored) */
int has(char *s, char *w)
{
    int n, m, i, j;
    n = strlen(s);
    m = strlen(w);
    for (i = 0; i + m <= n; i++) {
        for (j = 0; j < m; j++)
            if (tolower(s[i + j]) != tolower(w[j]))
                break;
        if (j == m)
            return 1;
    }
    return 0;
}

/* is name one of the words of the header's names */
int named(char *list, char *name)
{
    char w[40];
    int i, n;
    i = 0;
    for (;;) {
        while (list[i] == ' ')
            i++;
        if (!list[i])
            return 0;
        n = 0;
        while (list[i] && list[i] != ' ' && n < 39)
            w[n++] = list[i++];
        w[n] = 0;
        if (has(w, name) && strlen(w) == strlen(name))
            return 1;
    }
}

/* the summary line: after the NAME heading, blanks and the name dropped */
void listentry(void)
{
    char *p;
    char out[200];
    p = summary;
    while (*p == ' ')
        p++;
    sprintf(out, "%-10s", names);
    if (strlen(names) > 10)
        sprintf(out, "%s ", names);
    strcat(out, *p ? strchr(p, '-') ? strchr(p, '-') + 2 : p : "");
    outline(out);
}

int main(int argc, char **argv)
{
    FILE *f;
    char path[24];
    int i, nn, list, key, inentry, show, hit, afterhdr, eof, status;
    int matched[20];
    char *want[20];
    char *a;
    nn = 0;
    list = 0;
    key = 0;
    for (i = 1; i < argc; i++) {
        a = argv[i];
        if (strcmp(a, "-l") == 0)
            list = 1;
        else if (strcmp(a, "-k") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "use: MAN [-k WORD] [-l] [NAME ...]\n");
                return 1;
            }
            key = 1;
            strncpy(word, argv[++i], 59);
        } else if (a[0] == '-' && a[1] != 0) {
            fprintf(stderr, "man: invalid option -- '%s'\nuse: MAN [-k WORD] [-l] [NAME ...]\n", a + 1);
            return 1;
        } else if (nn < 20)
            want[nn++] = a;
    }
    if (nn == 0 && !key)
        list = 1;
    if (SYSCOM->crtinfo.height > 0)
        rows = SYSCOM->crtinfo.height;
    tty = (stdout->flags & __F_CON) != 0;
    room = rows - 1;
    sprintf(path, "#%d:MAN.TEXT", SYSCOM->segtable[1].codeunit);
    f = fopen(path, "r");
    if (!f) {
        fprintf(stderr, "man: %s: cannot open it (MAN.TEXT is next to MAN.CODE)\n", path);
        return 1;
    }
    inentry = 0;
    show = 0;
    hit = 0;
    afterhdr = 0;
    names[0] = 0;
    summary[0] = 0;
    for (i = 0; i < 20; i++)
        matched[i] = 0;
    for (;;) {
        eof = !fgets(line, 298, f);
        if (!eof) {
            a = line + strlen(line);
            while (a > line && (a[-1] == '\n' || a[-1] == ' '))
                *--a = 0;
        }
        if (eof || (line[0] == '%' && line[1] == '%')) {    /* the end of an entry */
            if (inentry && (list || (key && hit)))
                listentry();
            if (eof)
                break;
            inentry = 1;
            afterhdr = 0;
            summary[0] = 0;
            hit = 0;
            show = 0;
            a = line + 2;
            while (*a == ' ')
                a++;
            strncpy(names, a, 199);
            if (!list && !key)
                for (i = 0; i < nn; i++)
                    if (named(names, want[i])) {
                        show = 1;
                        matched[i] = 1;
                    }
            if (show)
                outline("");
            continue;
        }
        if (!inentry)
            continue;
        if (afterhdr == 1 && line[0]) {
            strncpy(summary, line, 119);
            afterhdr = 2;
        }
        if (strcmp(line, "NAME") == 0)
            afterhdr = 1;
        if (key && has(line, word))
            hit = 1;
        if (show)
            outline(line);
    }
    fclose(f);
    status = 0;
    if (!list && !key)
        for (i = 0; i < nn; i++)
            if (!matched[i]) {
                fprintf(stderr, "man: no manual entry for %s (MAN alone lists them)\n", want[i]);
                status = 1;
            }
    return status;
}
