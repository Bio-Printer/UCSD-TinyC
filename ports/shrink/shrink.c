/* shrink.c -- SHRINK NAME [SIZE]: the sub-volume NAME (a path: A, #5:A,
   #5:/A/B) or the volume (#5:, WORK:) becomes SIZE blocks: -N (N fewer), +N
   (N more) or N; without SIZE as small as its directory and files let it be
   (krunched first if it must be), no free blocks left.  A volume's disk image
   becomes that size too (the emulator's CSP 139: version 2.09).  The same as
   GROW (grow.c): both are the library's svresize (subvol.c).

   Shrinking krunches the sub-volume first when its files reach past the new
   end (only then: the files keep their places otherwise); it cannot be
   smaller than its directory and files.  The blocks a sub-volume gives up are
   free in what holds it.  The status: 0 done, 1 a bad SIZE, 2 NAME is not a
   sub-volume or a volume, 3 too many files for SIZE (or no room, or a disk
   error).

   Run it from the shell: SHRINK WORK -50, SHRINK #5:/A/B 60, SHRINK WORK,
   SHRINK #5:. */
#pragma segment MAIN	/* calls within the program: 2-byte CGPs, no NOP after them */
#include <stdio.h>
#include <psys.h>
#include <dirs.h>

int main(int argc, char **argv)
{
    if (argc != 2 && argc != 3) {
        fprintf(stderr, "usage: SHRINK NAME [-N | +N | N]   (a sub-volume or a volume, its size in blocks; no size: no free blocks left)\n");
        return 1;
    }
    return svresize(argv[1], argc == 3 ? argv[2] : "", "shrink");
}
