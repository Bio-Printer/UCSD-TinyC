/* pexec.c -- Tiny-C library: pexec() runs another program, then this one again */
#include "libint.h"
#include <psys.h>

/* The operating system (BIGGY 1.10 and later, GETCMD) does the work when this
   program has ended: it starts the code file at SYSCOM->expansion[1],[2]
   (unit, first block), and when that one ends, the one at [3],[4] -- this
   program, from the beginning. Nothing of this program stays in memory
   while the other one runs. */
int pexec(char *name)
{
    char title[40];
    int fib[40];                        /* the OS's file information block */
    int buf[1024];
    int *e;
    int u, first, kind, old, linked, i, n, a, self;
    if (strlen(name) > 30)
        return -1;
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
    e = SYSCOM->expansion;
    e[1] = u;
    e[2] = first;
    e[3] = SYSCOM->segtable[1].codeunit;
    e[4] = self;
    e[0] = PX_RUN;
    exit(0);
    return 0;
}
