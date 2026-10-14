/* dirs.c -- Tiny-C library: the disk units and their directories, and
   sub-volumes (dirs.h): units[], dirread, dirwrite, direntname, volunit,
   prefixvol, namesame, wildmatch, dirreadat, dirwriteat, pathdir, and
   stdio's __svopen, __svclose, __svremove.

   A directory is blocks 2..5 of a disk, int dir[1024]: entry 0 the volume,
   entries 1..dir[8] the files, 13 words each.  A sub-volume is a Data file
   whose own blocks 2..5 hold such a directory: a volume inside a file, its
   block numbers counted from the file's first block.  A path names a file
   inside sub-volumes: VOL:A/B/NAME or VOL:/A/B/NAME (A a sub-volume on VOL:,
   B one inside A); without VOL: it is on the shell's path (CD) or the
   prefix volume.  A \ is a / (VOL:\A\NAME, as Windows writes paths).

   The module is a code segment of its own (DIRS): it is in memory only
   while one of its functions runs, so a program that never names a path
   pays nothing for it. */
#include "libint.h"
#include <psys.h>
#include <dirs.h>
#pragma segment DIRS

int units[8] = { 4, 5, 9, 10, 11, 12, 13, 14 };

/* the directory of the volume (base 0) or sub-volume (base: its first
   block) on unit u into dir: 0 if there is none */
int dirreadat(int u, int base, int *dir)
{
    unsigned char *d;
    d = (unsigned char *)dir;
    __cspv(5, u, dir, 0, 2048, base + 2, 0);    /* UNITREAD */
    if (__cspi(34) != 0)
        return 0;
    return d[6] >= 1 && d[6] <= 7 && dir[8] >= 0 && dir[8] <= MAXDIR
        && dir[0] == 0 && dir[1] == 6 && dir[7] >= 6;
}

/* dir back; the OS's copy of a directory (GDIRP) is then forgotten, so
   that it reads this one again: 0 on a disk error */
int dirwriteat(int u, int base, int *dir)
{
    unsigned char *g;
    __cspv(6, u, dir, 0, 2048, base + 2, 0);    /* UNITWRITE */
    g = (unsigned char *)SYSCOM->gdirp;
    if (g)
        g[6] = 0;                       /* its volume name: no volume */
    return __cspi(34) == 0;
}

/* unit u's directory into dir: 0 if no disk is there */
int dirread(int u, int *dir)
{
    return dirreadat(u, 0, dir);
}

/* dir back on unit u: 0 on a disk error */
int dirwrite(int u, int *dir)
{
    return dirwriteat(u, 0, dir);
}

/* entry i's name (i = 0: the volume's) into s (16 bytes) */
void direntname(int *dir, int i, char *s)
{
    unsigned char *e;
    int n;
    e = (unsigned char *)dir + 26 * i + 6;
    for (n = 0; n < e[0] && n < 15; n++)
        s[n] = e[1 + n];
    s[n] = 0;
}

/* the same names, upper or lower case */
int namesame(char *a, char *b)
{
    while (*a && toupper(*a) == toupper(*b)) {
        a++;
        b++;
    }
    return *a == 0 && *b == 0;
}

/* the unit of a volume: "#n", "n", "*" (the system volume) or a name (":"
   after it or not); 0 when no disk on line has it.  dir: its directory */
int volunit(char *vol, int *dir)
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
    if (name[0] == '*' && n == 1)
        return SYSCOM->sysunit;
    if (name[0] == '#' || isdigit(name[0])) {
        for (u = 0, k = name[0] == '#'; isdigit(name[k]); k++)     /* (by hand: atoi is resident) */
            u = u * 10 + name[k] - '0';
        return u > 0 && u <= 14 && dirread(u, dir) ? u : 0;
    }
    for (k = 0; k < 8; k++)
        if (dirread(units[k], dir)) {
            direntname(dir, 0, got);
            if (namesame(got, name))
                return units[k];
        }
    return 0;
}

