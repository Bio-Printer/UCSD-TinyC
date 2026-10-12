/* redirect.c -- Tiny-C library: freopen, and the redirections on a command line
   (a program whose main has argc and argv gets them from __callmain: the shell's
   >NAME, >>NAME and <NAME on the command line are taken off and stdout / stdin go
   to / come from that file; \> and \< are ordinary characters) */
#include "libint.h"

FILE *freopen(char *name, char *mode, FILE *f)
{
    FILE *t;
    if ((f->flags & __F_CON) == 0 && f->flags)
        fclose(f);                      /* a file: closed; the console is just left */
    t = fopen(name, mode);
    if (!t && mode[0] == 'a')
        t = fopen(name, "w");        /* appending to a file that is not there yet: a new one */
    if (!t)
        return NULL;
    memcpy(f, t, sizeof(FILE));
    t->flags = 0;
    return f;
}

void __redirect(int *argcp, char **argv)
{
    int i, j, argc;
    char *a, *name, *mode;
    argc = *argcp;
    j = 1;
    for (i = 1; i < argc; i++) {
        a = argv[i];
        if (a[0] != '>' && a[0] != '<') {
            argv[j++] = a;
            continue;
        }
        mode = a[0] == '<' ? "r" : "w";
        name = a + 1;
        if (a[0] == '>' && a[1] == '>') {
            mode = "a";
            name++;
        }
        if (!*name && i + 1 < argc)
            name = argv[++i];
        if (!freopen(name, mode, a[0] == '<' ? stdin : stdout)) {
            fputs(name, stderr);
            fputs(": cannot open it\n", stderr);
            exit(1);
        }
    }
    argv[j] = NULL;
    *argcp = j;
}
