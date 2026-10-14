/* grow.c -- GROW NAME SIZE: the sub-volume NAME (a path: A, #5:A, #5:/A/B)
   becomes SIZE blocks: +N (N more), -N (N fewer) or N.  The same as SHRINK
   (shrink.c): both are the library's svresize (subvol.c).

   Growing takes the free blocks after the sub-volume; when there are not
   enough it moves to a free area that holds it, and when there is none the
   volume that holds it is krunched first (as KRUNCH does), then it moves.
   Shrinking krunches the sub-volume itself first when its files reach past
   the new end, and only then.  The status: 0 done, 1 a bad SIZE, 2 NAME is
   not a sub-volume, 3 no room (or too many files for SIZE, or a disk error).

   Run it from the shell: GROW WORK +100, GROW #5:/A/B 400. */
#pragma segment MAIN	/* calls within the program: 2-byte CGPs, no NOP after them */
#include <stdio.h>
#include <psys.h>
#include <dirs.h>

int main(int argc, char **argv)
{
    if (argc != 3) {
        fprintf(stderr, "usage: GROW NAME +N | -N | N   (a sub-volume, its size in blocks)\n");
        return 1;
    }
    return svresize(argv[1], argv[2], "grow");
}
