/* dirs.c -- Tiny-C library: the disk units and their directories, and
   sub-volumes (dirs.h): units[], direntname, volunit,
   prefixvol, namesame, wildmatch, dirreadat, dirwriteat, pathdir,
   __svplace, and stdio's __svopen, __svremove; in a small segment of
   their own (SVAT) dirwriteat, findname and the opens and closes by a
   place: __svopenat, __svcloseat, and stdio's __svclose.

   A directory is blocks 2..5 of a disk, int dir[1024]: entry 0 the volume,
   entries 1..dir[8] the files, 13 words each.  A sub-volume is a Data file
   whose own blocks 2..5 hold such a directory: a volume inside a file, its
   block numbers counted from the file's first block.  A path names a file
   inside sub-volumes: VOL:A/B/NAME or VOL:/A/B/NAME (A a sub-volume on VOL:,
   B one inside A); without VOL: it is on the shell's path (CD) or the
   prefix volume.  A \ is a / (VOL:\A\NAME, as Windows writes paths); .
   is where the path is, .. the sub-volume that holds it (../X.C, ..).

   The module is a code segment of its own (DIRS): it is in memory only
   while one of its functions runs, so a program that never names a path
   pays nothing for it. */
#include "libint.h"
#include <psys.h>
#include <dirs.h>

/* copies and searches by hand, not by the library's string functions:
   those are resident, so a program that uses them for nothing else (the
   Pascal compiler in C) would hold them all the time; these are in SVAT,
   in memory only while a path is used */
