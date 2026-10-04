/* shell.c -- a mini-shell: runs programs with pexec() and comes back;
   NAME ARG1 ARG2 ... passes the arguments to NAME's main(argc, argv).
   A NAME without a volume (no ':', no '*') is looked for on every disk
   on line: found once it runs, found on several disks (up to 8) you
   choose with one key, no RETURN. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <conio.h>
#include <psys.h>

#define MAXFOUND 8

int units[] = { 4, 5, 9, 10, 11, 12, 13, 14 };   /* the disk units */
char found[MAXFOUND][24];               /* "#10:ARGS", as pexec takes it */
char vols[MAXFOUND][8];                 /* its volume's name */

/* look for the program NAME (NAME.CODE; NAME. exactly, as X(ecute) in the
   directory of every disk unit; returns how many code files were found */
int search(char *name)
{
    int dir[1024];                      /* directory: blocks 2..5 */
    unsigned char *d;
    unsigned char *e;
    char want[20];
    int n, k, i, len, nf, u, j;
    len = strlen(name);
    if (len == 0 || len > 15)
        return 0;
    for (i = 0; i < len; i++)
        want[i] = toupper(name[i]);
    want[len] = 0;
    if (want[len - 1] == '.')
        want[--len] = 0;
    else if (len <= 10) {
        strcpy(want + len, ".CODE");
        len = len + 5;
    } else
        return 0;
    d = (unsigned char *)dir;
    n = 0;
    for (k = 0; k < 8; k++) {
        u = units[k];
        __cspv(5, u, dir, 0, 2048, 2, 0);       /* UNITREAD: nothing there, or not a disk: skip */
        if (__cspi(34) != 0)
            continue;
        nf = dir[8];                    /* DNUMFILES */
        if (d[6] < 1 || d[6] > 7 || nf < 0 || nf > 77)
            continue;
        for (i = 1; i <= nf && n < MAXFOUND; i++) {
            e = d + 26 * i;             /* DTID: length byte, then the name */
            if (e[6] != len || (dir[13 * i + 2] & 15) != 2 || memcmp(e + 7, want, len) != 0)
                continue;
            sprintf(found[n], "#%d:%s", u, name);
            for (j = 0; j < d[6]; j++)
                vols[n][j] = d[7 + j];
            vols[n][j] = 0;
            n++;
        }
    }
    return n;
}

/* run NAME (with its arguments ARGS); returns only when it cannot */
void run(char *name, char *args)
{
    char cmd[130];
    int n, k, c;
    if (strchr(name, ':') || name[0] == '*')
        strcpy(cmd, name);              /* a volume given: just that one */
    else {
        n = search(name);
        if (n == 0) {
            printf("%s: no such program\n", name);
            return;
        }
        k = 1;
        if (n > 1) {
            printf("%s is on more than one disk:\n", name);
            for (k = 0; k < n; k++)
                printf("  %d  %s:%s  (%s)\n", k + 1, vols[k], name, found[k]);
            printf("Which one (1-%d, any other key: none)? ", n);
            c = getch();                /* one key: no RETURN needed */
            if (c < '1' || c >= '1' + n) {
                printf("\n");
                return;
            }
            printf("%c\n", c);
            k = c - '0';
        }
        strcpy(cmd, found[k - 1]);
    }
    if (strlen(cmd) + strlen(args) + 1 > 129) {
        printf("command line too long (80 characters at most)\n");
        return;
    }
    if (*args) {
        strcat(cmd, " ");
        strcat(cmd, args);
    }
    switch (pexec(cmd)) {
    case -1: printf("%s: no such program\n", name); break;
    case -2: printf("%s: not linked\n", name); break;
    case -4: printf("command line too long (80 characters at most)\n"); break;
    default: printf("%s: cannot find the shell's own code file\n", name); break;
    }
}

int main(void)
{
    char line[130];                     /* more than pexec takes (80): it says so */
    char here;
    char *heaptop;
    char *name;
    char *args;
    int n;
    if (pexec_returned())
        printf("[exit status %d]\n", pexec_status());
    for (;;) {
        printf("shell> ");
        if (!fgets(line, sizeof line, stdin))
            return 0;
        n = strlen(line);
        while (n > 0 && (line[n - 1] == '\n' || line[n - 1] == ' '))
            line[--n] = 0;
        name = line;
        while (*name == ' ')
            name++;
        if (*name == 0)
            continue;
        if (strcmp(name, "bye") == 0 || strcmp(name, "BYE") == 0)
            return 0;
        if (strcmp(name, "mem") == 0 || strcmp(name, "MEM") == 0) {
            __cspv(32, &heaptop);       /* MARK: the top of the heap */
            printf("shell: %u words free\n", (unsigned)(&here - heaptop) / 2);
            continue;
        }
        args = name;
        while (*args && *args != ' ')
            args++;
        if (*args) {
            *args++ = 0;
            while (*args == ' ')
                args++;
        }
        run(name, args);
    }
}
