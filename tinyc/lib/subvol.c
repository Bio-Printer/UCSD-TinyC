/* subvol.c -- Tiny-C library: paths written out, krunching a directory, and
   a sub-volume's size (dirs.h): svcanon, dirfree, fileson, dirkrunch,
   svresize.  KRUNCH, GROW and SHRINK are made of these; the shell, GREP and
   LC name sub-volumes with svcanon.

   A code segment of its own (SUBVOL), apart from dirs.c's DIRS, which CC's
   file finder shares: in memory only while one of these runs. */
#include "libint.h"
#include <psys.h>
#include <dirs.h>
#pragma segment SUBVOL

/* path as the shell keeps it: VOL:/A/B (the volume's name, capitals, no /
   at the end; VOL: for the volume itself); .. goes up one; 0 when there is
   no such volume or sub-volume */
int svcanon(char *path, char *out)
{
    int dir[1024];
    char vol[20];
    char leaf[16];
    char buf[80];
    char *c;
    char *p;
    int u, base, n;
    if (strlen(path) >= sizeof buf)
        return 0;
    strcpy(buf, path);
    slashes(buf);                               /* (A\B: A/B) */
    path = buf;
    c = strchr(path, ':');
    if (c) {
        n = c - path < 19 ? c - path : 19;
        memcpy(vol, path, n);
        vol[n] = 0;
        p = c + 1;
        if (!(u = volunit(vol, dir)))
            return 0;
        direntname(dir, 0, out);
        strcat(out, ":");
    } else {
        p = path;
        if (__svpath[0] && *p != '/')
            strcpy(out, __svpath);
        else {
            prefixvol(out);
            strcat(out, ":");
        }
    }
    while (*p == '/')
        p++;
    while (*p) {
        for (n = 0; p[n] && p[n] != '/'; n++)
            ;
        if (n == 2 && p[0] == '.' && p[1] == '.') {
            c = strrchr(out, '/');
            if (c)
                *c = 0;                         /* (VOL:/A: VOL:) */
        } else if (n > 0) {
            if (strlen(out) + n + 2 > sizeof __svpath - 1)
                return 0;
            strcat(out, "/");
            strncat(out, p, n);
        }
        p = p + n;
        while (*p == '/')
            p++;
    }
    for (p = out; *p; p++)
        *p = toupper(*p);
    n = strlen(out);
    if (out[n - 1] == ':')
        return 1;                               /* the volume itself */
    strcat(out, "/");
    n = pathdir(out, dir, &u, &base, leaf);     /* the sub-volume there */
    out[strlen(out) - 1] = 0;
    return n;
}

/* the smallest free area of dir with room for nb blocks: its first block,
   0 if there is none */
int dirfree(int *dir, int nb)
{
    int k, last, n, at, best;
    at = 0;
    best = 0;
    last = dir[1];                      /* the directory's end */
    for (k = 1; k <= dir[8] + 1; k++) {
        n = (k <= dir[8] ? dir[13 * k] : dir[7]) - last;
        if (n >= nb && (at == 0 || n < best)) {
            best = n;
            at = last;
        }
        if (k <= dir[8])
            last = dir[13 * k + 1];
    }
    return at;
}

/* a file of this program open on unit u (a redirection to that disk) */
int fileson(int u)
{
    int i;
    for (i = 0; i < FOPEN_MAX + 3; i++)
        if (__files[i].flags && !(__files[i].flags & __F_CON) && __files[i].fib
         && ((int *)__files[i].fib)[__files[i].flags & __F_SUB ? 0 : 7] == u)  /* FUNIT (in a sub-volume: its record's unit) */
            return 1;
    return 0;
}

/* blocks first..last-1 of unit u went down by d: the OS's segment table
   and the shell's place to come back to (pexec) follow them */
static void follow(int u, int first, int last, int d)
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

/* len blocks of unit u from block from to block to (down, or to a place
   apart: the first blocks first), buf nb blocks; 0 on a disk error */
static int moveblocks(int u, int from, int to, int len, char *buf, int nb)
{
    int k, n;
    for (k = 0; k < len; k = k + n) {
        n = len - k < nb ? len - k : nb;
        __cspv(5, u, buf, 0, n * 512, from + k, 0);     /* UNITREAD */
        if (__cspi(34) == 0)
            __cspv(6, u, buf, 0, n * 512, to + k, 0);   /* UNITWRITE */
        if (__cspi(34) != 0)
            return 0;
    }
    return 1;
}