/* the prefix volume's name (the OS's DKVID) into s (8 bytes) */
void prefixvol(char *s)
{
    unsigned char *v;
    int n;
    v = (unsigned char *)__osvaraddr(OS_DKVID);
    for (n = 0; n < v[0] && n < 7; n++)
        s[n] = v[1 + n];
    s[n] = 0;
}

/* does name match the pattern: * or = any characters, ? any one (upper or
   lower case) */
int wildmatch(char *pat, char *name)
{
    if (*pat == 0)
        return *name == 0;
    if (*pat == '*' || *pat == '=') {
        for (;;) {
            if (wildmatch(pat + 1, name))
                return 1;
            if (*name == 0)
                return 0;
            name++;
        }
    }
    if (*name == 0)
        return 0;
    if (*pat != '?' && toupper(*pat) != toupper(*name))
        return 0;
    return wildmatch(pat + 1, name + 1);
}

/* every \ in s a / (a path as Windows writes it) */
void slashes(char *s)
{
    for (; *s; s++)
        if (*s == '\\')
            *s = '/';
}

/* the entry called name: its index, 0 if none */
int findname(int *dir, char *name)
{
    char got[16];
    int i;
    for (i = 1; i <= dir[8]; i++) {
        direntname(dir, i, got);
        if (namesame(got, name))
            return i;
    }
    return 0;
}

/* Where path's file is: its directory in dir (unit *up, the sub-volume's
   first block *bp, 0 for a volume's own) and its name in leaf (16 bytes;
   "" when path ends with /).  0 when a volume or a sub-volume on the way
   is not there. */
int pathdir(char *path, int *dir, int *up, int *bp, char *leaf)
{
    char vol[20];
    char full[100];
    char *p;
    char *q;
    int u, base, i, n, len;
    p = strchr(path, ':');
    full[0] = 0;
    if (!p && __svpath[0] && path[0] != '/' && path[0] != '\\' && path[0] != '#') {
        n = strlen(__svpath);                   /* on the shell's path */
        strcpy(full, __svpath);
        if (n > 0 && full[n - 1] != '/' && full[n - 1] != ':')
            strcat(full, "/");
    }
    if (strlen(full) + strlen(path) + 1 > sizeof full)
        return 0;
    strcat(full, path);
    slashes(full);                              /* (A\B: A/B) */
    path = full;
    p = strchr(path, ':');
    if (p) {
        n = p - path;
        if (n > 19)
            n = 19;
        memcpy(vol, path, n);
        vol[n] = 0;
        p++;
    } else {
        prefixvol(vol);
        p = path;
    }
    u = volunit(vol, dir);
    if (!u || !dirreadat(u, 0, dir))
        return 0;
    base = 0;
    if (*p == '/')
        p++;
    while ((q = strchr(p, '/')) != NULL) {
        n = q - p;
        if (n > 15)
            return 0;
        memcpy(leaf, p, n);
        leaf[n] = 0;
        if (n > 0) {                            /* (A//B: the same as A/B) */
            if (!(i = findname(dir, leaf)))
                return 0;
            n = dir[13 * i + 2] & 15;           /* a Data (or untyped) file */
            len = dir[13 * i + 1] - dir[13 * i];
            if (n != 5 && n != 0)
                return 0;
            base = base + dir[13 * i];
            if (!dirreadat(u, base, dir) || dir[7] > len)
                return 0;
        }
        p = q + 1;
    }
    if (strlen(p) > 15)
        return 0;
    strcpy(leaf, p);
    *up = u;
    *bp = base;
    return 1;
}

/* today's date (the boot disk's: the OS's THEDATE) */
static int today(void)
{
    int blk[256];
    __cspv(5, SYSCOM->sysunit, blk, 0, 512, 2, 0);
    return __cspi(34) == 0 ? blk[10] : 0;
}

/* The record of an open file on a sub-volume, in f->fib (stdio's __blockio
   reads and writes through it):
   [0] unit  [1] its first block (on the disk)  [2] the blocks it may use
   [3] the blocks it has  [4] the sub-volume's first block  [5] blocks it
   had when opened  [6] its kind  [7..14] its name */
