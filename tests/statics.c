/* statics.c -- a static array inside a function, its size given by its
   initializer: initialized in its own place, not over the first globals */
#include <stdio.h>
static const char first[] = "abcd";
int g = 5;
static void help(void)
{
    static const char *h[] = { "one", "two", 0 };
    static int counter = 40;
    int i;
    for (i = 0; h[i]; i++)
        printf("%s ", h[i]);
    printf("%d\n", ++counter);
}
int main(void)
{
    help();
    help();
    printf("%s %d\n", first, g);
    return 0;
}
