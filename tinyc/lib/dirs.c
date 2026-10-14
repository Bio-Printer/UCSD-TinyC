/* dirs.c -- Tiny-C library: the disk units and their directories (psys.h):
   units[], dirread, dirwrite, direntname, volunit, prefixvol, namesame,
   wildmatch.  A directory is blocks 2..5 of a disk, int dir[1024]: entry 0
   the volume, entries 1..dir[8] the files, 13 words each. */
#include "libint.h"
#include <psys.h>
#include <dirs.h>

int units[8] = { 4, 5, 9, 10, 11, 12, 13, 14 };

/* unit u's directory into dir: 0 if no disk is there */
int dirread(int u, int *dir)
{
    unsigned char *d;
    d = (unsigned char *)dir;
    __cspv(5, u, dir, 0, 2048, 2, 0);   /* UNITREAD */
    if (__cspi(34) != 0)
        return 0;
    return d[6] >= 1 && d[6] <= 7 && dir[8] >= 0 && dir[8] <= MAXDIR;
}

/* dir back on unit u; the OS's copy of a directory (GDIRP) is then
   forgotten, so that it reads this one again: 0 on a disk error */
int dirwrite(int u, int *dir)
{
    unsigned char *g;
    __cspv(6, u, dir, 0, 2048, 2, 0);   /* UNITWRITE */
    g = (unsigned char *)SYSCOM->gdirp;
    if (g)
        g[6] = 0;                       /* its volume name: no volume */
    return __cspi(34) == 0;
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
    if (strcmp(name, "*") == 0)
        return SYSCOM->sysunit;
    if (name[0] == '#' || isdigit(name[0])) {
        u = atoi(name[0] == '#' ? name + 1 : name);
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
