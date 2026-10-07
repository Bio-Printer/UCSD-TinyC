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
     WHEREIS [VOL: | #n:]PAT the files that match on every disk on line
                             (WHEREIS STDIO.H, WHEREIS *.C), or on one
     VOLUMES                 every disk on line: unit, volume, files,
                             blocks used of its size
     COPY SRC DEST           SRC: [VOL: | #n:]PAT; DEST: a volume (VOL:,
                             #n:: the same names) or, for one file, a
                             name (on the prefix volume unless VOL:):
                             the kind, date and length as they were
     MOVE SRC DEST           the same, then SRC goes (on its own disk:
                             only its name changes)
     RENAME [VOL: | #n:]NAME NEW   one file's name, on its disk
     MEM                     the shell's free memory
     BYE                     back to the Command: prompt
   The command line: Up and Down bring back the last 10 commands (kept in
   #4:SYSTEM.CMDS, so they outlast the shell); Left, Right, Home and End
   move in the line; Insert switches between inserting and typing over;
   Delete deletes the character at the cursor, Backspace the one before
   it; ESC clears the line.  (Home, End, Insert and Delete need emulator
   2.00 or later: see psys.h PX_KEYS.) */
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

/* ---- the command line: history and editing ---- */

#define NHIST 10
#define LINEMAX 120
#define HISTFILE "#4:SYSTEM.CMDS"
char hist[NHIST][LINEMAX + 1];          /* the oldest first */
int nhist;

/* the commands kept in #4:SYSTEM.CMDS: lines, then a NUL */
void loadhist(void)
{
    FILE *f;
    int c, n;
    nhist = 0;
    f = fopen(HISTFILE, "rb");
    if (!f)
        return;
    n = 0;
    while ((c = fgetc(f)) != EOF && c != 0 && nhist < NHIST) {
        if (c == '\n') {
            hist[nhist][n] = 0;
            if (n > 0)
                nhist++;
            n = 0;
        } else if (n < LINEMAX)
            hist[nhist][n++] = c;
    }
    fclose(f);
}

/* a command entered: the newest in the history, which goes to the file */
void savehist(char *cmd)
{
    FILE *f;
    int i;
    if (!*cmd || (nhist > 0 && strcmp(hist[nhist - 1], cmd) == 0))
        return;
    if (nhist == NHIST) {
        for (i = 1; i < NHIST; i++)
            strcpy(hist[i - 1], hist[i]);
        nhist--;
    }
    strcpy(hist[nhist++], cmd);
    f = fopen(HISTFILE, "wb");
    if (!f)
        return;
    for (i = 0; i < nhist; i++) {
        fputs(hist[i], f);
        fputc('\n', f);
    }
    fputc(0, f);
    fclose(f);
}

void out(int c)
{
    fputc(c, stdout);
}

/* the cursor n places left */
void back(int n)
{
    int bs;
    bs = SYSCOM->crtctrl.backspace ? SYSCOM->crtctrl.backspace : 8;
    while (n-- > 0)
        out(bs);
}

/* the cursor is at from: buf[from..len) again, blanks over what was
   shown beyond it, then the cursor to to; returns what is shown now */
int redraw(char *buf, int len, int from, int to, int shown)
{
    int i;
    for (i = from; i < len; i++)
        out(buf[i]);
    for (; i < shown; i++)
        out(' ');
    back(i - to);
    return len;
}

/* a command line, edited: into buf (LINEMAX characters at most) */
void editline(char *buf)
{
    char draft[LINEMAX + 1];
    struct crtinforec *ci;
    int len, pos, shown, ins, h, c;
    ci = &SYSCOM->crtinfo;
    len = pos = shown = 0;
    ins = 1;
    h = nhist;
    buf[0] = 0;
    draft[0] = 0;
    SYSCOM->expansion[1] = PX_KEYS;     /* Home ... Delete: one code each */
    for (;;) {
        fflush(stdout);
        c = getch();
        if (c == '\r' || c == '\n')
            break;
        if ((c == ci->up && h > 0) || (c == ci->down && h < nhist)) {
            if (h == nhist)
                strcpy(draft, buf);     /* what was being typed */
            h = c == ci->up ? h - 1 : h + 1;
            back(pos);
            strcpy(buf, h == nhist ? draft : hist[h]);
            len = pos = strlen(buf);
            shown = redraw(buf, len, 0, len, shown);
        } else if (c == ci->left || c == KEY_HOME) {
            c = c == KEY_HOME ? pos : (pos > 0);
            back(c);
            pos = pos - c;
        } else if (c == ci->right || c == KEY_END) {
            c = c == KEY_END ? len : (pos < len ? pos + 1 : pos);
            for (; pos < c; pos++)
                out(buf[pos]);
        } else if (c == KEY_INSERT)
            ins = !ins;
        else if ((c == KEY_DELETE && pos < len)
                 || ((c == 8 || c == 127 || c == ci->chardel) && pos > 0)) {
            if (c != KEY_DELETE) {
                pos--;
                back(1);
            }
            memmove(buf + pos, buf + pos + 1, len - pos);
            len--;
            shown = redraw(buf, len, pos, pos, shown);
        } else if (c == 27 || c == ci->linedel) {
            back(pos);
            len = pos = 0;
            buf[0] = 0;
            shown = redraw(buf, 0, 0, 0, shown);
        } else if (c >= ' ' && c < 127) {
            if (ins || pos == len) {
                if (len >= LINEMAX)
                    continue;
                memmove(buf + pos + 1, buf + pos, len - pos + 1);
                len++;
            }
            buf[pos] = c;
            shown = redraw(buf, len, pos, pos + 1, shown);
            pos++;
        }
    }
    SYSCOM->expansion[1] = 0;           /* the keys as other programs want them */
    out('\n');
    fflush(stdout);
}
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

/* a directory's blocks: in files, free, the largest free area */
void space(int *dir, int *used, int *unused, int *largest)
{
    int *e;
    int i, gap, last;
    *used = *unused = *largest = 0;
    last = dir[1];                      /* the directory's end: the first free block */
    for (i = 1; i <= dir[8] + 1; i++) {
        e = dir + 13 * i;
        gap = (i <= dir[8] ? e[0] : dir[7]) - last;     /* the last: to DEOVBLK */
        *unused += gap;
        if (gap > *largest)
            *largest = gap;
        if (i <= dir[8]) {
            last = e[1];
            *used += e[1] - e[0];
        }
    }
}

/* entry i: name, blocks, date, kind */
void fileline(int *dir, int i)
{
    char name[16];
    int *e;
    int m;
    e = dir + 13 * i;
    dirname(dir, i, name);
    m = e[12] & 15;
    if (m == 0)                         /* no date */
        printf("%-15s %5d             %s\n", name, e[1] - e[0], kinds[e[2] & 7]);
    else
        printf("%-15s %5d  %2d-%s-%02d  %s\n", name, e[1] - e[0], (e[12] >> 4) & 31,
               months[m <= 12 ? m : 0], (e[12] >> 9) & 127, kinds[e[2] & 7]);
}

/* DIR [VOL: | #n:][PAT] */
void dir(char *arg)
{
    int dir[1024];
    char vol[20];
    char name[16];
    char *pat;
    int u, i, nf, shown, used, largest, unused, lines;
    u = target("dir", arg, dir, vol, &pat);
    if (!u)
        return;
    nf = dir[8];
    printf("%s: (#%d)\n", vol, u);
    lines = 1;
    shown = 0;
    for (i = 1; i <= nf; i++) {
        dirname(dir, i, name);
        if (*pat && !match(pat, name))
            continue;
        fileline(dir, i);
        shown++;
        if (!more(&lines))
            return;
    }
    space(dir, &used, &unused, &largest);
    printf("%d of %d files, %d blocks used, %d unused, %d in the largest area\n",
           shown, nf, used, unused, largest);
}

/* WHEREIS [VOL: | #n:]PAT: the matching files on every disk (or on one) */
void whereis(char *arg)
{
    int dir[1024];
    char vol[20];
    char name[16];
    char *pat;
    int k, u, one, i, found, vols, lines, here;
    if (!*arg) {
        printf("whereis: which files? (WHEREIS STDIO.H, WHEREIS *.C)\n");
        return;
    }
    one = 0;
    pat = arg;
    if (strchr(arg, ':') || arg[0] == '#') {
        one = target("whereis", arg, dir, vol, &pat);     /* a volume given: there only */
        if (!one)
            return;
    }
    found = vols = 0;
    lines = 0;
    for (k = 0; k < 8; k++) {
        u = units[k];
        if (one ? u != one : !readdir(u, dir))
            continue;
        dirname(dir, 0, vol);
        strcat(vol, ":");
        here = 0;
        for (i = 1; i <= dir[8]; i++) {
            dirname(dir, i, name);
            if (*pat && !match(pat, name))
                continue;
            printf("#%-2d %-8s ", u, vol);
            fileline(dir, i);
            here++;
            if (!more(&lines))
                return;
        }
        found += here;
        vols += here > 0;
    }
    if (found)
        printf("%d file%s on %d volume%s\n", found, found == 1 ? "" : "s", vols, vols == 1 ? "" : "s");
    else
        printf("whereis: no file %s on %s\n", pat, one ? vol : "any disk on line");
}

/* VOLUMES: every disk on line */
void volumes(void)
{
    int dir[1024];
    char vol[16];
    char pre[8];
    int k, u, used, unused, largest;
    prefix(pre);
    printf("Unit  Volume    Files  Blocks used\n");
    for (k = 0; k < 8; k++) {
        u = units[k];
        if (!readdir(u, dir))
            continue;
        dirname(dir, 0, vol);
        space(dir, &used, &unused, &largest);
        strcat(vol, ":");
        printf(" #%-2d  %-8s  %5d  %5d of %5d%s%s\n", u, vol, dir[8], used, dir[7],
               u == SYSCOM->sysunit ? "  (boot)" : "",
               strncmp(vol, pre, strlen(pre)) == 0 && vol[strlen(pre)] == ':' ? "  (prefix)" : "");
    }
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

/* ---- COPY, MOVE, RENAME ---- */

void __ptitle(char *name, char *title);     /* the C library's: a Pascal string */

/* the OS's file routines on an untyped file (the bytes as they are) */
int bopen(char *fib, char *path, int old)
{
    char title[32];
    __ptitle(path, title);
    __cxp0v(3, fib, 0, -1);             /* FINIT(fib, NIL, untyped) */
    __cxp0v(5, fib, title, old, 0);     /* FOPEN */
    return __cspi(34) == 0;
}

/* n blocks at block b: 1 if all of them */
int bio(char *fib, char *buf, int n, int b, int doread)
{
    int got;
    got = __cxp0i(28, fib, buf, 0, n, b, doread, 0, 0);    /* FBLOCKIO */
    return __cspi(34) == 0 && got == n;
}

/* write unit u's directory back; the OS's copy of a directory (GDIRP)
   is then forgotten, so that it reads this one before it changes it */
int writedir(int u, int *dir)
{
    unsigned char *g;
    __cspv(6, u, dir, 0, 2048, 2, 0);   /* UNITWRITE */
    g = (unsigned char *)SYSCOM->gdirp;
    if (g)
        g[6] = 0;                       /* its volume name: no volume */
    return __cspi(34) == 0;
}

/* the entry named name: its index, 0 if none */
int findentry(int *dir, char *name)
{
    char got[16];
    int i;
    for (i = 1; i <= dir[8]; i++) {
        dirname(dir, i, got);
        if (strcmp(got, name) == 0)
            return i;
    }
    return 0;
}

/* a file name for a new entry: upper case, 1 to 15 characters, none of
   : # * = ? , or a blank */
int newname(char *name)
{
    char *p;
    if (*name == 0 || strlen(name) > 15)
        return 0;
    for (p = name; *p; p++) {
        if (*p <= ' ' || strchr(":#*=?,", *p))
            return 0;
        *p = toupper(*p);
    }
    return 1;
}

/* entry i of unit su's directory (sdir) to unit du as dname: its blocks,
   then its kind, date and last byte; 1 if done */
int copyone(int su, int *sdir, int i, char *svol, int du, char *dvol, char *dname)
{
    int ddir[1024];
    char sfib[80];
    char dfib[80];
    char buf[2048];
    char sname[16];
    char path[24];
    int *e;
    int n, b, nb, j, had;
    e = sdir + 13 * i;
    nb = e[1] - e[0];
    dirname(sdir, i, sname);
    sprintf(path, "#%d:%s", su, sname);
    if (!bopen(sfib, path, 1)) {
        printf("%s:%s: cannot open it\n", svol, sname);
        return 0;
    }
    had = readdir(du, ddir) && findentry(ddir, dname);
    sprintf(path, "#%d:%s", du, dname);
    if (!bopen(dfib, path, 0)) {
        __cxp0v(6, sfib, 0);
        printf("%s:%s: cannot create it\n", dvol, dname);
        return 0;
    }
    for (b = 0; b < nb; b = b + n) {
        n = nb - b < 4 ? nb - b : 4;
        if (!bio(sfib, buf, n, b, 1) || !bio(dfib, buf, n, b, 0)) {
            __cxp0v(6, dfib, 2);        /* FCLOSE(PURGE) */
            __cxp0v(6, sfib, 0);
            printf("%s:%s: no room on %s (or a disk error)\n", svol, sname, dvol);
            return 0;
        }
    }
    __cxp0v(6, sfib, 0);
    __cxp0v(6, dfib, 1);                /* FCLOSE(LOCK): an old one of that name goes */
    if (__cspi(34) != 0 || !readdir(du, ddir) || !(j = findentry(ddir, dname))) {
        printf("%s:%s: cannot create it\n", dvol, dname);
        return 0;
    }
    /* the OS made it a data file of today, its last block full */
    e = ddir + 13 * j;
    e[2] = (e[2] & ~15) | (sdir[13 * i + 2] & 15);      /* DFKIND */
    e[11] = sdir[13 * i + 11];          /* DLASTBYTE */
    e[12] = sdir[13 * i + 12];          /* DACCESS */
    writedir(du, ddir);
    printf("%s:%s -> %s:%s%s\n", svol, sname, dvol, dname, had ? " (replaced)" : "");
    return 1;
}

/* entry name on unit u becomes new; 1 if done */
int renameone(int u, char *vol, char *name, char *new)
{
    int dir[1024];
    unsigned char *t;
    int i;
    if (!readdir(u, dir) || !(i = findentry(dir, name))) {
        printf("%s:%s: not there\n", vol, name);
        return 0;
    }
    if (findentry(dir, new)) {
        printf("%s:%s is there already\n", vol, new);
        return 0;
    }
    t = (unsigned char *)dir + 26 * i + 6;      /* DTID */
    t[0] = strlen(new);
    memcpy(t + 1, new, t[0]);
    if (!writedir(u, dir)) {
        printf("%s: cannot write its directory\n", vol);
        return 0;
    }
    printf("%s:%s -> %s:%s\n", vol, name, vol, new);
    return 1;
}

/* COPY / MOVE SRC DEST (move: 1) */
void copy(char *arg, int move)
{
    int sdir[1024];
    int ddir[1024];
    char svol[20];
    char dvol[20];
    char dname[20];
    char sname[16];
    char path[24];
    char idx[78];
    char *cmd;
    char *pat;
    char *dst;
    char *c;
    int su, du, i, n, k;
    cmd = move ? "move" : "copy";
    dst = arg;
    while (*dst && *dst != ' ')
        dst++;
    if (*dst)
        *dst++ = 0;
    while (*dst == ' ')
        dst++;
    if (!*arg || !*dst) {
        printf("%s: from where to where? (%s X.C #9:, %s X.C Y.C)\n", cmd, cmd, cmd);
        return;
    }
    su = target(cmd, arg, sdir, svol, &pat);
    if (!su)
        return;
    if (!*pat) {
        printf("%s: which files on %s:? (* for all of them)\n", cmd, svol);
        return;
    }
    n = 0;
    for (i = 1; i <= sdir[8]; i++) {
        dirname(sdir, i, sname);
        if (match(pat, sname))
            idx[n++] = i;
    }
    if (n == 0) {
        printf("%s: no file %s on %s:\n", cmd, pat, svol);
        return;
    }
    /* DEST: VOL: or #n(:) -- the same names; [VOL:]NAME -- one file */
    c = strchr(dst, ':');
    dname[0] = 0;
    if (c) {
        *c = 0;
        strcpy(dvol, dst);
        strncpy(dname, c + 1, 19);
        dname[19] = 0;
    } else if (dst[0] == '#')
        strcpy(dvol, dst);
    else {
        prefix(dvol);
        strncpy(dname, dst, 19);
        dname[19] = 0;
    }
    if (dname[0] && (wild(dname) || !newname(dname))) {
        printf("%s: %s is not a file name (no wildcards in the new name)\n", cmd, dname);
        return;
    }
    if (dname[0] && n > 1) {
        printf("%s: %d files: to a volume, not to one name (%s %s #9:)\n", cmd, n, cmd, pat);
        return;
    }
    du = unitof(dvol, ddir);
    if (du == 0 || !readdir(du, ddir)) {
        printf("%s: no disk %s on line\n", cmd, dvol);
        return;
    }
    dirname(ddir, 0, dvol);
    for (k = 0; k < n; k++) {
        dirname(sdir, idx[k], sname);
        if (du == su) {
            if (!dname[0] || strcmp(dname, sname) == 0) {
                printf("%s: %s:%s is that file\n", cmd, svol, sname);
                continue;
            }
            if (move) {                 /* on its own disk: a new name */
                renameone(su, svol, sname, dname);
                continue;
            }
        }
        if (!copyone(su, sdir, idx[k], svol, du, dvol, dname[0] ? dname : sname))
            return;
        if (move) {
            sprintf(path, "#%d:%s", su, sname);
            if (remove(path) != 0)
                printf("%s:%s: cannot delete it\n", svol, sname);
        }
    }
}

/* RENAME [VOL: | #n:]NAME NEW */
void rename(char *arg)
{
    int dir[1024];
    char vol[20];
    char name[16];
    char *pat;
    char *new;
    char *c;
    int u, i, n, k;
    new = arg;
    while (*new && *new != ' ')
        new++;
    if (*new)
        *new++ = 0;
    while (*new == ' ')
        new++;
    if (!*arg || !*new) {
        printf("rename: which file, and its new name? (RENAME X.C Y.C)\n");
        return;
    }
    u = target("rename", arg, dir, vol, &pat);
    if (!u)
        return;
    n = 0;
    for (i = 1; i <= dir[8]; i++) {
        dirname(dir, i, name);
        if (*pat && match(pat, name)) {
            n++;
            k = i;
        }
    }
    if (n == 0) {
        printf("rename: no file %s on %s:\n", pat, vol);
        return;
    }
    if (n > 1) {
        printf("rename: %d files match: one at a time\n", n);
        return;
    }
    c = strchr(new, ':');
    if (c) {
        *c = 0;
        if (unitof(new, dir) != u) {
            printf("rename: to another disk: MOVE\n");
            return;
        }
        new = c + 1;
    }
    if (wild(new) || !newname(new)) {
        printf("rename: %s is not a file name\n", new);
        return;
    }
    readdir(u, dir);
    dirname(dir, k, name);
    renameone(u, vol, name, new);
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
    loadhist();
    for (;;) {
        printf("shell> ");
        editline(line);
        n = strlen(line);
        while (n > 0 && line[n - 1] == ' ')
            line[--n] = 0;
        name = line;
        while (*name == ' ')
            name++;
        if (*name == 0)
            continue;
        if (!same(name, "bye"))
            savehist(name);             /* before it runs: pexec ends the shell */
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
        if (same(name, "whereis")) {
            whereis(args);
            continue;
        }
        if (same(name, "volumes") || same(name, "vols")) {
            volumes();
            continue;
        }
        if (same(name, "copy")) {
            copy(args, 0);
            continue;
        }
        if (same(name, "move")) {
            copy(args, 1);
            continue;
        }
        if (same(name, "rename") || same(name, "ren")) {
            rename(args);
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
