/* args.c -- main(argc, argv): does what its arguments say (run it from SHELL) */
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>

/* the same word, ignoring upper/lower case */
int is(char *a, char *b)
{
    while (*a && toupper(*a) == toupper(*b)) {
        a++;
        b++;
    }
    return *a == 0 && *b == 0;
}

int main(int argc, char **argv)
{
    int i, n;
    if (argc < 2) {
        printf("args: no arguments (argc %d). Try, from SHELL:\n", argc);
        printf("  ARGS ADD 2 3     ARGS MUL 6 7     ARGS REPEAT 3 HELLO     ARGS ECHO A B C\n");
        return 1;
    }
    if (is(argv[1], "ADD") && argc == 4) {
        n = atoi(argv[2]) + atoi(argv[3]);
        printf("args: %s + %s = %d\n", argv[2], argv[3], n);
        return n;
    }
    if (is(argv[1], "MUL") && argc == 4) {
        n = atoi(argv[2]) * atoi(argv[3]);
        printf("args: %s * %s = %d\n", argv[2], argv[3], n);
        return n;
    }
    if (is(argv[1], "REPEAT") && argc == 4) {
        for (i = 0; i < atoi(argv[2]); i++)
            printf("args: %d %s\n", i + 1, argv[3]);
        return 0;
    }
    if (is(argv[1], "ECHO")) {
        printf("args: argc %d\n", argc);
        for (i = 0; i < argc; i++)
            printf("args: argv[%d] = \"%s\"\n", i, argv[i]);
        return argc;
    }
    printf("args: don't know %s with %d arguments\n", argv[1], argc - 1);
    return 2;
}