/* a buffer of as many blocks as memory allows (up to 32): *nb of them */
static char *blockbuf(int *nb)
{
    char *buf;
    for (*nb = 32; *nb >= 1; *nb = *nb / 2)
        if ((buf = (char *)malloc(*nb * 512)) != NULL)
            return buf;
    return NULL;
}

/* The files of the directory at block base of unit u (read into dir) moved
   down, one after another from the directory's end, so that the free blocks
   are one area at the end; dir is written after each file, so a disk stopped
   halfway has every file whole.  Code files that are running may move (the
   OS's segment table and pexec's return follow them).  show: a line for each
   file moved.  The number of files moved; -1 on a disk error, -2 with no
   memory, -3 when the directory is not in the order of its blocks */
int dirkrunch(int u, int base, int *dir, int show)
{
    char name[16];
    char *buf;
    int *e;
    int i, nb, next, from, len, moves;
    next = dir[1];
    for (i = 1; i <= dir[8]; i++) {
        e = dir + 13 * i;
        if (e[0] < next || e[1] < e[0])
            return -3;
        next = e[1];
    }
    if ((buf = blockbuf(&nb)) == NULL)
        return -2;
    moves = 0;
    next = dir[1];
    for (i = 1; i <= dir[8]; i++) {
        e = dir + 13 * i;
        from = e[0];
        len = e[1] - e[0];
        if (from > next) {
            direntname(dir, i, name);
            if (!moveblocks(u, base + from, base + next, len, buf, nb)) {
                fprintf(stderr, "disk error %d moving %s (block %d)\n", __cspi(34), name, from);
                free(buf);
                return -1;
            }
            e[0] = next;
            e[1] = next + len;
            follow(u, base + from, base + from + len, from - next);
            if (!dirwriteat(u, base, dir)) {
                fprintf(stderr, "disk error %d writing the directory\n", __cspi(34));
                free(buf);
                return -1;
            }
            if (show)
                printf("  %-15s %5d block%s from block %5d to %5d\n", name, len, len == 1 ? " " : "s", from, next);
            moves++;
        }
        next = e[1];
    }
    free(buf);
    return moves;
}

/* the blocks the files of dir use, the directory's own with them (where a
   krunch would leave the free area) */
static int usedblocks(int *dir)
{
    int i, n;
    n = dir[1];
    for (i = 1; i <= dir[8]; i++)
        n = n + dir[13 * i + 1] - dir[13 * i];
    return n;
}

/* the last block + 1 that dir's files reach (the directory's end if none) */
static int lastused(int *dir)
{
    return dir[8] > 0 ? dir[13 * dir[8] + 1] : dir[1];
}

/* the blocks entry i of dir may reach: up to the next file (or the end) */
static int roomof(int *dir, int i)
{
    return (i < dir[8] ? dir[13 * (i + 1)] : dir[7]) - dir[13 * i];
}

/* GROW and SHRINK (cmd: their name, for the messages): the sub-volume at
   path becomes size blocks: +N (N more), -N (N fewer) or N.  Growing takes
   the blocks after it; when those are not free it moves to a free area that
   holds it, and when there is none the volume that holds it is krunched
   first.  Shrinking krunches the sub-volume first when its files reach past
   the new end (only then).  The exit status: 0 done, 1 a bad size, 2 no such
   sub-volume (or a file of the program open on its disk), 3 no room, too
   many files for the size, or a disk error */
