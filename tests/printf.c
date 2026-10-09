/* printf's conversions: flags, width, precision, *, l, and the float
   styles (values whose digits a float holds exactly) */
#include <stdio.h>

int main(void)
{
    int n;
    long big;
    char s[40];
    big = 2147483647L;
    printf("[%d] [%5d] [%-5d] [%05d] [%+d] [% d] [%+d]\n", 42, 42, 42, 42, 42, 42, -42);
    printf("[%.3d] [%8.3d] [%-8.3d] [%08.3d] [%05d]\n", 7, 7, 7, 7, -42);
    printf("[%x] [%X] [%#x] [%#X] [%#x] [%o] [%#o]\n", 255, 255, 255, 255, 0, 8, 8);
    printf("[%08x] [%-8x] [%#8x] [%#08x] [%.4x] [%u]\n", 4660, 4660, 4660, 4660, 18, 1000);
    printf("[%ld] [%ld] [%lu] [%lx] [%lo] [%12ld] [%-12ld]\n", big, -big - 1, 3000000000UL, 3735928559UL, 511L, -big, big);
    printf("[%c] [%3c] [%-3c] [%s] [%10s] [%-10s] [%.2s] [%6.2s]\n", 'a', 'b', 'c', "str", "right", "left", "cut", "cut");
    printf("[%*d] [%-*d] [%*d] [%.*d] [%*.*s]\n", 6, 1, 6, 2, -6, 3, 4, 5, 5, 2, "abc");
    printf("[%%] [%-4s]%n\n", "ab", &n);
    printf("%d\n", n);
    sprintf(s, "%s=%d,%x", "k", -1, 171);
    printf("[%s]\n", s);
    n = sprintf(s, "%8.3d", 5);
    printf("[%s] %d\n", s, n);
    printf("[%f] [%.2f] [%8.3f] [%-8.1f] [%+.1f] [%08.2f] [%.0f]\n", 1.5, 0.125, -2.25, 3.5, 100.0, -1.5, 2.0);
    printf("[%e] [%.2E] [%g] [%G] [%10.3e]\n", 1.5, 1024.0, 0.5, 100.0, -0.125);
    return 0;
}
