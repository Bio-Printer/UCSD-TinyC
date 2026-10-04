/* cmpcode.c -- compare two code files byte by byte (Tiny-C Verify).
 *
 * In a code file block 0 names segment 1 after the program (bytes 72..79),
 * so a compiler linked as TINYC2 differs from TINYC there only: for .CODE
 * files those bytes are skipped.  Other files (e.g. libraries) are
 * compared in full.
 * Prints IDENTICAL or the first differences.
 */
#include <stdio.h>
#include <string.h>
#include <ctype.h>

static void ask(char *prompt, char *buf)
{
    int n;
    printf("%s", prompt);
    if (!fgets(buf, 40, stdin))
        buf[0] = 0;
    n = strlen(buf);
    while (n > 0 && (buf[n - 1] == '\n' || buf[n - 1] == '\r' || buf[n - 1] == ' '))
        buf[--n] = 0;
    for (n = 0; buf[n]; n++)            /* UCSD names ignore case: tinyc.code is TINYC.CODE */
        buf[n] = toupper(buf[n]);
}

int main(void)
{
    char a[44];
    char b[44];
    FILE *f;
    FILE *g;
    long pos;
    int c;
    int d;
    int diffs;
    int code;
    ask("First file? ", a);
    ask("Second file? ", b);
    f = fopen(a, "rb");
    g = fopen(b, "rb");
    if (!f || !g) {
        printf("cannot open %s\n", f ? b : a);
        return 1;
    }
    code = strstr(a, ".CODE") != 0;
    pos = 0;
    diffs = 0;
    for (;;) {
        c = getc(f);
        d = getc(g);
        if (c != d && !(code && pos >= 72 && pos < 80)) {
            if (diffs < 5)
                printf("differ at %ld: %d %d\n", pos, c, d);
            diffs++;
        }
        if (c == EOF || d == EOF)
            break;
        pos++;
    }
    fclose(f);
    fclose(g);
    if (diffs)
        printf("DIFFERENT: %d bytes\n", diffs);
    else
        printf("IDENTICAL: %ld bytes\n", pos);
    return diffs != 0;
}
