/* shell.c -- a mini-shell: runs programs with pexec() and comes back;
   NAME ARG1 ARG2 ... passes the arguments to NAME's main(argc, argv).
   A NAME without a volume (no ':', no '*') is looked for on every disk
   on line: found once it runs, found on several disks (up to 8) you
   choose with one key, no RETURN.
   Its own commands (upper or lower case):
     CD [#n | n | VOL | *]   the prefix: the volume of unit n (or VOL, or
                             the system volume), as the Filer's Prefix: the
                             files of later commands are there when they
                             name no volume.  CD alone: what it is.
     DIR [VOL: | #n:][PAT]   the files of the prefix volume, or of VOL or
                             unit n, whose names match PAT: * or = any
                             characters, ? any one (DIR #5, DIR *.C,
                             DIR TOOLSRC:VI*.C, DIR #9:?.TEXT)
     TYPE [VOL: | #n:]NAME   a text file on the console (with wildcards:
                             each file that matches, under its name)
     DELETE [VOL: | #n:]PAT  the files that match (with wildcards it lists
                             them and asks first, one key: Y deletes)
     MEM                     the shell's free memory
     BYE                     back to the Command: prompt */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <conio.h>
#include <psys.h>

#define MAXFOUND 8

int units[] = { 4, 5, 9, 10, 11, 12, 13, 14 };   /* the disk units */

/* the operating system's prefix (DKVID, the Filer's Prefix) and system
   volume (SYVID): STRING[7] globals, OS words 59..62 and 63..66 (II.0
   lays out "SYVID,DKVID: VID" last first) */
#define OS_DKVID 59
#define OS_SYVID 63
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

/* same, ignoring case */
int same(char *a, char *b)
{
    while (*a && toupper(*a) == toupper(*b)) {
        a++;
        b++;
    }
    return *a == 0 && *b == 0;
}

/* unit u's directory (blocks 2..5) into dir: 0 if no disk is there */
int readdir(int u, int *dir)
{
    unsigned char *d;
    d = (unsigned char *)dir;
    __cspv(5, u, dir, 0, 2048, 2, 0);   /* UNITREAD */
    if (__cspi(34) != 0)
        return 0;
    return d[6] >= 1 && d[6] <= 7 && dir[8] >= 0 && dir[8] <= 77;
}

