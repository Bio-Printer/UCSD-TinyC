/* dirs.h -- the P-System's disks and their directories (the library's
   dirs.c and subvol.c; with <psys.h>) */
#ifndef __DIRS_H
#define __DIRS_H

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
int dirread(int u, int *dir);           /* unit u's directory: 0 if no disk there */
int dirwrite(int u, int *dir);          /* back on unit u (GDIRP forgotten): 0 on error */
void direntname(int *dir, int i, char *s);  /* entry i's name, i = 0 the volume's */
int volunit(char *vol, int *dir);       /* "#n", "n", "*", VOL[:]: its unit, 0 none */
void prefixvol(char *s);                /* the prefix volume's name (8 bytes) */
int namesame(char *a, char *b);         /* the same names, upper or lower case */
int wildmatch(char *pat, char *name);   /* * or = any characters, ? any one */

/* sub-volumes: a Data file whose own blocks 2..5 hold a directory (a volume
   inside a file).  A path: VOL:A/B/NAME or VOL:/A/B/NAME, or without VOL: on
   the shell's path (CD) or the prefix volume; fopen and remove take them. */
int dirreadat(int u, int base, int *dir);   /* a (sub-)volume's directory; base: its first block */
int dirwriteat(int u, int base, int *dir);
int findname(int *dir, char *name);     /* the entry called name, 0 none */
int pathdir(char *path, int *dir, int *up, int *bp, char *leaf);    /* the directory path's file is in */

/* subvol.c, a code segment of its own (SUBVOL) */
#pragma segment SUBVOL
int svcanon(char *path, char *out);     /* path written out: VOL: or VOL:/A/B; 0 if not there */
int dirfree(int *dir, int nb);          /* the smallest free area for nb blocks: its first block, 0 none */
int fileson(int u);                     /* a file of the program open on unit u */
int dirkrunch(int u, int base, int *dir, int show);     /* files moved down, the free blocks at the end */
int svresize(char *path, char *size, char *cmd);        /* GROW, SHRINK: +N, -N or N blocks */
#pragma segment MAIN

#endif
