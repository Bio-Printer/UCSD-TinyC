/* memmark.c -- the least free memory of another program:
     MEMMARK FILL    fills the free memory with a pattern (memfill, psys.h)
     ... run the program (nothing else in between) ...
     MEMMARK SCAN    the longest run of the pattern still intact (memgap):
                     roughly the least free memory (words) the program had;
                     see memgap in psys.h for why only roughly (for an exact
                     figure: the emulator's Options > Track Least Free Memory).
                     MEMMARK's own printf takes about 1100 words of it.
   With no argument (X(ecute passes none) it asks: F(ill or S(can, one key. */
#include <stdio.h>
#include <conio.h>
#include <psys.h>

int main(int argc, char **argv)
{
    unsigned n;
    int c;
    if (argc == 2)
        c = argv[1][0];
    else {
        printf("memmark: F(ill, S(can? ");
        c = getch();
        printf("%c\n", c);
    }
    if (c == 'F' || c == 'f') {
        n = memfill();
        printf("memmark: %u words filled\n", n);
        return 0;
    }
    if (c == 'S' || c == 's') {
        n = memgap();
        printf("memmark: %u words free at the least\n", n);
        return 0;
    }
    printf("use: MEMMARK FILL, then the program, then MEMMARK SCAN\n");
    return 1;
}
