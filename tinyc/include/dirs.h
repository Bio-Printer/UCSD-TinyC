/* dirs.h -- the P-System's disks and their directories (the library's
   dirs.c and subvol.c; with <psys.h>) */
#ifndef __DIRS_H
#define __DIRS_H
#include <stdio.h>

/* the disk units, in the order the OS looks at them, and their directories
   (dirs.c).  A directory is blocks 2..5 of a disk, int dir[1024]: entry 0 the
   volume (its name at byte 6, DEOVBLK dir[7], DNUMFILES dir[8]), entries
   1..dir[8] the files, 13 words each (first block, last + 1, kind, name at
   byte 6, bytes in the last block at word 11, date at 12) */
extern int units[8];
extern char __svpath[64];               /* the shell's path ("" none): VOL:/A/B */
/* the functions are in a code segment of their own (DIRS: in memory only
   while one of them runs); the code after this header is in the main
   segment again */
#pragma segment DIRS
void direntname(int *dir, int i, char *s);  /* entry i's name, i = 0 the volume's */
int volunit(char *vol, int *dir);       /* "#n", "n", "*", VOL[:]: its unit, 0 none */
void prefixvol(char *s);                /* the prefix volume's name (8 bytes) */
int namesame(char *a, char *b);         /* the same names, upper or lower case */
int wildmatch(char *pat, char *name);   /* * or = any characters, ? any one */

/* sub-volumes: a Data file whose own blocks 2..5 hold a directory (a volume
   inside a file).  A path: VOL:A/B/NAME or VOL:/A/B/NAME, or without VOL: on
   the shell's path (CD) or the prefix volume; fopen and remove take them. */
int dirreadat(int u, int base, int *dir);   /* a (sub-)volume's directory; base: its first block */
#define dirread(u, dir) dirreadat(u, 0, dir)    /* unit u's directory: 0 if no disk there */
#define dirwrite(u, dir) dirwriteat(u, 0, dir)
void slashes(char *s);                  /* every \ in s a / (paths as Windows write them) */
int pathdir(char *path, int *dir, int *up, int *bp, char *leaf);    /* the directory path's file is in */

/* A file in a sub-volume found once (__svplace, while there is the memory for DIRS and a 2 KB
   directory), then opened and closed by that place without them: only the small SVAT segment
   comes in, the directory read into 2 KB the program has (vi: its window's, when the window
   has the memory).  While __svat points to one, fopen and fclose of a sub-volume path are its file. */
struct svat {
    int unit;                           /* the sub-volume's disk */
    int base;                           /* its first block (0: the volume's own directory) */
    char name[16];                      /* the file's name in it */
    int first;                          /* the file's first block on the disk (0: none yet) */
    int blocks;                         /* ... and its blocks */
    int *dir;                           /* 2 KB for the directory, while it is read */
};
extern struct svat *__svat;             /* (stdio) */
int __svplace(char *path, struct svat *at); /* path's place; 0 if its sub-volume is not there */

/* subvol.c, a code segment of its own (SUBVOL) */
#pragma segment SUBVOL
int svcanon(char *path, char *out);     /* path written out: VOL: or VOL:/A/B; 0 if not there */
int dirfree(int *dir, int nb);          /* the smallest free area for nb blocks: its first block, 0 none */
int fileson(int u);                     /* a file of the program open on unit u */
int dirkrunch(int u, int base, int *dir, int show);     /* files moved down, the free blocks at the end */
int svresize(char *path, char *size, char *cmd);        /* GROW, SHRINK: +N, -N or N blocks */
/* dirs.c, a small code segment of its own (SVAT): stdio's opens and closes by a place */
#pragma segment SVAT
int dirwriteat(int u, int base, int *dir);  /* a directory back (GDIRP forgotten): 0 on a disk error */
int findname(int *dir, char *name);     /* the entry called name, 0 none */
int __svopenat(FILE *f, char *mode, struct svat *at);   /* f opened (its record set) in at's sub-volume */
int __svcloseat(FILE *f, struct svat *at);  /* f's directory entry after it is written; at->first, blocks */
int __svclose(FILE *f);                 /* the same, with a 2 KB directory of its own */
#pragma segment MAIN
int __svname(char *name);               /* (stdio) a path into a sub-volume, or a name on the shell's path */

#endif
