/* textpage.c -- text files: lines of every length moved to the next 1 KB
   page when the page fills (no line crosses a page), read back exactly */
#include <stdio.h>
#include <string.h>

/* line i of a file: its length and its characters */
int linelen(int i, int big)
{
    if (big)
        return (i * 97) % 1000;          /* 0..999: long lines too */
    return (i * 37) % 120;
}

int linechar(int i, int k)
{
    return 'A' + (i * 7 + k) % 26;
}

int roundtrip(char *name, int nlines, int big)
{
    FILE *f;
    int i, k, c, n, bad, total;
    f = fopen(name, "w");
    if (!f)
        return -1;
    total = 0;
    for (i = 0; i < nlines; i++) {
        n = linelen(i, big);
        for (k = 0; k < n; k++)
            putc(linechar(i, k), f);
        putc('\n', f);
        total += n + 1;
    }
    if (fclose(f) == EOF)
        return -2;
    f = fopen(name, "r");
    if (!f)
        return -3;
    bad = 0;
    for (i = 0; i < nlines; i++) {
        n = linelen(i, big);
        for (k = 0; k < n; k++)
            if (getc(f) != linechar(i, k))
                bad++;
        if (getc(f) != '\n')
            bad++;
    }
    if (getc(f) != EOF)
        bad++;
    fclose(f);
    printf("%s: %d lines, %d characters, %d wrong\n", name, nlines, total, bad);
    return bad;
}

int main(void)
{
    roundtrip("TPSHORT.TEXT", 400, 0);
    roundtrip("TPLONG.TEXT", 60, 1);
    remove("TPSHORT.TEXT");
    remove("TPLONG.TEXT");
    return 0;
}