int svresize(char *path, char *size, char *cmd)
{
    int pdir[1024];                     /* the directory that holds it */
    int sdir[1024];                     /* its own */
    char leaf[16];
    char *p;
    char *buf;
    int *e;
    int u, pb, i, j, sb, len, cur, want, n, need, at, nb, k;
    p = size;
    if (*p == '+' || *p == '-')
        p++;
    for (n = 0; isdigit(*p) && n < 3277; p++)
        n = n * 10 + *p - '0';
    if (*p || p == size || !isdigit(p[-1])) {
        fprintf(stderr, "%s: %s: the size is +N (N blocks more), -N (N fewer) or N blocks\n", cmd, size);
        return 1;
    }
    i = 0;
    if (pathdir(path, pdir, &u, &pb, leaf) && leaf[0])
        i = findname(pdir, leaf);
    k = i ? pdir[13 * i + 2] & 15 : -1;         /* a data (or untyped) file that holds a directory */
    if ((k != 5 && k != 0) || !dirreadat(u, pb + pdir[13 * i], sdir) || sdir[7] > pdir[13 * i + 1] - pdir[13 * i]) {
        fprintf(stderr, "%s: %s is not a sub-volume\n", cmd, path);
        return 2;
    }
    if (fileson(u)) {
        fprintf(stderr, "%s: a file of %s is open on that disk (its output?)\n", cmd, cmd);
        return 2;
    }
    e = pdir + 13 * i;
    sb = pb + e[0];
    len = e[1] - e[0];
    cur = sdir[7];
    want = size[0] == '+' ? cur + n : size[0] == '-' ? cur - n : n;
    if (want < 7) {
        fprintf(stderr, "%s: %s: %d blocks: a sub-volume has 7 at least\n", cmd, path, want);
        return 3;
    }
    if (want == cur && want == len) {
        printf("%s: %d blocks already\n", path, cur);
        return 0;
    }
    /* smaller: its files must end before the new end (a krunch if they do not) */
    if (want < lastused(sdir)) {
        if (usedblocks(sdir) > want) {
            fprintf(stderr, "%s: %s: its files and directory take %d blocks: not %d\n", cmd, path, usedblocks(sdir), want);
            return 3;
        }
        printf("krunching %s first:\n", path);
        if (dirkrunch(u, sb, sdir, 1) < 0) {
            fprintf(stderr, "%s: %s could not be krunched\n", cmd, path);
            return 3;
        }
    }
    /* bigger: room after it, or a place that holds it */
    if (want > len && roomof(pdir, i) < want) {
        at = dirfree(pdir, want);
        if (at == 0) {
            if (pdir[7] - usedblocks(pdir) < want - len) {
                fprintf(stderr, "%s: %s: no room: %d blocks free where it is, %d more needed\n", cmd, path,
                        pdir[7] - usedblocks(pdir), want - len);
                return 3;
            }
            printf("krunching the volume that holds %s first:\n", path);
            if (dirkrunch(u, pb, pdir, 1) < 0) {
                fprintf(stderr, "%s: the volume that holds %s could not be krunched\n", cmd, path);
                return 3;
            }
            i = findname(pdir, leaf);
            e = pdir + 13 * i;
            sb = pb + e[0];
            at = roomof(pdir, i) >= want ? e[0] : dirfree(pdir, want);
        }
        if (at == 0) {
            fprintf(stderr, "%s: %s: no free area of %d blocks where it is\n", cmd, path, want);
            return 3;
        }
        if (at != e[0]) {                       /* it moves there, whole */
            if ((buf = blockbuf(&nb)) == NULL) {
                fprintf(stderr, "%s: no memory\n", cmd);
                return 3;
            }
            k = moveblocks(u, sb, pb + at, len, buf, nb);
            free(buf);
            if (!k) {
                fprintf(stderr, "%s: disk error %d moving %s\n", cmd, __cspi(34), path);
                return 3;
            }
            memcpy(sdir, e, 26);                /* (its entry, while the others close up) */
            memmove(e, e + 13, (pdir[8] - i) * 26);
            for (j = 1; j < pdir[8] && pdir[13 * j] < at; j++)
                ;
            memmove(pdir + 13 * (j + 1), pdir + 13 * j, (pdir[8] - 1 - j + 1) * 26);
            e = pdir + 13 * j;
            memcpy(e, sdir, 26);
            e[0] = at;
            e[1] = at + len;
            if (!dirwriteat(u, pb, pdir)) {
                fprintf(stderr, "%s: disk error %d writing the directory\n", cmd, __cspi(34));
                return 3;
            }
            printf("  %s moved to block %d\n", path, at);
            sb = pb + at;
            if (!dirreadat(u, sb, sdir)) {
                fprintf(stderr, "%s: %s: its directory is not there after the move\n", cmd, path);
                return 3;
            }
        }
    }
    /* the new size: the file's end, its volume's end (growing: the file
       first; shrinking: the volume first -- neither is ever past the other) */
    if (want > cur) {
        e[1] = e[0] + want;
        e[11] = 512;
        if (!dirwriteat(u, pb, pdir)) {
            fprintf(stderr, "%s: disk error %d writing the directory\n", cmd, __cspi(34));
            return 3;
        }
    }
    sdir[7] = want;
    if (!dirwriteat(u, sb, sdir)) {
        fprintf(stderr, "%s: disk error %d writing %s's directory\n", cmd, __cspi(34), path);
        return 3;
    }
    if (want <= cur && e[1] != e[0] + want) {
        e[1] = e[0] + want;
        e[11] = 512;
        if (!dirwriteat(u, pb, pdir)) {
            fprintf(stderr, "%s: disk error %d writing the directory\n", cmd, __cspi(34));
            return 3;
        }
    }
    printf("%s: %d blocks (it was %d)\n", path, want, cur);
    return 0;
}
