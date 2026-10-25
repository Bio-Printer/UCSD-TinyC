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
        } else if (n > 0 && !(n == 1 && p[0] == '.')) {     /* (. is here) */
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

/* len blocks of unit u from block from to block to (down, or to a place
   apart: the first blocks first), through a buffer of as many blocks as
   memory allows (32 at most): 1, 0 on a disk error, -1 with no memory */
static int moveblocks(int u, int from, int to, int len)
{
    char *buf;
    int nb, k, n;
    for (nb = 32; (buf = (char *)malloc(nb * 512)) == NULL; nb = nb / 2)
        if (nb == 1)
            return -1;
    for (k = 0; k < len; k = k + n) {
        n = len - k < nb ? len - k : nb;
        __cspv(5, u, buf, 0, n * 512, from + k, 0);     /* UNITREAD */
        if (__cspi(34) == 0)
            __cspv(6, u, buf, 0, n * 512, to + k, 0);   /* UNITWRITE */
        if (__cspi(34) != 0) {
            free(buf);
            return 0;
        }
    }
    free(buf);
    return 1;
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
    int *e;
    int i, k, next, from, len, moves;
    next = dir[1];
    for (i = 1; i <= dir[8]; i++) {
        e = dir + 13 * i;
        if (e[0] < next || e[1] < e[0])
            return -3;
        next = e[1];
    }
    moves = 0;
    next = dir[1];
    for (i = 1; i <= dir[8]; i++) {
        e = dir + 13 * i;
        from = e[0];
        len = e[1] - e[0];
        if (from > next) {
            direntname(dir, i, name);
            k = moveblocks(u, base + from, base + next, len);
            if (k < 0)
                return -2;
            if (k == 0) {
                fprintf(stderr, "disk error %d moving %s (block %d)\n", __cspi(34), name, from);
                return -1;
            }
            e[0] = next;
            e[1] = next + len;
            /* the OS's segment table and the shell's place to come back to
               (pexec) follow a code file that runs */
            for (k = 0; k < 16; k++)
                if (SYSCOM->segtable[k].codeunit == u && SYSCOM->segtable[k].diskaddr >= base + from
                 && SYSCOM->segtable[k].diskaddr < base + from + len)
                    SYSCOM->segtable[k].diskaddr -= from - next;
            k = SYSCOM->expansion[0] == PX_CHILD && SYSCOM->expansion[3] == u;
            if (k && SYSCOM->expansion[4] >= base + from && SYSCOM->expansion[4] < base + from + len)
                SYSCOM->expansion[4] -= from - next;
            if (!dirwriteat(u, base, dir)) {
                fprintf(stderr, "disk error %d writing the directory\n", __cspi(34));
                return -1;
            }
            if (show)
                printf("  %-15s %5d block%s from block %5d to %5d\n", name, len, len == 1 ? " " : "s", from, next);
            moves++;
        }
        next = e[1];
    }
    return moves;
}

/* the blocks the files of dir use, the directory's own with them (where a
   krunch would leave the free area); *last: the block after the last file
   (the directory's end if none) */
static int used(int *dir, int *last)
{
    int i, n;
    n = dir[1];
    for (i = 1; i <= dir[8]; i++)
        n = n + dir[13 * i + 1] - dir[13 * i];
    *last = dir[8] > 0 ? dir[13 * dir[8] + 1] : dir[1];
    return n;
}

static int *rpdir;                      /* resize's directories, one pair for every level */
static int *rsdir;

/* a directory written back, or the message: 1, 0 on a disk error */
static int putdir(int u, int base, int *dir, char *cmd, char *whose)
{
    if (dirwriteat(u, base, dir))
        return 1;
    fprintf(stderr, "%s: disk error %d writing %s directory\n", cmd, __cspi(34), whose);
    return 0;
}

/* The volume or sub-volume cp (svcanon's: VOL: or VOL:/A/B) becomes want
   blocks; want 0: as small as its files let it be.  A volume's disk image
   follows its size (the emulator's CSP 139, UNITSIZE: version 2.09).  A
   sub-volume that finds no room where it is grows the one that holds it
   first, by what it lacks -- up to the volume, whose image grows -- and then
   starts again (again: once only). */
static int resize(char *cp, int want, char *cmd, int again)
{
    char leaf[16];
    char up[80];
    int *pdir;
    int *sdir;
    int *e;
    char *c;
    int u, pb, i, j, sb, len, cur, n, at, k, last, need;
    pdir = rpdir;
    sdir = rsdir;
    if (!pathdir(cp, pdir, &u, &pb, leaf))
        return 2;
    if (!leaf[0]) {                             /* the volume itself */
        memcpy(sdir, pdir, 2048);
        cur = sdir[7];
        len = __cspi(139, u, 0);                /* its disk image's blocks */
        n = used(sdir, &last);
        if (!want)
            want = n;
        if (want < n) {
            fprintf(stderr, "%s: %s: its files and directory take %d blocks: not %d\n", cmd, cp, n, want);
            return 3;
        }
        if (want == cur && want == len) {
            printf("%s %d blocks already\n", cp, cur);
            return 0;
        }
        if (want < last) {
            printf("krunching %s first:\n", cp);
            if (dirkrunch(u, 0, sdir, 1) < 0) {
                fprintf(stderr, "%s: %s could not be krunched\n", cmd, cp);
                return 3;
            }
        }
        if (want > len && __cspi(139, u, want) != want) {
            fprintf(stderr, "%s: %s: its disk image cannot be %d blocks (no room on the PC's disk?)\n", cmd, cp, want);
            return 3;
        }
        sdir[7] = want;
        if (!putdir(u, 0, sdir, cmd, "the volume's"))
            return 3;
        if (want < len)
            __cspi(139, u, want);               /* (the image after the directory: never shorter) */
        printf("%s %d blocks (it was %d)\n", cp, want, cur);
        return 0;
    }
    c = strrchr(cp, '/');                       /* what holds it (VOL:/A/B: VOL:/A; VOL:/A: VOL:) */
    n = c - cp;
    memcpy(up, cp, n);
    up[n] = 0;
    i = findname(pdir, leaf);
    k = i ? pdir[13 * i + 2] & 15 : -1;         /* a data (or untyped) file that holds a directory */
    if ((k != 5 && k != 0) || !dirreadat(u, pb + pdir[13 * i], sdir) || sdir[7] > pdir[13 * i + 1] - pdir[13 * i])
        return 2;
    e = pdir + 13 * i;
    sb = pb + e[0];
    len = e[1] - e[0];
    cur = sdir[7];
    n = used(sdir, &last);
    if (!want)                                  /* (no size: as small as its files let it be) */
        want = n < 7 ? 7 : n;
    if (want < 7) {
        fprintf(stderr, "%s: %s: %d blocks: a sub-volume has 7 at least\n", cmd, cp, want);
        return 3;
    }
    if (want == cur && want == len) {
        printf("%s: %d blocks already\n", cp, cur);
        return 0;
    }
    /* smaller: its files must end before the new end (a krunch if they do not) */
    if (want < last) {
        if (n > want) {
            fprintf(stderr, "%s: %s: its files and directory take %d blocks: not %d\n", cmd, cp, n, want);
            return 3;
        }
        printf("krunching %s first:\n", cp);
        if (dirkrunch(u, sb, sdir, 1) < 0) {
            fprintf(stderr, "%s: %s could not be krunched\n", cmd, cp);
            return 3;
        }
    }
    /* bigger: room after it, or a place that holds it */
    k = i < pdir[8] ? pdir[13 * (i + 1)] : pdir[7];     /* how far it may reach: the next file, the end */
    if (want > len && k - e[0] < want) {
        at = dirfree(pdir, want);
        if (at == 0) {
            /* krunched, the one that holds it has its free blocks at its end: after it
               if it is the last there (it grows in place), else it moves to them */
            need = (i == pdir[8] ? want - len : want) - (pdir[7] - used(pdir, &last));
            if (need > 0) {
                if (again) {
                    fprintf(stderr, "%s: %s: no room after growing what holds it\n", cmd, cp);
                    return 3;
                }
                printf("no room for %s: %s grows first, by %d blocks\n", cp, up, need);
                k = resize(up, pdir[7] + need, cmd, 0);
                return k ? k : resize(cp, want, cmd, 1);    /* (both directories again) */
            }
            printf("krunching %s first (it holds %s):\n", up, cp);
            if (dirkrunch(u, pb, pdir, 1) < 0) {
                fprintf(stderr, "%s: the directory that holds %s could not be krunched\n", cmd, cp);
                return 3;
            }
            i = findname(pdir, leaf);
            e = pdir + 13 * i;
            sb = pb + e[0];
            k = i < pdir[8] ? pdir[13 * (i + 1)] : pdir[7];
            at = k - e[0] >= want ? e[0] : dirfree(pdir, want);
        }
        if (at == 0) {
            fprintf(stderr, "%s: %s: no free area of %d blocks where it is\n", cmd, cp, want);
            return 3;
        }
        if (at != e[0]) {                       /* it moves there, whole */
            k = moveblocks(u, sb, pb + at, len);
            if (k <= 0) {
                fprintf(stderr, k < 0 ? "%s: no memory\n" : "%s: disk error moving %s\n", cmd, cp);
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
            if (!putdir(u, pb, pdir, cmd, "the"))
                return 3;
            printf("  %s moved to block %d\n", cp, at);
            sb = pb + at;
            if (!dirreadat(u, sb, sdir)) {
                fprintf(stderr, "%s: %s: its directory is not there after the move\n", cmd, cp);
                return 3;
            }
        }
    }
    /* the new size: the file's end, its volume's end (growing: the file
       first; shrinking: the volume first -- neither is ever past the other) */
    if (want > cur) {
        e[1] = e[0] + want;
        e[11] = 512;
        if (!putdir(u, pb, pdir, cmd, "the"))
            return 3;
    }
    sdir[7] = want;
    if (!putdir(u, sb, sdir, cmd, "its own"))
        return 3;
    if (want <= cur && e[1] != e[0] + want) {
        e[1] = e[0] + want;
        e[11] = 512;
        if (!putdir(u, pb, pdir, cmd, "the"))
            return 3;
    }
    printf("%s: %d blocks (it was %d)\n", cp, want, cur);
    return 0;
}
/* GROW and SHRINK (cmd: their name, for the messages): the sub-volume or volume
   at path becomes size blocks: +N (N more), -N (N fewer) or N; "" (SHRINK NAME):
   as small as its directory and files let it be, no free blocks left.  Growing
   takes the blocks after it; when those are not free it moves to a free area that
   holds it, and when there is none the directory that holds it is krunched first,
   and if that leaves too few free blocks that one grows first, and so on up to
   the volume.  A volume's disk image grows and shrinks with it (the emulator's
   CSP 139: version 2.09).  Shrinking krunches first when the files reach past the
   new end (only then).  The exit status: 0 done, 1 a bad size, 2 no such
   sub-volume or volume (or a file of the program open on its disk), 3 no room,
   too many files for the size, or a disk error */
int svresize(char *path, char *size, char *cmd)
{
    int dirs[2048];                     /* resize's directories: the one that holds it, its own */
    char cp[80];
    char leaf[16];
    char *p;
    int u, pb, n, cur, r;
    p = size;
    if (*p == '+' || *p == '-')
        p++;
    for (n = 0; isdigit(*p) && n < 3277; p++)
        n = n * 10 + *p - '0';
    if (*size && (*p || p == size || !isdigit(p[-1]))) {
        fprintf(stderr, "%s: %s: the size is +N (N blocks more), -N (N fewer) or N blocks\n", cmd, size);
        return 1;
    }
    rpdir = dirs;
    rsdir = dirs + 1024;
    if (!svcanon(path, cp) || !pathdir(cp, dirs, &u, &pb, leaf)) {
        fprintf(stderr, "%s: %s is not a sub-volume or a volume\n", cmd, path);
        return 2;
    }
    if (fileson(u)) {
        fprintf(stderr, "%s: a file of %s is open on that disk (its output?)\n", cmd, cmd);
        return 2;
    }
    cur = 0;
    if (!leaf[0])
        cur = dirs[7];
    else if ((r = findname(dirs, leaf)) != 0 && dirreadat(u, pb + dirs[13 * r], rsdir))
        cur = rsdir[7];
    n = size[0] == '+' ? cur + n : size[0] == '-' ? cur - n : n;
    if (*size && n < 1) {
        fprintf(stderr, "%s: %s: %d blocks: too few\n", cmd, path, n);
        return 3;
    }
    r = resize(cp, *size ? n : 0, cmd, 0);
    if (r == 2)
        fprintf(stderr, "%s: %s is not a sub-volume or a volume\n", cmd, path);
    return r;
}
