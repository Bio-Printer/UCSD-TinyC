/* shell.c -- a mini-shell: runs programs with pexec() and comes back */
#include <stdio.h>
#include <string.h>
#include <psys.h>

int main(void)
{
    char line[80];
    char here;
    char *heaptop;
    int n;
    if (pexec_returned())
        printf("[exit status %d]\n", pexec_status());
    for (;;) {
        printf("shell> ");
        if (!fgets(line, sizeof line, stdin))
            return 0;
        n = strlen(line);
        while (n > 0 && (line[n - 1] == '\n' || line[n - 1] == ' '))
            line[--n] = 0;
        if (n == 0)
            continue;
        if (strcmp(line, "bye") == 0 || strcmp(line, "BYE") == 0)
            return 0;
        if (strcmp(line, "mem") == 0 || strcmp(line, "MEM") == 0) {
            __cspv(32, &heaptop);       /* MARK: the top of the heap */
            printf("shell: %u words free\n", (unsigned)(&here - heaptop) / 2);
            continue;
        }
        switch (pexec(line)) {          /* returns only when it cannot run it */
        case -1: printf("%s: no such program\n", line); break;
        case -2: printf("%s: not linked\n", line); break;
        default: printf("%s: cannot find the shell's own code file\n", line); break;
        }
    }
}
