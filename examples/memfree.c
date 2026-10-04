/* memfree.c -- prints the memory free to it (stack to heap) and exits with 7 */
#include <stdio.h>

int main(void)
{
    char here;
    char *heaptop;
    __cspv(32, &heaptop);               /* MARK: the top of the heap */
    printf("memfree: %u words free\n", (unsigned)(&here - heaptop) / 2);
    return 7;
}
