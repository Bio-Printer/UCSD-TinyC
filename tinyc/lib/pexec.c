/* pexec.c -- Tiny-C library: pexec() runs another program, then this one
   again; main(argc, argv) gets the command line it passed */
#include "libint.h"
#include <psys.h>

int main(int argc, char **argv);

/* checksum of a Pascal string: tells pexec's command line from anything
   else the OS's prompt line PL may hold */
int __pxsum(unsigned char *s)
{
    unsigned sum;
    int i;
    sum = s[0];
    for (i = 1; i <= s[0]; i++)
        sum = sum * 3 + s[i];
    return (int)(sum ^ 0x5A5A);
}

/* The startup code calls this instead of main when main has parameters.
   The command line is valid only while this program is the one pexec
   started (SYSCOM->expansion[0] PX_CHILD) and PL still holds it. */
int __callmain(void)
{
    int *e;
    unsigned char *pl;
    char *line;
    char **argv;
    int argc;
    int n;
    int i;
    e = SYSCOM->expansion;
    pl = (unsigned char *)__osvaraddr(PX_PL);
    n = 0;
    if (e[0] == PX_CHILD && e[6] == PX_ARGS && pl[0] <= 80 && e[7] == __pxsum(pl))
        n = pl[0];
    e[6] = 0;
    line = (char *)malloc(n + 1);
    argv = (char **)malloc(((n + 1) / 2 + 2) * sizeof(char *));
    if (!line || !argv)
        exit(1);
    memcpy(line, pl + 1, n);
    line[n] = 0;
    argc = 0;
    if (n == 0)
        argv[argc++] = line;            /* not started by pexec: argv[0] is "" */
    i = 0;
    while (i < n) {
        while (i < n && line[i] == ' ')
            i++;
        if (i == n)
            break;
        argv[argc++] = line + i;
        while (i < n && line[i] != ' ')
            i++;
        if (i < n)
            line[i++] = 0;
    }
    argv[argc] = NULL;
    return main(argc, argv);
}

/* The operating system (BIGGY 1.10 and later, GETCMD) does the work when this
   program has ended: it starts the code file at SYSCOM->expansion[1],[2]
   (unit, first block), and when that one ends, the one at [3],[4] -- this
   program, from the beginning. Nothing of this program stays in memory
   while the other one runs. */
int pexec(char *cmd)
{
    char name[32];
    char title[40];
    unsigned char *pl;
    int fib[40];                        /* the OS's file information block */
    int buf[1024];
    int *e;
    int u, first, kind, old, linked, i, n, a, self, len;
    while (*cmd == ' ')
        cmd++;
    len = strlen(cmd);
    while (len > 0 && cmd[len - 1] == ' ')
        len--;
    if (len > 80)                       /* PL is a STRING[80] */
        return -4;
    for (i = 0; i < len && cmd[i] != ' ' && i < 31; i++)
        name[i] = cmd[i];
    if (i < len && cmd[i] != ' ')       /* a name longer than 31 */
        return -1;
    name[i] = 0;
    __ptitle(name, title);              /* as X(ecute: NAME.CODE, or NAME. as it is */
    n = title[0];
    if (n && title[n] == '.')
        title[0] = n - 1;
    else {
        memcpy(title + n + 1, ".CODE", 5);
        title[0] = n + 5;
    }
    __cxp0v(3, fib, 0, -1);             /* FINIT */
    __cxp0v(5, fib, title, 1, 0);       /* FOPEN, old file */
    if (__cspi(34) != 0)
        return -1;
    u = fib[7];                         /* FUNIT */
    first = fib[16];                    /* FHEADER.DFIRSTBLK */
    kind = fib[18] & 15;                /* FHEADER.DFKIND */
    __cxp0v(6, fib, 0);                 /* FCLOSE */
    if (kind != 2)                      /* not a code file */
        return -1;
    __cspv(5, u, buf, 0, 512, first, 0);    /* block 0: SEGKIND at word 96 */
    if (__cspi(34) != 0)
        return -1;
    old = 0;
    linked = 1;
    for (i = 0; i < 16; i++) {
        if (buf[96 + i] < 0 || buf[96 + i] > 4)
            old = 1;                    /* pre-I.5 code file: all linked */
        else if (buf[96 + i] != 0)
            linked = 0;
    }
    if (!linked && !old)
        return -2;
    /* this program's own code file: the directory entry that holds its
       segment 1 (directory: blocks 2..5, 13-word entries, DNUMFILES at 8) */
    a = SYSCOM->segtable[1].diskaddr;
    __cspv(5, SYSCOM->segtable[1].codeunit, buf, 0, 2048, 2, 0);
    if (__cspi(34) != 0)
        return -3;
    self = -1;
    for (i = 1; i <= buf[8] && i < 78; i++)
        if (buf[13 * i] <= a && a < buf[13 * i + 1])
            self = buf[13 * i];
    if (self < 0)
        return -3;
    pl = (unsigned char *)__osvaraddr(PX_PL);   /* the command line, for main's argv */
    pl[0] = len;
    memcpy(pl + 1, cmd, len);
    e = SYSCOM->expansion;
    e[6] = PX_ARGS;
    e[7] = __pxsum(pl);
    e[1] = u;
    e[2] = first;
    e[3] = SYSCOM->segtable[1].codeunit;
    e[4] = self;
    e[0] = PX_RUN;
    exit(0);
    return 0;
}