#pragma segment SVAT
static void svcopy(char *d, char *s, int n);    /* n bytes */
static char *svchr(char *s, int c);             /* the first c in s, NULL none */
#define ISDIGIT(c) ((c) >= '0' && (c) <= '9')

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
    for (n = 0; n < 19 && vol[n]; n++)
        name[n] = vol[n];
    name[n] = 0;
    if (n > 0 && name[n - 1] == ':')
        name[--n] = 0;
    if (n == 0)
        return 0;
    if (name[0] == '*' && n == 1)
        return SYSCOM->sysunit;
    if (name[0] == '#' || ISDIGIT(name[0])) {
        for (u = 0, k = name[0] == '#'; ISDIGIT(name[k]); k++)     /* (by hand: atoi is resident) */
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

/* . and .. in s (a path after its volume): .. takes back the name before
   it; when the last name is . or .. the path is a directory (it ends with
   a /) */
static void dots(char *s)
{
    char out[100];
    char *p;
    int n, k, dir;
    n = 0;
    dir = 0;
    for (p = s; *p; p = p + k) {
        while (*p == '/')
            p++;
        for (k = 0; p[k] && p[k] != '/'; k++)
            ;
        dir = k == 0 || (p[0] == '.' && (k == 1 || (k == 2 && p[1] == '.')));
        if (k == 2 && p[0] == '.' && p[1] == '.') {
            while (n > 0 && out[n - 1] != '/')
                n--;
            if (n > 0)
                n--;                            /* (and its /) */
        } else if (k > 0 && !(k == 1 && p[0] == '.')) {
            if (n > 0)
                out[n++] = '/';
            svcopy(out + n, p, k);
            n = n + k;
        }
    }
    if (dir && n > 0)
        out[n++] = '/';
    out[n] = 0;
    if (*s == '/')
        s++;                                    /* (VOL:/A: the / stays) */
    svcopy(s, out, n + 1);
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
    p = svchr(path, ':');
    n = 0;
    if (!p && __svpath[0] && path[0] != '/' && path[0] != '\\' && path[0] != '#') {
        n = strlen(__svpath);                   /* on the shell's path */
        svcopy(full, __svpath, n);
        if (n > 0 && full[n - 1] != '/' && full[n - 1] != ':')
            full[n++] = '/';
    }
    len = strlen(path);
    if (n + len + 1 > sizeof full)
        return 0;
    svcopy(full + n, path, len + 1);
    slashes(full);                              /* (A\B: A/B) */
    path = full;
    p = svchr(path, ':');
    dots(p ? p + 1 : path);                     /* (A/../B: B) */
    if (p) {
        n = p - path;
        if (n > 19)
            n = 19;
        svcopy(vol, path, n);
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
    while ((q = svchr(p, '/')) != NULL) {
        n = q - p;
        if (n > 15)
            return 0;
        svcopy(leaf, p, n);
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
    n = strlen(p);
    if (n > 15)
        return 0;
    svcopy(leaf, p, n + 1);
    *up = u;
    *bp = base;
    return 1;
}

/* the place's kind for a new file of its name: Text, Code, else Data */
static void svkind(struct svat *at)
{
    char *s;
    s = at->name + strlen(at->name);
    at->kind = 5;
    if (s - at->name > 5)
        at->kind = namesame(s - 5, ".TEXT") ? 3 : namesame(s - 5, ".CODE") ? 2 : 5;
}

/* the place of path's file (dirs.h); 0 if its sub-volume is not there */
int __svplace(char *path, struct svat *at)
{
    int dir[1024];
    if (!pathdir(path, dir, &at->unit, &at->base, at->name) || !at->name[0])
        return 0;
    svkind(at);
    return 1;
}

/* stdio's fopen of a path: its place (pathdir), then opened by it */
int __svopen(FILE *f, char *name, char *mode)
{
    int dir[1024];
    struct svat at;
    if (!pathdir(name, dir, &at.unit, &at.base, at.name) || !at.name[0])
        return 0;
    svkind(&at);
    return __svopenat(f, mode, &at);
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

/* ---- the opens and closes by a place, and what the DIRS functions share with them (SVAT:
   small, so that a program whose memory is taken can still open and save a file in a
   sub-volume) ---- */
#pragma segment SVAT

static void svcopy(char *d, char *s, int n)
{
    while (n-- > 0)
        *d++ = *s++;
}

static char *svchr(char *s, int c)
{
    for (; *s; s++)
        if (*s == c)
            return s;
    return NULL;
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

/* is the entry e (26 bytes) called name (upper or lower case) */
static int entname(unsigned char *e, char *name)
{
    int k;
    for (k = 0; name[k] && toupper(e[7 + k]) == toupper(name[k]); k++)
        ;
    return !name[k] && e[6] == k;
}

/* the entry called name: its index, 0 if none */
int findname(int *dir, char *name)
{
    int i;
    for (i = 1; i <= dir[8]; i++)
        if (entname((unsigned char *)(dir + 13 * i), name))
            return i;
    return 0;
}

/* entry i of the directory of the sub-volume at block base on unit u into e (26 bytes), read a
   block at a time into buf (512 bytes; *blk: the directory block it holds, -1 none): an open
   needs no 2 KB of memory for the directory.  0 if it cannot be read */
static int svent(char *buf, int *blk, int u, int base, int i, int *e)
{
    int o, b, n, k;
    o = 26 * i;
    for (b = 0; o >= 512; b++)
        o = o - 512;                    /* (block b, offset o: no shifts, whose helpers are resident) */
    for (k = 0; k < 26; k = k + n) {
        if (*blk != b) {
            *blk = -1;
            __cspv(5, u, buf, 0, 512, base + 2 + b, 0);    /* UNITREAD */
            if (__cspi(34) != 0)
                return 0;
            *blk = b;
        }
        n = 512 - o;
        if (n > 26 - k)
            n = 26 - k;
        svcopy((char *)e + k, buf + o, n);
        o = 0;
        b++;
    }
    return 1;
}

/* The record of an open file on a sub-volume, in f->fib (stdio's __blockio
   reads and writes through it):
   [0] unit  [1] its first block (on the disk)  [2] the blocks it may use
   [3] the blocks it has  [4] the sub-volume's first block  [5] blocks it
   had when opened  [6] its kind  [7..14] its name.  The directory is read
   a block at a time into f's own buffer (it was found good when the
   place was).  0 if it cannot be. */
int __svopenat(FILE *f, char *mode, struct svat *at)
{
    int v[13];
    int e[13];
    int *r;
    char *buf;
    int blk, i, k, nf, a, room, last, n, found;
    r = (int *)f->fib;
    buf = (char *)f->buf;
    blk = -1;
    if (!svent(buf, &blk, at->unit, at->base, 0, v))
        return 0;
    nf = v[8];
    r[0] = at->unit;
    r[4] = at->base;
    svcopy((char *)(r + 7), at->name, strlen(at->name) + 1);
    found = 0;
    for (i = 1; i <= nf && !found; i++) {      /* the entry of that name */
        if (!svent(buf, &blk, at->unit, at->base, i, e))
            return 0;
        found = entname((unsigned char *)e, at->name);
    }
    if (mode[0] == 'r' || mode[0] == 'a') {     /* an old file */
        if (!found)
            return 0;
        a = e[0];
        r[1] = at->base + a;
        r[3] = r[5] = e[1] - a;
        r[6] = e[2] & 15;
        if (i <= nf && !svent(buf, &blk, at->unit, at->base, i, e))
            return 0;
        r[2] = (i <= nf ? e[0] : v[7]) - a;     /* and the room after it */
        return 1;
    }
    /* a new file: the largest free area, as the OS does */
    room = 0;
    a = 0;
    last = v[1];
    for (k = 1; k <= nf + 1; k++) {
        if (k <= nf && !svent(buf, &blk, at->unit, at->base, k, e))
            return 0;
        n = (k <= nf ? e[0] : v[7]) - last;
        if (n > room) {
            room = n;
            a = last;
        }
        last = e[1];
    }
    if (room == 0 || nf >= MAXDIR)
        return 0;
    r[1] = at->base + a;
    r[2] = room;
    r[3] = r[5] = 0;
    r[6] = at->kind;
    return 1;
}

/* the directory after f is written (when it is new or its length changed), read into
   at->dir: a new file's entry (an old one of its name goes), an old file's new length.  0 if
   it cannot be */
int __svcloseat(FILE *f, struct svat *at)
{
    int *dir;
    int *r;
    int *e;
    char *name;
    int i, k, first, n, date;
    dir = at->dir;
    r = (int *)f->fib;
    name = (char *)(r + 7);
    __cspv(5, SYSCOM->sysunit, dir, 0, 512, 2, 0);     /* today: the boot disk's date (THEDATE) */
    date = __cspi(34) == 0 ? dir[10] : 0;
    __cspv(5, r[0], dir, 0, 2048, r[4] + 2, 0);        /* UNITREAD: the directory */
    if (__cspi(34) != 0)
        return 0;
    first = r[1] - r[4];
    i = findname(dir, name);
    if (f->flags & __F_NEW) {
        if (i) {                                /* the old file of that name goes */
            memmove(dir + 13 * i, dir + 13 * (i + 1), (dir[8] - i) * 26);
            dir[8]--;
        }
        for (i = 1; i <= dir[8] && dir[13 * i] < first; i++)
            ;
        if (i <= dir[8] && dir[13 * i] < first + r[3])     /* the area must still be free */
            return 0;
        if (i > 1 && dir[13 * (i - 1) + 1] > first || dir[8] >= MAXDIR)
            return 0;
        memmove(dir + 13 * (i + 1), dir + 13 * i, (dir[8] - i + 1) * 26);
        dir[8]++;
        e = dir + 13 * i;
        memset(e, 0, 26);
        e[0] = first;
        e[2] = r[6];
        n = strlen(name);
        ((unsigned char *)e)[6] = n;
        for (k = 0; k < n; k++)
            ((unsigned char *)e)[7 + k] = toupper(name[k]);
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

/* ... with a 2 KB directory of its own */
static int svclose(FILE *f)
{
    int dir[1024];
    struct svat at;
    at.dir = dir;
    return __svcloseat(f, &at);
}

/* stdio's fclose of a file in a sub-volume (no place): its directory entry when it is new or of
   a new length (not for one only read: no directory then, as a program whose memory is taken
   can close it) */
int __svclose(FILE *f)
{
    int *r;
    r = (int *)f->fib;
    return !(f->flags & __F_NEW) && r[3] == r[5] || svclose(f);
}
