/* pexec.c -- Tiny-C library: pexec() runs another program, then this one
   again; main(argc, argv) gets the command line it passed */
#include "libint.h"
#include <psys.h>
#include <dirs.h>

int main(int argc, char **argv);
void __redirect(int *argcp, char **argv);

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
    for (i = 0; i < n && line[i] != PX_PATHSEP; i++)
        ;
    if (i < n) {                        /* the shell's path after the arguments */
        line[i] = 0;
        strncpy(__svpath, line + i + 1, sizeof __svpath - 1);
        n = i;
    }
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
    __redirect(&argc, argv);            /* >NAME, <NAME */
    return main(argc, argv);
}

/* The operating system (BIGGY 1.10 and later, GETCMD) does the work when this
   program has ended: it starts the code file at SYSCOM->expansion[1],[2]
   (unit, first block), and when that one ends, the one at [3],[4] -- this
   program, from the beginning. Nothing of this program stays in memory
   while the other one runs.  The OS loads a code file's segments by their
   blocks on the disk, so one in a sub-volume runs where it is: NAME a path
   (A/PROG, #5:/A/PROG) or, after the shell's CD into a sub-volume, a name
   there. */
int pexec(char *cmd)
{
    char name[64];
    char title[40];
    char path[70];
    char leaf[16];
    unsigned char *pl;
    int fib[40];                        /* the OS's file information block */
    int buf[1024];
    int *e;
    int u, first, kind, old, linked, i, n, a, self, len, base, k, found;
    while (*cmd == ' ')
        cmd++;
    len = strlen(cmd);
    while (len > 0 && cmd[len - 1] == ' ')
        len--;
    if (len > 80)                       /* PL is a STRING[80] */
        return -4;
    for (i = 0; i < len && cmd[i] != ' ' && i < 63; i++)
        name[i] = cmd[i];
    if (i < len && cmd[i] != ' ')       /* a name longer than 63 */
        return -1;
    name[i] = 0;
    found = 0;
    if (strchr(name, '/') || strchr(name, '\\')
     || (__svpath[0] && !strchr(name, ':') && name[0] != '#' && name[0] != '*')) {
        strcpy(path, name);             /* in a sub-volume: NAME.CODE, or NAME. as it is */
        n = strlen(path);
        if (n > 0 && path[n - 1] == '.')
            path[n - 1] = 0;
        else
            strcat(path, ".CODE");
        if (pathdir(path, buf, &u, &base, leaf) && leaf[0] && (i = findname(buf, leaf)) != 0) {
            first = base + buf[13 * i];         /* its blocks on the disk */
            kind = buf[13 * i + 2] & 15;
            found = 1;
        } else if (strchr(name, '/') || strchr(name, '\\'))
            return -1;
    }
    if (!found) {
        __ptitle(name, title);          /* as X(ecute: NAME.CODE, or NAME. as it is */
        n = title[0];
        if (n && title[n] == '.')
            title[0] = n - 1;
        else {
            memcpy(title + n + 1, ".CODE", 5);
            title[0] = n + 5;
        }
        __cxp0v(3, fib, 0, -1);         /* FINIT */
        __cxp0v(5, fib, title, 1, 0);   /* FOPEN, old file */
        if (__cspi(34) != 0)
            return -1;
        u = fib[7];                     /* FUNIT */
        first = fib[16];                /* FHEADER.DFIRSTBLK */
        kind = fib[18] & 15;            /* FHEADER.DFKIND */
        __cxp0v(6, fib, 0);             /* FCLOSE */
    }
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
       segment 1 (directory: blocks 2..5, 13-word entries, DNUMFILES at 8);
       an entry that is a sub-volume holding it: the entry in there */
    a = SYSCOM->segtable[1].diskaddr;
    base = 0;
    for (k = 0; k < 8; k++) {
        __cspv(5, SYSCOM->segtable[1].codeunit, buf, 0, 2048, base + 2, 0);
        if (__cspi(34) != 0)
            return -3;
        self = -1;
        for (i = 1; i <= buf[8] && i < 78; i++)
            if (base + buf[13 * i] <= a && a < base + buf[13 * i + 1]) {
                self = base + buf[13 * i];
                kind = buf[13 * i + 2] & 15;
            }
        if (self < 0)
            return -3;
        if (kind == 2)
            break;
        base = self;
    }
    i = __svpath[0] ? strlen(__svpath) + 1 : 0;   /* and the shell's path (dirs.c) */
    if (len + i > 80)
        return -4;
    pl = (unsigned char *)__osvaraddr(PX_PL);   /* the command line, for main's argv */
    pl[0] = len + i;
    memcpy(pl + 1, cmd, len);
    if (i) {
        pl[1 + len] = PX_PATHSEP;
        memcpy(pl + 2 + len, __svpath, i - 1);
    }
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
