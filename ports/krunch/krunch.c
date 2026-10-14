/* krunch.c -- KRUNCH [-y] [VOL: | #n]: move the files of a disk down so that
   the unused blocks between them become one area at the end (the Filer's
   K(runch).

     -y   no question: krunch at once (without it KRUNCH says what it will
          do and asks, one key: Y goes on)

   The disk is the prefix volume, or VOL: (a volume's name), #n (unit n) or
   * (the system volume).  Each file that has unused blocks before it is
   copied down, as many blocks at a time as memory allows, and the
   directory is written again after each one, so a disk stopped halfway
   (an I/O error) has every file whole, only some of them moved.

   Moving a code file that is running is safe: the operating system finds
   the segments of the OS (SYSTEM.PASCAL), of KRUNCH and of the shell that
   started it by their block numbers (SYSCOM's segment table, pexec's
   return), and KRUNCH changes those to the new places.  It will not krunch
   a disk on which one of its own files is open (KRUNCH >X.TEXT on that
   disk).

   The status is 0 when the disk was krunched (or had nothing to move), 1
   when the answer was not Y, 2 when the disk is not on line or a file of
   KRUNCH is open on it, 3 on a disk error (the files moved so far stay
   where they went, the rest where they were). */
#pragma segment MAIN	/* calls within the program: 2-byte CGPs, no NOP after them */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <conio.h>
#include <psys.h>

int dir[1024];

int same(char *a, char *b)
{
    while (*a && toupper(*a) == toupper(*b)) {
        a++;
        b++;
    }
    return *a == 0 && *b == 0;
}

/* unit u's directory (blocks 2..5) into dir: 0 if no disk is there */
int readdir(int u)
{
    unsigned char *d;
    d = (unsigned char *)dir;
    __cspv(5, u, dir, 0, 2048, 2, 0);   /* UNITREAD */
    if (__cspi(34) != 0)
        return 0;
    return d[6] >= 1 && d[6] <= 7 && dir[8] >= 0 && dir[8] <= 77;
}

/* the directory back on unit u; the OS's copy of a directory (GDIRP) is
   then forgotten, so that it reads this one again */
int writedir(int u)
{
    unsigned char *g;
    __cspv(6, u, dir, 0, 2048, 2, 0);   /* UNITWRITE */
    g = (unsigned char *)SYSCOM->gdirp;
    if (g)
        g[6] = 0;                       /* its volume name: no volume */
    return __cspi(34) == 0;
}

/* a directory's volume name, or entry i's file name */
void dirname(int i, char *s)
{
    unsigned char *e;
    int n;
    e = (unsigned char *)dir + 26 * i + 6;
    for (n = 0; n < e[0] && n < 15; n++)
        s[n] = e[1 + n];
    s[n] = 0;
}

/* the unit of a volume: "#n", "n", "*" (the system volume) or a name
   (":" after it or not); 0 when no disk on line has it */
int unitof(char *vol)
{
    char name[20];
    char got[16];
    int n, k, u;
    strncpy(name, vol, 19);
    name[19] = 0;
    n = strlen(name);
    if (n > 0 && name[n - 1] == ':')
        name[--n] = 0;
    if (n == 0)
        return 0;
    if (strcmp(name, "*") == 0)
        return SYSCOM->sysunit;
    if (name[0] == '#' || isdigit(name[0])) {
        u = atoi(name[0] == '#' ? name + 1 : name);
        return u > 0 && u <= 14 && readdir(u) ? u : 0;
    }
    for (k = 0; k < 8; k++)
        if (readdir(units[k])) {
            dirname(0, got);
            if (same(got, name))
                return units[k];
        }
    return 0;
}

/* the prefix volume's name (the OS's DKVID) */
void prefix(char *s)
{
    unsigned char *v;
    int n;
    v = (unsigned char *)__osvaraddr(OS_DKVID);
    for (n = 0; n < v[0] && n < 7; n++)
        s[n] = v[1 + n];
    s[n] = 0;
}

/* a file of this program open on unit u (a redirection to that disk) */
int openon(int u)
{
    int i;
    for (i = 0; i < FOPEN_MAX + 3; i++)
        if (__files[i].flags && !(__files[i].flags & __F_CON) && __files[i].fib
         && ((int *)__files[i].fib)[7] == u)     /* FUNIT */
            return 1;
    return 0;
}

