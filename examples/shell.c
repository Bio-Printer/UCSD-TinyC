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
     CD PATH                 the path: a sub-volume (CD WORK:/A/B, CD A
                             -- a sub-volume A here, else the volume A --,
                             CD .., CD VOL: leaves it): names without a
                             volume (in the shell and in the programs it
                             runs) are in it, and the prefix is its volume
     DIR [/E] [VOL: | #n:][PAT]   the files of the prefix volume (or the
                             path), or of VOL, unit n or a sub-volume
                             (DIR A, DIR #5:/A/B/*.C), whose names match PAT: * or =
                             any characters, ? any one (DIR #5, DIR *.C,
                             DIR TOOLSRC:VI*.C, DIR #9:?.TEXT); /E adds
                             each file's first block and the bytes in its
                             last block, and the unused areas
     TYPE [VOL: | #n:]NAME   a text file on the console (with wildcards:
                             each file that matches, under its name)
     DELETE [VOL: | #n:]PAT  the files that match (with wildcards it lists
                             them and asks first, one key: Y deletes)
     WHEREIS [VOL: | #n:]PAT the files that match on every disk on line
                             (WHEREIS STDIO.H, WHEREIS *.C), or on one
     VOLUMES                 every disk on line: unit, volume, files,
                             blocks used of its size
     COPY SRC DEST           SRC: [VOL: | #n: | PATH]PAT; DEST: a volume
                             or sub-volume (VOL:, #n:, A/, VOL:/A/B/, or
                             A when A is a sub-volume: the same names) or,
                             for one file, a name (on the path or the
                             prefix volume unless VOL: or PATH): the
                             kind, date and length as they were; a whole
                             sub-volume copies as one file
     MOVE SRC DEST           the same, then SRC goes (in its own
                             directory: only its name changes)
     RENAME [VOL: | #n: | PATH]NAME NEW   one file's name, where it is
     MKDIR NAME [BLOCKS]     a sub-volume: a data file NAME (100 blocks
                             unless BLOCKS) that holds a volume of its own
                             (77 files); its files are VOL:NAME/FILE or
                             VOL:/NAME/FILE, and sub-volumes nest
                             (VOL:/A/B/FILE).  Programs open such paths
                             like any file name; DIR marks them Sub-volume
     RMDIR NAME              a sub-volume goes (one with files in it
                             after a Y; not one the path is in); DELETE
                             leaves sub-volumes alone
     MEM                     the shell's free memory
     BYE                     back to the Command: prompt
   Redirection and pipes: COMMAND >FILE puts what the command writes into FILE
   (>>FILE appends), COMMAND <FILE is its standard input, and A | B runs A,
   then B with A's output as its input (A | B | C ...; up to 8).  Works for
   the shell's own commands (DIR *.C >LIST.TEXT) and for programs whose main
   has argc and argv (GREP PAT FILES >OUT.TEXT, DIR *.C | GREP TEST).  A name
   ending in .TEXT is a text file (use it for anything you will TYPE or edit).
   \> \< \| are ordinary characters (GREP p-\>q FILE).  A pipe's
   in-between files are #4:PIPE1.TEXT and PIPE2.TEXT; they go when it ends.
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
#include <dirs.h>

#define MAXFOUND 8

/* ---- the command line: history and editing ---- */

#define NHIST 10
#define LINEMAX 120
#define HISTFILE "#4:SYSTEM.CMDS"
char hist[NHIST][LINEMAX + 1];          /* the oldest first */
int nhist;

/* the commands kept in #4:SYSTEM.CMDS: lines, then a NUL, then the path
   (CD into a sub-volume) and a NUL: the shell is started again after every
   program and finds them there */
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
    while (c != 0 && c != EOF)          /* (more than NHIST lines) */
        c = fgetc(f);
    n = 0;
    if (c == 0)
        while ((c = fgetc(f)) != EOF && c != 0 && n < (int)sizeof __svpath - 1)
            __svpath[n++] = c;
    __svpath[n] = 0;
    fclose(f);
}

void savestate(void)
{
    FILE *f;
    int i;
    f = fopen(HISTFILE, "wb");
    if (!f)
        return;
    for (i = 0; i < nhist; i++) {
        fputs(hist[i], f);
        fputc('\n', f);
    }
    fputc(0, f);
    fputs(__svpath, f);
    fputc(0, f);
    fclose(f);
}

/* a command entered: the newest in the history, which goes to the file */
void savehist(char *cmd)
{
    int i;
    if (!*cmd || (nhist > 0 && strcmp(hist[nhist - 1], cmd) == 0))
        return;
    if (nhist == NHIST) {
        for (i = 1; i < NHIST; i++)
            strcpy(hist[i - 1], hist[i]);
        nhist--;
    }
    strcpy(hist[nhist++], cmd);
    savestate();
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

/* CD: the prefix becomes the volume in unit n (or VOL, or *) */
/* a path (into a sub-volume): with a / in it, or on the shell's path */
int ispath(char *s)
{
    return strchr(s, '/') || (__svpath[0] && !strchr(s, ':') && s[0] != '#');
}

/* path as the shell keeps it: VOL:/A/B (the volume's name, capitals, no /
   at the end); .. goes up one; 0 when there is no such sub-volume */
int canon(char *path, char *out)
{
    int dir[1024];
    char vol[20];
    char leaf[16];
    char *c;
    char *p;
    int u, base, n;
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

void cd(char *arg)
{
    int dir[1024];
    char vol[16];
    char path[80];
    char sub[64];
    unsigned char *v;
    int u, n;
    if (*arg) {
        n = 0;
        if (ispath(arg) || (arg[0] == '.' && arg[1] == '.')
         || (!strchr(arg, ':') && arg[0] != '#' && arg[0] != '*' && !isdigit(arg[0]))) {   /* (CD A: a sub-volume A, else the volume A) */
            n = canon(arg, path);
            if (!n && (strchr(arg, '/') || arg[0] == '.')) {
                printf("cd: no sub-volume %s\n", arg);
                return;
            }
        }
        if (n) {
            n = strlen(path);
            strcpy(sub, path[n - 1] == ':' ? "" : path);        /* (VOL: again: no path) */
            *strchr(path, ':') = 0;
            arg = path;                         /* and the prefix: its volume */
        } else
            sub[0] = 0;
        u = volunit(arg, dir);
        if (u == 0 || !dirread(u, dir)) {
            printf("cd: no disk %s on line\n", arg);
            return;
        }
        strcpy(__svpath, sub);
        direntname(dir, 0, vol);
        v = (unsigned char *)__osvaraddr(OS_DKVID);
        n = strlen(vol);
        v[0] = n;
        memcpy(v + 1, vol, n);
        savestate();
    }
    prefixvol(vol);
    u = volunit(vol, dir);
    if (__svpath[0])
        printf("path is %s (#%d)\n", __svpath, u);
    else if (u)
        printf("prefix is %s: (#%d)\n", vol, u);
    else
        printf("prefix is %s: (not on line)\n", vol);
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

/* what target found besides: where its files are, to open them ("#5:" or
   "#5:/A/B/"), and the sub-volume's first block (0: the volume's own) */
char tfile[80];
int tbase;

/* [VOL: | #n:][PAT], or a path [VOL:][/]A/B/[PAT]: the unit (the directory
   read into dir; into vol what goes before a file's name: "VOL:" or
   "VOL:/A/B/") and *pat; 0 (and a message for cmd) when there is none */
int target(char *cmd, char *arg, int *dir, char *vol, char **pat)
{
    char path[80];
    char leaf[16];
    char *colon;
    int u, i;
    tbase = 0;
    if (ispath(arg)) {
        colon = strrchr(arg, '/');
        *pat = colon ? colon + 1 : arg;         /* the last name: the pattern */
        i = *pat - arg;
        memcpy(path, arg, i);
        path[i] = 0;
        if (!canon(path, vol)) {
            printf("%s: no sub-volume %s\n", cmd, *path ? path : __svpath);
            return 0;
        }
        strcat(vol, vol[strlen(vol) - 1] == ':' ? "" : "/");
        strcpy(path, vol);
        strcat(path, "X");
        pathdir(path, dir, &u, &tbase, leaf);
        sprintf(tfile, "#%d:%s", u, strchr(vol, ':') + 1);
        return u;
    }
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
        prefixvol(vol);
        *pat = arg;
    }
    u = volunit(vol, dir);
    if (u == 0 || !dirread(u, dir)) {
        printf("%s: no disk %s on line\n", cmd, *vol ? vol : ":");
        return 0;
    }
    direntname(dir, 0, vol);
    strcat(vol, ":");
    sprintf(tfile, "#%d:", u);
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

/* entry i of the directory at block base of unit u: a sub-volume (a data
   file whose blocks 2..5 are a directory, as pathdir takes them) */
int issub(int u, int base, int *dir, int i)
{
    int blk[256];
    unsigned char *d;
    int *e;
    e = dir + 13 * i;
    if ((e[2] & 15) != 5 && (e[2] & 15) != 0)
        return 0;
    __cspv(5, u, blk, 0, 512, base + e[0] + 2, 0);    /* UNITREAD: its first directory block */
    if (__cspi(34) != 0)
        return 0;
    d = (unsigned char *)blk;
    return d[6] >= 1 && d[6] <= 7 && blk[0] == 0 && blk[1] == 6 && blk[7] >= 6
        && blk[7] <= e[1] - e[0] && blk[8] >= 0 && blk[8] <= MAXDIR;
}

/* entry i: name, blocks, date, (ext: first block, bytes in the last block,) kind
   (sub: "Sub-volume") */
void fileline(int *dir, int i, int ext, int sub)
{
    char name[16];
    int *e;
    int m;
    e = dir + 13 * i;
    direntname(dir, i, name);
    m = e[12] & 15;
    printf("%-15s %5d  ", name, e[1] - e[0]);
    if (m == 0)                         /* no date */
        printf("         ");
    else
        printf("%2d-%s-%02d", (e[12] >> 4) & 31, months[m <= 12 ? m : 0], (e[12] >> 9) & 127);
    if (ext)
        printf("  %5d  %4d", e[0], e[11]);
    printf("  %s\n", sub ? "Sub-volume" : kinds[e[2] & 7]);
}

/* /E among a command's words (before or after the others): 1, and it is
   taken out of arg */
int optext(char *arg)
{
    char *p;
    int n;
    for (p = arg; (p = strchr(p, '/')) != NULL; p++)
        if ((p == arg || p[-1] == ' ') && toupper(p[1]) == 'E' && (p[2] == 0 || p[2] == ' ')) {
            memmove(p, p + 2, strlen(p + 2) + 1);
            while (*arg == ' ')
                memmove(arg, arg + 1, strlen(arg));
            n = strlen(arg);
            while (n > 0 && arg[n - 1] == ' ')
                arg[--n] = 0;
            return 1;
        }
    return 0;
}

/* DIR [/E] [VOL: | #n:][PAT]: /E (extended) adds each file's first block
   and the bytes in its last block, and (no PAT) the unused areas between
   the files, where they are */
void dir(char *arg)
{
    int dir[1024];
    char vol[80];
    char sub[96];
    char name[16];
    char *pat;
    int u, i, nf, shown, used, largest, unused, lines, ext, last, next;
    ext = optext(arg);
    u = target("dir", arg, dir, vol, &pat);
    if (!u)
        return;
    if (*pat && !wild(pat) && (i = findname(dir, pat)) != 0 && issub(u, tbase, dir, i)
     && strlen(arg) < sizeof sub - 2) {
        strcpy(sub, arg);                       /* DIR A, A a sub-volume: what is in it */
        strcat(sub, "/");
        u = target("dir", sub, dir, vol, &pat);
        if (!u)
            return;
    }
    nf = dir[8];
    printf("%s (#%d)\n", vol, u);
    lines = 1;
    if (ext) {
        printf("File            Blocks  Date       Start  Last  Kind\n");
        lines++;
    }
    shown = 0;
    last = dir[1];                      /* the directory's end */
    for (i = 1; i <= nf + 1; i++) {
        next = i <= nf ? dir[13 * i] : dir[7];
        if (ext && !*pat && next > last) {          /* an unused area */
            printf("< unused >      %5d             %5d\n", next - last, last);
            if (!more(&lines))
                return;
        }
        if (i > nf)
            break;
        last = dir[13 * i + 1];
        direntname(dir, i, name);
        if (*pat && !wildmatch(pat, name))
            continue;
        fileline(dir, i, ext, issub(u, tbase, dir, i));
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
    char vol[80];
    char name[16];
    char *pat;
    int k, u, one, i, found, vols, lines, here;
    if (!*arg) {
        printf("whereis: which files? (WHEREIS STDIO.H, WHEREIS *.C)\n");
        return;
    }
    one = 0;
    pat = arg;
    if (strchr(arg, ':') || strchr(arg, '/') || arg[0] == '#') {
        one = target("whereis", arg, dir, vol, &pat);     /* a volume or a path given: there only */
        if (!one)
            return;
    }
    found = vols = 0;
    lines = 0;
    for (k = 0; k < 8; k++) {
        u = units[k];
        if (one ? u != one : !dirread(u, dir))
            continue;
        if (!one) {
            direntname(dir, 0, vol);
            strcat(vol, ":");
        }
        here = 0;
        for (i = 1; i <= dir[8]; i++) {
            direntname(dir, i, name);
            if (*pat && !wildmatch(pat, name))
                continue;
            printf("#%-2d %-8s ", u, vol);
            fileline(dir, i, 0, 0);
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
    prefixvol(pre);
    printf("Unit  Volume    Files  Blocks used\n");
    for (k = 0; k < 8; k++) {
        u = units[k];
        if (!dirread(u, dir))
            continue;
        direntname(dir, 0, vol);
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
    char vol[80];
    char name[16];
    char path[96];
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
        direntname(dir, i, name);
        if (!wildmatch(pat, name))
            continue;
        found++;
        if (wild(pat))
            printf("--- %s%s\n", vol, name);
        if ((dir[13 * i + 2] & 15) != 3) {
            printf("%s: not a text file\n", name);
            continue;
        }
        sprintf(path, "%s%s", tfile, name);
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
        printf("type: no file %s on %s\n", pat, vol);
}

/* DELETE [VOL: | #n:]PAT: with wildcards, after a Y */
void delete(char *arg)
{
    int dir[1024];
    char vol[80];
    char names[77][16];
    char path[96];
    char *pat;
    int u, i, n, nf, c, subs;
    if (!*arg) {
        printf("delete: which files?\n");
        return;
    }
    u = target("delete", arg, dir, vol, &pat);
    if (!u)
        return;
    if (!*pat) {
        printf("delete: which files on %s? (* for all of them)\n", vol);
        return;
    }
    nf = dir[8];
    n = subs = 0;
    for (i = 1; i <= nf; i++) {
        direntname(dir, i, names[n]);
        if (!wildmatch(pat, names[n]))
            continue;
        if (issub(u, tbase, dir, i)) {          /* (RMDIR's) */
            printf("delete: %s%s is a sub-volume (RMDIR removes it)\n", vol, names[n]);
            subs++;
            continue;
        }
        n++;
    }
    if (n == 0) {
        if (!subs)
            printf("delete: no file %s on %s\n", pat, vol);
        return;
    }
    if (wild(pat)) {
        for (i = 0; i < n; i++)
            printf("  %s%s\n", vol, names[i]);
        printf("Delete %s %d file%s (Y/N)? ", n == 1 ? "this" : "these", n, n == 1 ? "" : "s");
        c = getch();
        printf("%c\n", c >= ' ' ? c : ' ');
        if (c != 'Y' && c != 'y')
            return;
    }
    for (i = 0; i < n; i++) {
        sprintf(path, "%s%s", tfile, names[i]);
        if (remove(path) == 0)
            printf("%s%s deleted\n", vol, names[i]);
        else
            printf("%s%s: cannot delete it\n", vol, names[i]);
    }
}

/* ---- COPY, MOVE, RENAME ----
   On a volume and in sub-volumes alike: the blocks go with UNITREAD and
   UNITWRITE, and the shell writes the directories itself (dirreadat,
   dirwriteat), as the OS would: entries in the order of their blocks. */

/* the shell's path is in the sub-volume path, or is it */
int pathin(char *path)
{
    char full[80];
    int n;
    n = canon(path, full) ? strlen(full) : 0;
    return n > 0 && full[n - 1] != ':' && strncmp(__svpath, full, n) == 0
        && (__svpath[n] == 0 || __svpath[n] == '/');
}

/* a file name for a new entry: upper case, 1 to 15 characters, none of
   : # * = ? , / or a blank */
int newname(char *name)
{
    char *p;
    if (*name == 0 || strlen(name) > 15)
        return 0;
    for (p = name; *p; p++) {
        if (*p <= ' ' || strchr(":#*=?,/", *p))
            return 0;
        *p = toupper(*p);
    }
    return 1;
}

/* entry i of dir goes (dir is not written) */
void unentry(int *dir, int i)
{
    memmove(dir + 13 * i, dir + 13 * (i + 1), (dir[8] - i) * 26);
    dir[8]--;
}

/* the smallest free area of dir with room for nb blocks: its first block,
   0 if there is none */
int freearea(int *dir, int nb)
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

/* sname, in the directory at block sbase of unit su, to the directory at
   block dbase of unit du as dname: its blocks, then its entry (the kind,
   date and last byte as they were; an old file of that name goes); 1 if
   done */
int copyone(int su, int sbase, char *sname, char *svol, int du, int dbase, char *dname, char *dvol)
{
    int buf[1024];                      /* the source's directory, then the blocks on their way */
    int ddir[1024];
    int ent[13];
    int *e;
    int i, j, had, nb, at, from, to, b, n;
    if (!dirreadat(su, sbase, buf) || (i = findname(buf, sname)) == 0 || !dirreadat(du, dbase, ddir)) {
        printf("%s%s: not there (or a disk error)\n", svol, sname);
        return 0;
    }
    memcpy(ent, buf + 13 * i, 26);
    nb = ent[1] - ent[0];
    from = sbase + ent[0];
    had = findname(ddir, dname);
    if (had && issub(du, dbase, ddir, had)) {
        printf("%s%s is a sub-volume: not replaced (RMDIR it first)\n", dvol, dname);
        return 0;
    }
    if (su == du && dbase >= from && dbase < from + nb) {
        printf("%s%s: not into itself\n", svol, sname);     /* (COPY A A/B/) */
        return 0;
    }
    at = freearea(ddir, nb);
    if (at == 0 || (!had && ddir[8] >= MAXDIR)) {
        printf("%s%s: no room on %s\n", svol, sname, dvol);
        return 0;
    }
    to = dbase + at;
    for (b = 0; b < nb; b = b + n) {
        n = nb - b < 4 ? nb - b : 4;
        __cspv(5, su, buf, 0, n * 512, from + b, 0);      /* UNITREAD */
        if (__cspi(34) == 0)
            __cspv(6, du, buf, 0, n * 512, to + b, 0);    /* UNITWRITE */
        if (__cspi(34) != 0) {
            printf("%s%s: disk error %d\n", svol, sname, __cspi(34));
            return 0;
        }
    }
    if (had)
        unentry(ddir, had);
    for (j = 1; j <= ddir[8] && ddir[13 * j] < at; j++)
        ;
    memmove(ddir + 13 * (j + 1), ddir + 13 * j, (ddir[8] - j + 1) * 26);
    ddir[8]++;
    e = ddir + 13 * j;
    memcpy(e, ent, 26);
    e[0] = at;
    e[1] = at + nb;
    memset((char *)e + 6, 0, 16);
    n = strlen(dname);
    ((unsigned char *)e)[6] = n;
    for (b = 0; b < n; b++)
        ((unsigned char *)e)[7 + b] = toupper(dname[b]);
    if (!dirwriteat(du, dbase, ddir)) {
        printf("%s%s: cannot write the directory of %s\n", svol, sname, dvol);
        return 0;
    }
    printf("%s%s -> %s%s%s\n", svol, sname, dvol, dname, had ? " (replaced)" : "");
    return 1;
}

/* name goes from the directory at block base of unit u: 1 if done */
int unlinkat(int u, int base, char *name)
{
    int dir[1024];
    int i;
    if (!dirreadat(u, base, dir) || (i = findname(dir, name)) == 0)
        return 0;
    unentry(dir, i);
    return dirwriteat(u, base, dir);
}

/* entry name in the directory at block base of unit u becomes new; 1 if done */
int renameone(int u, int base, char *vol, char *name, char *new)
{
    int dir[1024];
    unsigned char *t;
    int i;
    if (!dirreadat(u, base, dir) || (i = findname(dir, name)) == 0) {
        printf("%s%s: not there\n", vol, name);
        return 0;
    }
    if (findname(dir, new)) {
        printf("%s%s is there already\n", vol, new);
        return 0;
    }
    t = (unsigned char *)dir + 26 * i + 6;      /* DTID */
    memset(t, 0, 16);
    t[0] = strlen(new);
    memcpy(t + 1, new, t[0]);
    if (!dirwriteat(u, base, dir)) {
        printf("%s cannot write its directory\n", vol);
        return 0;
    }
    printf("%s%s -> %s%s\n", vol, name, vol, new);
    return 1;
}

/* DEST of COPY / MOVE: its directory (into dir; unit *du, its first block
   *db), where that is for messages (dvol: "VOL:" or "VOL:/A/") and dname
   (a new name, "" for the same names); 0 (said) if there is no such place.
   A name that is a sub-volume is the place, not a file (COPY X.C A) */
int dest(char *cmd, char *dst, int *dir, int *du, int *db, char *dvol, char *dname)
{
    char path[96];
    char leaf[16];
    int j;
    strncpy(path, dst, 90);
    path[90] = 0;
    if (path[0] == '#' && !strchr(path, ':'))
        strcat(path, ":");              /* #9: the unit */
    if (!pathdir(path, dir, du, db, leaf)) {
        printf("%s: no volume or sub-volume %s\n", cmd, dst);
        return 0;
    }
    if (leaf[0] && (j = findname(dir, leaf)) != 0 && issub(*du, *db, dir, j)) {
        strcat(path, "/");              /* into the sub-volume */
        pathdir(path, dir, du, db, leaf);
    }
    strcpy(dname, leaf);
    path[strlen(path) - strlen(leaf)] = 0;
    if (!canon(path, dvol)) {
        printf("%s: no volume or sub-volume %s\n", cmd, dst);
        return 0;
    }
    if (dvol[strlen(dvol) - 1] != ':')
        strcat(dvol, "/");
    return 1;
}

/* COPY / MOVE SRC DEST (move: 1) */
void copy(char *arg, int move)
{
    int dir[1024];
    char svol[80];
    char dvol[80];
    char dname[20];
    char path[96];
    char names[77][16];
    char *cmd;
    char *pat;
    char *dst;
    int su, sb, du, db, i, n, k;
    cmd = move ? "move" : "copy";
    dst = arg;
    while (*dst && *dst != ' ')
        dst++;
    if (*dst)
        *dst++ = 0;
    while (*dst == ' ')
        dst++;
    if (!*arg || !*dst) {
        printf("%s: from where to where? (%s X.C #9:, %s X.C Y.C, %s X.C A/)\n", cmd, cmd, cmd, cmd);
        return;
    }
    su = target(cmd, arg, dir, svol, &pat);
    if (!su)
        return;
    sb = tbase;
    if (!*pat) {
        printf("%s: which files on %s? (* for all of them)\n", cmd, svol);
        return;
    }
    n = 0;
    for (i = 1; i <= dir[8]; i++) {
        direntname(dir, i, names[n]);
        if (wildmatch(pat, names[n]))
            n++;
    }
    if (n == 0) {
        printf("%s: no file %s on %s\n", cmd, pat, svol);
        return;
    }
    if (!dest(cmd, dst, dir, &du, &db, dvol, dname))
        return;
    if (dname[0] && (wild(dname) || !newname(dname))) {
        printf("%s: %s is not a file name (no wildcards in the new name)\n", cmd, dname);
        return;
    }
    if (dname[0] && n > 1) {
        printf("%s: %d files: to a volume, not to one name (%s %s #9:)\n", cmd, n, cmd, pat);
        return;
    }
    for (k = 0; k < n; k++) {
        if (move) {
            sprintf(path, "%s%s", svol, names[k]);
            if (pathin(path)) {
                printf("%s: the shell's path is in %s (CD .. first)\n", cmd, path);
                continue;
            }
        }
        if (du == su && db == sb) {     /* the same directory */
            if (!dname[0] || namesame(dname, names[k])) {
                printf("%s: %s%s is that file\n", cmd, svol, names[k]);
                continue;
            }
            if (move) {                 /* only its name changes */
                renameone(su, sb, svol, names[k], dname);
                continue;
            }
        }
        if (!copyone(su, sb, names[k], svol, du, db, dname[0] ? dname : names[k], dvol))
            return;
        if (move && !unlinkat(su, sb, names[k]))
            printf("%s%s: cannot delete it\n", svol, names[k]);
    }
}

/* RENAME [VOL: | #n: | PATH]NAME NEW */
void rename(char *arg)
{
    int dir[1024];
    char vol[80];
    char name[16];
    char leaf[16];
    char path[96];
    char *pat;
    char *new;
    int u, base, u2, b2, i, n, k;
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
    base = tbase;
    n = 0;
    for (i = 1; i <= dir[8]; i++) {
        direntname(dir, i, name);
        if (*pat && wildmatch(pat, name)) {
            n++;
            k = i;
        }
    }
    if (n == 0) {
        printf("rename: no file %s on %s\n", pat, vol);
        return;
    }
    if (n > 1) {
        printf("rename: %d files match: one at a time\n", n);
        return;
    }
    direntname(dir, k, name);
    if (strchr(new, ':') || strchr(new, '/')) {         /* (the same place, said again) */
        if (!pathdir(new, dir, &u2, &b2, leaf) || u2 != u || b2 != base) {
            printf("rename: to another place: MOVE\n");
            return;
        }
        new = leaf;
    }
    if (wild(new) || !newname(new)) {
        printf("rename: %s is not a file name\n", new);
        return;
    }
    sprintf(path, "%s%s", vol, name);
    if (pathin(path)) {
        printf("rename: the shell's path is in %s (CD .. first)\n", path);
        return;
    }
    renameone(u, base, vol, name, new);
}

/* MKDIR NAME [BLOCKS]: a sub-volume: a data file NAME (BLOCKS long, 100
   if not given) whose blocks 2..5 are a directory with no files, its
   volume name NAME's first 7 characters (before a '.') */
void mkdir(char *arg)
{
    int dir[1024];
    char leaf[16];
    char *size;
    FILE *f;
    int u, base, n, i, date, ok;
    size = arg;
    while (*size && *size != ' ')
        size++;
    if (*size)
        *size++ = 0;
    while (*size == ' ')
        size++;
    n = *size ? atoi(size) : 100;
    if (!*arg || n < 7 || n > 32000) {
        printf("mkdir: which name, and how many blocks (7 to 32000)? (MKDIR WORK 200)\n");
        return;
    }
    date = dirread(SYSCOM->sysunit, dir) ? dir[10] : 0;     /* today: the boot disk's date */
    if (!pathdir(arg, dir, &u, &base, leaf) || !newname(leaf) || wild(leaf)) {
        printf("mkdir: %s: no such volume or sub-volume, or not a file name\n", arg);
        return;
    }
    if (findname(dir, leaf)) {
        printf("mkdir: %s is there already\n", arg);
        return;
    }
    f = fopen(arg, "wb");
    if (!f) {
        printf("mkdir: cannot create %s\n", arg);
        return;
    }
    memset(dir, 0, sizeof dir);
    ok = fwrite(dir, 1, 1024, f) == 1024;      /* blocks 0 and 1 */
    dir[1] = 6;                                 /* the directory: blocks 2..5 */
    for (i = 0; i < 7 && leaf[i] && leaf[i] != '.'; i++)
        ((char *)dir)[7 + i] = leaf[i];
    ((char *)dir)[6] = i;
    dir[7] = n;
    dir[10] = date;
    ok = ok && fwrite(dir, 1, 2048, f) == 2048;
    memset(dir, 0, 2048);
    for (i = 6; ok && i < n; i = i + 4)
        ok = fwrite(dir, 1, (n - i < 4 ? n - i : 4) * 512, f) == (n - i < 4 ? n - i : 4) * 512;
    if (fclose(f) != 0 || !ok) {
        remove(arg);
        printf("mkdir: no room for %d blocks\n", n);
        return;
    }
    if (pathdir(arg, dir, &u, &base, leaf) && (i = findname(dir, leaf)) != 0) {
        dir[13 * i + 2] = (dir[13 * i + 2] & ~15) | 5;     /* a data file */
        dirwriteat(u, base, dir);
    }
    printf("%s: a sub-volume of %d blocks\n", arg, n);
}

/* RMDIR NAME: a sub-volume goes (one with files in it after a Y; not one
   the shell's path is in) */
void rmdir(char *arg)
{
    int dir[1024];
    int sub[1024];
    char leaf[16];
    int u, base, i, c;
    if (!*arg) {
        printf("rmdir: which sub-volume? (RMDIR WORK, RMDIR #5:/A/B)\n");
        return;
    }
    if (!pathdir(arg, dir, &u, &base, leaf) || !leaf[0] || (i = findname(dir, leaf)) == 0) {
        printf("rmdir: no %s\n", arg);
        return;
    }
    if (!issub(u, base, dir, i) || !dirreadat(u, base + dir[13 * i], sub)) {
        printf("rmdir: %s is not a sub-volume (DELETE takes a file)\n", arg);
        return;
    }
    if (pathin(arg)) {
        printf("rmdir: the shell's path is in %s (CD .. first)\n", arg);
        return;
    }
    if (sub[8] > 0) {                           /* files in it: they go too, after a Y */
        printf("%s holds %d file%s: remove it and %s (Y/N)? ", arg, sub[8], sub[8] == 1 ? "" : "s",
               sub[8] == 1 ? "that one" : "them");
        c = getch();
        printf("%c\n", c >= ' ' ? c : ' ');
        if (c != 'Y' && c != 'y')
            return;
    }
    if (remove(arg) == 0)
        printf("%s removed\n", arg);
    else
        printf("rmdir: cannot remove %s\n", arg);
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

/* the shell's own commands */
char *builtins[] = { "cd", "dir", "type", "delete", "del", "whereis", "volumes", "vols", "copy", "move",
                     "rename", "ren", "mkdir", "rmdir", "mem", 0 };

int isbuiltin(char *name)
{
    int i;
    for (i = 0; builtins[i]; i++)
        if (namesame(name, builtins[i]))
            return 1;
    return 0;
}

int isbye(char *name)
{
    int n;
    n = 0;
    while (name[n] && name[n] != ' ')
        n++;
    return n == 3 && namesame(name, "bye");
}

void builtin(char *name, char *args)
{
    char here;
    char *heaptop;
    if (namesame(name, "cd")) {
        cd(args);
        return;
    }
    if (namesame(name, "dir")) {
        dir(args);
        return;
    }
    if (namesame(name, "type")) {
        type(args);
        return;
    }
    if (namesame(name, "delete") || namesame(name, "del")) {
        delete(args);
        return;
    }
    if (namesame(name, "whereis")) {
        whereis(args);
        return;
    }
    if (namesame(name, "volumes") || namesame(name, "vols")) {
        volumes();
        return;
    }
    if (namesame(name, "copy")) {
        copy(args, 0);
        return;
    }
    if (namesame(name, "move")) {
        copy(args, 1);
        return;
    }
    if (namesame(name, "rename") || namesame(name, "ren")) {
        rename(args);
        return;
    }
    if (namesame(name, "mkdir")) {
        mkdir(args);
        return;
    }
    if (namesame(name, "rmdir")) {
        rmdir(args);
        return;
    }
    if (namesame(name, "mem")) {
        __cspv(32, &heaptop);       /* MARK: the top of the heap */
        printf("shell: %u words free\n", (unsigned)(&here - heaptop) / 2);
        return;
    }
}

/* ---- redirection and pipes ----
   COMMAND >FILE (>>FILE appends) puts what the program writes (printf, puts, putchar) into
   FILE; COMMAND <FILE is its standard input.  A program takes them from its command line
   when its main has argc and argv; the shell does it for its own commands.  A name ending
   in .TEXT is a text file, any other is raw bytes.  A | B | C runs the programs one after
   the other: the output of each is a text file (#4:PIPE1.TEXT, #4:PIPE2.TEXT) that the
   next one reads, and those go when the last one ends.  \> \< \| are ordinary characters. */

#define PIPEFILE "#4:SYSTEM.PIPE"
#define PIPE1 "#4:PIPE1.TEXT"
#define PIPE2 "#4:PIPE2.TEXT"
#define MAXSTAGE 8

char pend[MAXSTAGE][LINEMAX + 1];       /* the stages still to run */
int npend;

/* the stages left, from #4:SYSTEM.PIPE: -1 if there is no such file */
int pipe_load(void)
{
    FILE *f;
    int c, n;
    npend = 0;
    f = fopen(PIPEFILE, "rb");
    if (!f)
        return -1;
    n = 0;
    while ((c = fgetc(f)) != EOF && c != 0 && npend < MAXSTAGE) {
        if (c == '\n') {
            pend[npend][n] = 0;
            if (n > 0)
                npend++;
            n = 0;
        } else if (n < LINEMAX)
            pend[npend][n++] = c;
    }
    fclose(f);
    return npend;
}

void pipe_save(void)
{
    FILE *f;
    int i;
    f = fopen(PIPEFILE, "wb");
    if (!f)
        return;
    for (i = 0; i < npend; i++) {
        fputs(pend[i], f);
        fputc('\n', f);
    }
    fputc(0, f);
    fclose(f);
}

/* the pipe is over (or broken off): its files go */
void pipe_finish(void)
{
    remove(PIPEFILE);
    remove(PIPE1);
    remove(PIPE2);
    npend = 0;
}

/* the next stage to run: 0 if none (a finished pipe is cleaned up) */
int pipe_next(char *line)
{
    int i, n;
    n = pipe_load();
    if (n < 0)
        return 0;
    if (n == 0) {
        pipe_finish();
        return 0;
    }
    strcpy(line, pend[0]);
    for (i = 1; i < npend; i++)
        strcpy(pend[i - 1], pend[i]);
    npend--;
    pipe_save();
    return 1;
}

/* the character at p is an operator (c) and not escaped by a backslash */
int isop(char *s, char *p, int c)
{
    return *p == c && (p == s || p[-1] != '\\');
}

/* line into its stages at the unescaped |; returns how many (the stage texts are in the line) */
int split_stages(char *line, char **stage)
{
    char *p;
    int n;
    n = 0;
    stage[n++] = line;
    for (p = line; *p; p++)
        if (isop(line, p, '|')) {
            *p = 0;
            if (n < MAXSTAGE)
                stage[n] = p + 1;
            n++;
        }
    return n;
}

/* a stage without its redirections: cmd; their names in in, out; *app: >> */
void redirs(char *stage, char *cmd, char *in, char *out, int *app)
{
    char *p, *d;
    int c, n;
    in[0] = out[0] = 0;
    *app = 0;
    d = cmd;
    for (p = stage; *p; p++) {
        if (isop(stage, p, '<') || isop(stage, p, '>')) {
            c = *p++;
            if (c == '>' && *p == '>') {
                *app = 1;
                p++;
            }
            while (*p == ' ')
                p++;
            n = 0;
            while (*p && *p != ' ' && !isop(stage, p, '<') && !isop(stage, p, '>')) {
                if (n < 63)
                    (c == '<' ? in : out)[n++] = *p;
                p++;
            }
            (c == '<' ? in : out)[n] = 0;
            p--;
            continue;
        }
        *d++ = *p;
    }
    while (d > cmd && d[-1] == ' ')
        d--;                    /* (the blanks before a redirection) */
    *d = 0;
}

/* the line with the redirections put at its end: ... <in >out */
void joinstage(char *line, char *cmd, char *in, char *out, int app)
{
    int n;
    strcpy(line, cmd);
    n = strlen(line);
    while (n > 0 && line[n - 1] == ' ')
        line[--n] = 0;
    if (*in) {
        strcat(line, " <");
        strcat(line, in);
    }
    if (*out) {
        strcat(line, app ? " >>" : " >");
        strcat(line, out);
    }
}

/* one command: a builtin (returns 1) or a program (pexec: returns only when it cannot start: 0) */
int runstage(char *stage)
{
    char cmd[LINEMAX + 1];
    char in[64];
    char out[64];
    char full[LINEMAX + 1];
    char *name, *args;
    int app, builtin_run;
    FILE save;
    redirs(stage, cmd, in, out, &app);
    name = cmd;
    while (*name == ' ')
        name++;
    if (*name == 0)
        return 1;
    args = name;
    while (*args && *args != ' ')
        args++;
    if (*args) {
        *args++ = 0;
        while (*args == ' ')
            args++;
    }
    if (isbuiltin(name)) {
        if (*out) {                     /* its output to the file, then the console again */
            memcpy(&save, stdout, sizeof(FILE));
            if (!freopen(out, app ? "a" : "w", stdout)) {
                printf("%s: cannot open it\n", out);
                return 1;
            }
        }
        builtin(name, args);
        if (*out) {
            fclose(stdout);
            memcpy(stdout, &save, sizeof(FILE));
        }
        return 1;
    }
    joinstage(full, args, in, out, app);        /* its arguments and redirections: the program takes them */
    run(name, full);
    return 0;
}

/* a line typed (or a stage taken from the pipe): the stages after the first go to the pipe
   file, then the first runs; returns 1 when it ran and the shell is still here */
void runline(char *line)
{
    char text[LINEMAX + 1];
    char *stage[MAXSTAGE + 1];
    char cmd[LINEMAX + 1];
    char in[64];
    char out[64];
    int n, i, app, ok;
    strcpy(text, line);
    n = split_stages(text, stage);
    if (n > MAXSTAGE) {
        printf("too many | (%d programs at most)\n", MAXSTAGE);
        return;
    }
    if (n == 1) {
        runstage(stage[0]);
        return;
    }
    npend = 0;
    for (i = 0; i < n; i++) {
        redirs(stage[i], cmd, in, out, &app);
        if (i > 0 && *in) {
            printf("a < after a |: the pipe is its input\n");
            return;
        }
        if (i < n - 1 && *out) {
            printf("a > before a |: the pipe is its output\n");
            return;
        }
        if (i > 0)
            strcpy(in, (i - 1) % 2 == 0 ? PIPE1 : PIPE2);
        if (i < n - 1)
            strcpy(out, i % 2 == 0 ? PIPE1 : PIPE2);
        if (strlen(cmd) + strlen(in) + strlen(out) + 5 > LINEMAX) {
            printf("command line too long (80 characters at most)\n");
            return;
        }
        joinstage(pend[i], cmd, in, out, app);
        if (strlen(pend[i]) > 80 && !isbuiltin(cmd)) {
            printf("command line too long (80 characters at most)\n");
            return;
        }
    }
    strcpy(text, pend[0]);              /* the first now, the others when it ends */
    for (i = 1; i < n; i++)
        strcpy(pend[i - 1], pend[i]);
    npend = n - 1;
    pipe_save();
    ok = runstage(text);
    if (!ok)
        pipe_finish();                  /* it did not start: no pipe */
}

int main(void)
{
    char line[130];                     /* more than pexec takes (80): it says so */
    char *name;
    int n, back;
    back = pexec_returned();
    if (!back)
        pipe_finish();                  /* a pipe a break left behind */
    else if (pipe_load() <= 0)
        printf("[exit status %d]\n", pexec_status());
    loadhist();
    for (;;) {
        if (pipe_next(line)) {          /* the next program of a pipe */
            runline(line);
            continue;
        }
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
        if (!isbye(name))
            savehist(name);             /* before it runs: pexec ends the shell */
        if (isbye(name))
            return 0;
        runline(name);
    }
}