/* a directory's volume name, or entry i's file name */
void dirname(int *dir, int i, char *s)
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
int unitof(char *vol, int *dir)
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
        return u > 0 && u <= 14 && readdir(u, dir) ? u : 0;
    }
    for (k = 0; k < 8; k++)
        if (readdir(units[k], dir)) {
            dirname(dir, 0, got);
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

/* CD: the prefix becomes the volume in unit n (or VOL, or *) */
void cd(char *arg)
{
    int dir[1024];
    char vol[16];
    unsigned char *v;
    int u, n;
    if (*arg) {
        u = unitof(arg, dir);
        if (u == 0 || !readdir(u, dir)) {
            printf("cd: no disk %s on line\n", arg);
            return;
        }
        dirname(dir, 0, vol);
        v = (unsigned char *)__osvaraddr(OS_DKVID);
        n = strlen(vol);
        v[0] = n;
        memcpy(v + 1, vol, n);
    }
    prefix(vol);
    u = unitof(vol, dir);
    if (u)
        printf("prefix is %s: (#%d)\n", vol, u);
    else
        printf("prefix is %s: (not on line)\n", vol);
}

/* does name match the pattern: * or = any characters, ? any one */
int match(char *pat, char *name)
{
    if (*pat == 0)
        return *name == 0;
    if (*pat == '*' || *pat == '=') {
        for (;;) {
            if (match(pat + 1, name))
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
    return match(pat + 1, name + 1);
}

char *months[] = { "", "Jan", "Feb", "Mar", "Apr", "May", "Jun",
                   "Jul", "Aug", "Sep", "Oct", "Nov", "Dec" };
char *kinds[] = { "", "Bad", "Code", "Text", "Info", "Data", "Graf", "Foto" };

/* after a screenful, wait for a key: 0 if it was ESC (stop) */
int more(int *lines)
{
    int c;
    if (++*lines < SYSCOM->crtinfo.height - 1)
        return 1;
    *lines = 0;
    printf("-- more (any key; ESC stops) --");
    c = getch();
    printf("\r                                \r");
    return c != 27;
}

/* [VOL: | #n:][PAT]: the unit (its directory read into dir, its volume's
   name into vol) and *pat; 0 (and a message for cmd) when no disk has it */
int target(char *cmd, char *arg, int *dir, char *vol, char **pat)
{
    char *colon;
    int u, i;
    colon = strchr(arg, ':');
    if (colon) {
        i = colon - arg;
        if (i > 19)
            i = 19;
        memcpy(vol, arg, i);
        vol[i] = 0;
        *pat = colon + 1;
    } else if (arg[0] == '#') {
        strcpy(vol, arg);               /* #5: the unit, every file */
        *pat = "";
    } else {
        prefix(vol);
        *pat = arg;
    }
    u = unitof(vol, dir);
    if (u == 0 || !readdir(u, dir)) {
        printf("%s: no disk %s on line\n", cmd, *vol ? vol : ":");
        return 0;
    }
    dirname(dir, 0, vol);
    return u;
}

int wild(char *pat)
{
    return strpbrk(pat, "*=?") != NULL;
}

/* DIR [VOL: | #n:][PAT] */
void dir(char *arg)
{
    int dir[1024];
    char vol[20];
    char name[16];
    char *pat;
    int *e;
    int u, i, nf, shown, used, gap, largest, unused, last, lines, m;
    u = target("dir", arg, dir, vol, &pat);
    if (!u)
        return;
    nf = dir[8];
    printf("%s: (#%d)\n", vol, u);
    lines = 1;
    shown = used = unused = largest = 0;
    last = dir[1];                      /* the directory's end: the first free block */
    for (i = 1; i <= nf; i++) {
        e = dir + 13 * i;
        gap = e[0] - last;
        unused += gap;
        if (gap > largest)
            largest = gap;
        last = e[1];
        used += e[1] - e[0];
        dirname(dir, i, name);
        if (*pat && !match(pat, name))
            continue;
        m = e[12] & 15;
        if (m == 0)                     /* no date */
            printf("%-15s %5d             %s\n", name, e[1] - e[0], kinds[e[2] & 7]);
        else
            printf("%-15s %5d  %2d-%s-%02d  %s\n", name, e[1] - e[0], (e[12] >> 4) & 31,
                   months[m <= 12 ? m : 0], (e[12] >> 9) & 127, kinds[e[2] & 7]);
        shown++;
        if (!more(&lines))
            return;
    }
    gap = dir[7] - last;                /* DEOVBLK: to the volume's end */
    unused += gap;
    if (gap > largest)
        largest = gap;
    printf("%d of %d files, %d blocks used, %d unused, %d in the largest area\n",
           shown, nf, used, unused, largest);
}

/* TYPE [VOL: | #n:]NAME: a text file on the console */
void type(char *arg)
{
    int dir[1024];
    char vol[20];
    char name[16];
    char path[24];
    char line[256];
    char *pat;
    FILE *f;
    int u, i, nf, found;
    if (!*arg) {
        printf("type: which file?\n");
        return;
    }
    u = target("type", arg, dir, vol, &pat);
    if (!u)
        return;
    nf = dir[8];
    found = 0;
    for (i = 1; i <= nf; i++) {
        dirname(dir, i, name);
        if (!match(pat, name))
            continue;
        found++;
        if (wild(pat))
            printf("--- %s:%s\n", vol, name);
        if ((dir[13 * i + 2] & 15) != 3) {
            printf("%s: not a text file\n", name);
            continue;
        }
        sprintf(path, "#%d:%s", u, name);
        f = fopen(path, "r");
        if (!f) {
            printf("%s: cannot open it\n", name);
            continue;
        }
        while (fgets(line, sizeof line, f))
            fputs(line, stdout);
        fclose(f);
    }
    if (!found)
        printf("type: no file %s on %s:\n", pat, vol);
}

/* DELETE [VOL: | #n:]PAT: with wildcards, after a Y */
void delete(char *arg)
{
    int dir[1024];
    char vol[20];
    char names[77][16];
    char path[24];
    char *pat;
    int u, i, n, nf, c;
    if (!*arg) {
        printf("delete: which files?\n");
        return;
    }
    u = target("delete", arg, dir, vol, &pat);
    if (!u)
        return;
    if (!*pat) {
        printf("delete: which files on %s:? (* for all of them)\n", vol);
        return;
    }
    nf = dir[8];
    n = 0;
    for (i = 1; i <= nf; i++) {
        dirname(dir, i, names[n]);
        if (match(pat, names[n]))
            n++;
    }
    if (n == 0) {
        printf("delete: no file %s on %s:\n", pat, vol);
        return;
    }
    if (wild(pat)) {
        for (i = 0; i < n; i++)
            printf("  %s:%s\n", vol, names[i]);
        printf("Delete %s %d file%s (Y/N)? ", n == 1 ? "this" : "these", n, n == 1 ? "" : "s");
        c = getch();
        printf("%c\n", c >= ' ' ? c : ' ');
        if (c != 'Y' && c != 'y')
            return;
    }
    for (i = 0; i < n; i++) {
        sprintf(path, "#%d:%s", u, names[i]);
        if (remove(path) == 0)
            printf("%s:%s deleted\n", vol, names[i]);
        else
            printf("%s:%s: cannot delete it\n", vol, names[i]);
    }
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
        args = name;
        while (*args && *args != ' ')
            args++;
        if (*args) {
            *args++ = 0;
            while (*args == ' ')
                args++;
        }
        if (same(name, "bye"))
            return 0;
        if (same(name, "cd")) {
            cd(args);
            continue;
        }
        if (same(name, "dir")) {
            dir(args);
            continue;
        }
        if (same(name, "type")) {
            type(args);
            continue;
        }
        if (same(name, "delete") || same(name, "del")) {
            delete(args);
            continue;
        }
        if (same(name, "mem")) {
            __cspv(32, &heaptop);       /* MARK: the top of the heap */
            printf("shell: %u words free\n", (unsigned)(&here - heaptop) / 2);
            continue;
        }
        run(name, args);
    }
}