int __svopen(FILE *f, char *name, char *mode)
{
    int dir[1024];
    char leaf[16];
    int *r;
    int *e;
    int u, base, i, k, at, room, last, n;
    char *s;
    r = (int *)f->fib;
    if (!pathdir(name, dir, &u, &base, leaf) || !leaf[0])
        return 0;
    i = findname(dir, leaf);
    r[0] = u;
    r[4] = base;
    strcpy((char *)(r + 7), leaf);
    if (mode[0] == 'r' || mode[0] == 'a') {     /* an old file */
        if (!i)
            return 0;
        e = dir + 13 * i;
        r[1] = base + e[0];
        r[3] = r[5] = e[1] - e[0];
        r[2] = (i < dir[8] ? dir[13 * (i + 1)] : dir[7]) - e[0];    /* and the room after it */
        r[6] = e[2] & 15;
        return 1;
    }
    /* a new file: the largest free area, as the OS does */
    room = 0;
    at = 0;
    last = dir[1];
    for (k = 1; k <= dir[8] + 1; k++) {
        n = (k <= dir[8] ? dir[13 * k] : dir[7]) - last;
        if (n > room) {
            room = n;
            at = last;
        }
        if (k <= dir[8])
            last = dir[13 * k + 1];
    }
    if (room == 0 || dir[8] >= MAXDIR)
        return 0;
    r[1] = base + at;
    r[2] = room;
    r[3] = r[5] = 0;
    n = strlen(leaf);
    s = leaf + n;
    r[6] = n > 5 && namesame(s - 5, ".TEXT") ? 3 : n > 5 && namesame(s - 5, ".CODE") ? 2 : 5;
    return 1;
}

/* the directory after f is written: a new file's entry (an old one of its
   name goes), an old file's new length; 0 if it cannot be */
int __svclose(FILE *f)
{
    int dir[1024];
    int *r;
    int *e;
    int i, k, first, n, date;
    r = (int *)f->fib;
    if (!(f->flags & __F_NEW) && r[3] == r[5])
        return 1;                               /* (only read, or the same length) */
    if (!dirreadat(r[0], r[4], dir))
        return 0;
    first = r[1] - r[4];
    date = today();
    i = findname(dir, (char *)(r + 7));
    if (f->flags & __F_NEW) {
        if (i) {                                /* the old file of that name goes */
            memmove(dir + 13 * i, dir + 13 * (i + 1), (dir[8] - i) * 26);
            dir[8]--;
        }
        for (i = 1; i <= dir[8] && dir[13 * i] < first; i++)
            ;
        for (k = i; k <= dir[8]; k++)           /* the area must still be free */
            if (dir[13 * k] < first + r[3])
                return 0;
        if (i > 1 && dir[13 * (i - 1) + 1] > first || dir[8] >= MAXDIR)
            return 0;
        memmove(dir + 13 * (i + 1), dir + 13 * i, (dir[8] - i + 1) * 26);
        dir[8]++;
        e = dir + 13 * i;
        memset(e, 0, 26);
        e[0] = first;
        e[2] = r[6];
        n = strlen((char *)(r + 7));
        ((unsigned char *)e)[6] = n;
        for (k = 0; k < n; k++)
            ((unsigned char *)e)[7 + k] = toupper(((char *)(r + 7))[k]);
    } else {
        if (!i)
            return 0;
        e = dir + 13 * i;
    }
    e[1] = first + r[3];
    e[11] = 512;
    e[12] = date;
    return dirwriteat(r[0], r[4], dir);
}

/* the file at path goes: 0 if done */
int __svremove(char *path)
{
    int dir[1024];
    char leaf[16];
    int u, base, i;
    if (!pathdir(path, dir, &u, &base, leaf) || !(i = findname(dir, leaf)))
        return -1;
    memmove(dir + 13 * i, dir + 13 * (i + 1), (dir[8] - i) * 26);
    dir[8]--;
    return dirwriteat(u, base, dir) ? 0 : -1;
}