/* blocks first..last-1 of unit u went down by d: the OS's segment table
   and the shell's place to come back to (pexec) follow them */
void follow(int u, int first, int last, int d)
{
    int s;
    int *e;
    for (s = 0; s < 16; s++)
        if (SYSCOM->segtable[s].codeunit == u && SYSCOM->segtable[s].diskaddr >= first
         && SYSCOM->segtable[s].diskaddr < last)
            SYSCOM->segtable[s].diskaddr -= d;
    e = SYSCOM->expansion;
    if (e[0] == PX_CHILD && e[3] == u && e[4] >= first && e[4] < last)
        e[4] -= d;
}

int main(int argc, char **argv)
{
    char vol[20];
    char name[16];
    char *buf;
    int *e;
    int u, i, k, n, nb, yes, next, from, len, moves, gain, d;
    yes = 0;
    vol[0] = 0;
    for (i = 1; i < argc; i++) {
        if (same(argv[i], "-y"))
            yes = 1;
        else if (argv[i][0] == '-') {
            fprintf(stderr, "usage: KRUNCH [-y] [VOL: | #n]\n");
            return 2;
        } else {
            strncpy(vol, argv[i], 19);
            vol[19] = 0;
        }
    }
    if (!vol[0])
        prefix(vol);
    u = unitof(vol);
    if (u == 0 || !readdir(u)) {
        fprintf(stderr, "krunch: no disk %s on line\n", vol);
        return 2;
    }
    dirname(0, vol);
    if (openon(u)) {
        fprintf(stderr, "krunch: a file of KRUNCH is open on %s: (its output?)\n", vol);
        return 2;
    }
    /* what moves */
    next = dir[1];                      /* the first block after the directory */
    moves = gain = 0;
    for (i = 1; i <= dir[8]; i++) {
        e = dir + 13 * i;
        if (e[0] < next || e[1] < e[0]) {
            fprintf(stderr, "krunch: %s:'s directory is not in order (entry %d)\n", vol, i);
            return 2;
        }
        if (e[0] > next)
            moves++;
        next = next + e[1] - e[0];
    }
    gain = dir[7] - next;               /* the free blocks: after, one area */
    if (moves == 0) {
        printf("%s: (#%d) has nothing to move: %d blocks free, from block %d\n", vol, u, gain, next);
        return 0;
    }
    if (!yes) {
        printf("Krunch %s: (#%d)? %d file%s to move; then %d blocks free, from block %d (Y/N) ",
               vol, u, moves, moves == 1 ? "" : "s", gain, next);
        fflush(stdout);
        k = getch();
        printf("%c\n", k > ' ' ? k : ' ');
        if (toupper(k) != 'Y')
            return 1;
    }
    /* the buffer: as many blocks as memory allows */
    for (nb = 32; nb > 1; nb = nb / 2)
        if ((buf = (char *)malloc(nb * 512)) != NULL)
            break;
    if (nb == 1 && (buf = (char *)malloc(512)) == NULL) {
        fprintf(stderr, "krunch: no memory\n");
        return 3;
    }
    next = dir[1];
    for (i = 1; i <= dir[8]; i++) {
        e = dir + 13 * i;
        from = e[0];
        len = e[1] - e[0];
        if (from > next) {
            dirname(i, name);
            d = from - next;
            for (k = 0; k < len; k = k + n) {           /* down: the first blocks first */
                n = len - k < nb ? len - k : nb;
                __cspv(5, u, buf, 0, n * 512, from + k, 0);    /* UNITREAD */
                if (__cspi(34) == 0)
                    __cspv(6, u, buf, 0, n * 512, next + k, 0);    /* UNITWRITE */
                if (__cspi(34) != 0) {
                    fprintf(stderr, "krunch: disk error %d moving %s (block %d)\n", __cspi(34), name, from + k);
                    writedir(u);
                    return 3;
                }
            }
            e[0] = next;
            e[1] = next + len;
            follow(u, from, from + len, d);
            if (!writedir(u)) {
                fprintf(stderr, "krunch: disk error %d writing the directory\n", __cspi(34));
                return 3;
            }
            printf("  %-15s %5d block%s from block %5d to %5d\n", name, len, len == 1 ? " " : "s", from, next);
        }
        next = e[1];
    }
    printf("%s: (#%d) krunched: %d blocks free, from block %d\n", vol, u, dir[7] - next, next);
    return 0;
}
