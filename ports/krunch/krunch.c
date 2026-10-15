/* krunch.c -- KRUNCH [-y] [-r] [VOL: | #n | PATH]: move the files of a disk (or
   a sub-volume) down so that the unused blocks between them become one area
   at the end (the Filer's K(runch).

     -y   no question: krunch at once (without it KRUNCH says what it will
          do and asks, one key: Y goes on)
     -r   every sub-volume in it too, at any depth (each krunched before the
          one that holds it), no question; KRUNCH -r * : every disk on line

   The disk is the prefix volume, or VOL: (a volume's name), #n (unit n) or
   * (the system volume); a PATH with a / in it is a sub-volume (KRUNCH A/,
   KRUNCH #5:/A/B).  Each file that has unused blocks before it is
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
   where they went, the rest where they were).  The moving is the library's
   dirkrunch (subvol.c), which GROW and SHRINK use too.  With -r the status
   is the worst of the disks and sub-volumes krunched (the others still are). */
#pragma segment MAIN	/* calls within the program: 2-byte CGPs, no NOP after them */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <conio.h>
#include <psys.h>
#include <dirs.h>

int dir[1024];

/* the directory at block base of unit u, in dir (vol: its name, "VOL:" or
   "VOL:/A/B/"), krunched: the status (main's) */
int krunch(int u, int base, char *vol, int yes)
{
    int *e;
    int i, k, next, moves, gain;
    next = dir[1];                      /* the first block after the directory */
    moves = 0;
    for (i = 1; i <= dir[8]; i++) {
        e = dir + 13 * i;
        if (e[0] < next || e[1] < e[0]) {
            fprintf(stderr, "krunch: %s's directory is not in order (entry %d)\n", vol, i);
            return 2;
        }
        if (e[0] > next)
            moves++;
        next = e[1];
    }
    gain = dir[7];                      /* the free blocks: after, one area */
    for (i = 1; i <= dir[8]; i++)
        gain = gain - (dir[13 * i + 1] - dir[13 * i]);
    gain = gain - dir[1];
    next = dir[7] - gain;
    if (moves == 0) {
        printf("%s (#%d) has nothing to move: %d blocks free, from block %d\n", vol, u, gain, next);
        return 0;
    }
    if (!yes) {
        printf("Krunch %s (#%d)? %d file%s to move; then %d blocks free, from block %d (Y/N) ",
               vol, u, moves, moves == 1 ? "" : "s", gain, next);
        fflush(stdout);
        k = getch();
        printf("%c\n", k > ' ' ? k : ' ');
        if (toupper(k) != 'Y')
            return 1;
    }
    k = dirkrunch(u, base, dir, 1);
    if (k == -2) {
        fprintf(stderr, "krunch: no memory\n");
        return 3;
    }
    if (k < 0)
        return 3;
    printf("%s (#%d) krunched: %d blocks free, from block %d\n", vol, u, gain, next);
    return 0;
}

/* -r: the sub-volumes in the directory at block base of unit u (read into dir
   again after each), theirs ..., then that directory itself: the worst status */
int krunchall(int u, int base, char *vol)
{
    char sub[80];
    int *e;
    int i, k, first, len, worst;
    worst = 0;
    for (i = 1; i <= dir[8]; i++) {
        e = dir + 13 * i;
        first = base + e[0];
        len = e[1] - e[0];
        k = e[2] & 15;
        if ((k != 5 && k != 0) || strlen(vol) > 62)
            continue;
        strcpy(sub, vol);
        if (sub[strlen(sub) - 1] == ':')
            strcat(sub, "/");
        direntname(dir, i, sub + strlen(sub));
        strcat(sub, "/");
        if (dirreadat(u, first, dir) && dir[7] <= len) {        /* a sub-volume */
            k = krunchall(u, first, sub);
            if (k > worst)
                worst = k;
        }
        if (!dirreadat(u, base, dir)) {
            fprintf(stderr, "krunch: disk error %d reading %s\n", __cspi(34), vol);
            return 3;
        }
    }
    k = krunch(u, base, vol, 1);
    return k > worst ? k : worst;
}

int main(int argc, char **argv)
{
    char vol[80];
    char path[80];
    char leaf[16];
    int u, base, i, k, yes, rec, worst;
    yes = rec = 0;
    vol[0] = 0;
    for (i = 1; i < argc; i++) {
        if (namesame(argv[i], "-y"))
            yes = 1;
        else if (namesame(argv[i], "-r"))
            rec = 1;
        else if (argv[i][0] == '-') {
            fprintf(stderr, "usage: KRUNCH [-y] [-r] [VOL: | #n | PATH]\n");
            return 2;
        } else {
            strncpy(vol, argv[i], 70);
            vol[70] = 0;
            slashes(vol);               /* (#5:\A: #5:/A) */
        }
    }
    if (rec && strcmp(vol, "*") == 0) {         /* KRUNCH -r *: every disk on line */
        worst = 0;
        for (i = 0; i < 8; i++) {
            u = units[i];
            if (!dirread(u, dir))
                continue;
            direntname(dir, 0, vol);
            strcat(vol, ":");
            if (fileson(u)) {
                fprintf(stderr, "krunch: a file of KRUNCH is open on %s (its output?)\n", vol);
                k = 2;
            } else
                k = krunchall(u, 0, vol);
            if (k > worst)
                worst = k;
        }
        return worst;
    }
    if (!vol[0])
        prefixvol(vol);
    base = 0;
    if (strchr(vol, '/')) {                     /* a sub-volume */
        strcpy(path, vol);
        if (path[strlen(path) - 1] != '/')
            strcat(path, "/");
        if (!pathdir(path, dir, &u, &base, leaf) || !svcanon(path, vol)) {
            fprintf(stderr, "krunch: no sub-volume %s\n", path);
            return 2;
        }
        strcat(vol, "/");
    } else {
        u = volunit(vol, dir);
        if (u == 0 || !dirread(u, dir)) {
            fprintf(stderr, "krunch: no disk %s on line\n", vol);
            return 2;
        }
        direntname(dir, 0, vol);
        strcat(vol, ":");
    }
    if (fileson(u)) {
        fprintf(stderr, "krunch: a file of KRUNCH is open on %s (its output?)\n", vol);
        return 2;
    }
    if (rec)
        return krunchall(u, base, vol);
    return krunch(u, base, vol, yes);
}
