/* dirs.h -- the P-System's disks and their directories (the library's
   dirs.c; with <psys.h>) */
#ifndef __DIRS_H
#define __DIRS_H

/* the disk units, in the order the OS looks at them, and their directories
   (dirs.c).  A directory is blocks 2..5 of a disk, int dir[1024]: entry 0 the
   volume (its name at byte 6, DEOVBLK dir[7], DNUMFILES dir[8]), entries
   1..dir[8] the files, 13 words each (first block, last + 1, kind, name at
   byte 6, bytes in the last block at word 11, date at 12) */
extern int units[8];
int dirread(int u, int *dir);           /* unit u's directory: 0 if no disk there */
int dirwrite(int u, int *dir);          /* back on unit u (GDIRP forgotten): 0 on error */
void direntname(int *dir, int i, char *s);  /* entry i's name, i = 0 the volume's */
int volunit(char *vol, int *dir);       /* "#n", "n", "*", VOL[:]: its unit, 0 none */
void prefixvol(char *s);                /* the prefix volume's name (8 bytes) */
int namesame(char *a, char *b);         /* the same names, upper or lower case */
int wildmatch(char *pat, char *name);   /* * or = any characters, ? any one */

#endif
