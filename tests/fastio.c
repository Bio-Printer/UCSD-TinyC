/* block-at-a-time stdio: fread, fgets and fwrite on text and binary files and
   the console must give what fgetc and fputc give, across 512-byte blocks and
   1K text pages, with DLE blank compression, lines with and without ends */
#include <stdio.h>
#include <string.h>

char ref[8000];
char got[8000];
char line[200];

int main(void)
{
    FILE *f;
    int i, n, m, k, c, bad, lines;
    char *p;

    /* a text file: 100 lines (about 5K: several pages), DLE blanks, a long
       line, a line of 100 blanks, an unfinished last line */
    f = fopen("FASTIO.TEXT", "w");
    for (i = 0; i < 100; i++) {
        if (i % 11 == 0) {
            fputc(16, f);
            fputc(32 + 5, f);       /* DLE: 5 blanks */
        }
        fprintf(f, "line %d%s\n", i, i % 7 ? " xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx" : "");
    }
    fwrite("a\nb\n", 1, 4, f);                      /* fwrite with LF */
    fwrite("0123456789012345678901234567890123456789012345678901234567890123456789", 1, 70, f);
    fputc('\n', f);
    fputs("last line, no end", f);
    fclose(f);

    /* the reference: fgetc */
    f = fopen("FASTIO.TEXT", "r");
    n = 0;
    while (n < 7000 && (c = fgetc(f)) != EOF)
        ref[n++] = c;
    fclose(f);
    printf("text: %d characters\n", n);

    /* fread in pieces of every size from 1 to 100 */
    bad = 0;
    for (k = 1; k <= 100; k += 9) {
        f = fopen("FASTIO.TEXT", "r");
        m = 0;
        while (m < 7000 && (i = fread(got + m, 1, k, f)) > 0)
            m += i;
        fclose(f);
        if (m != n || memcmp(got, ref, n))
            bad++;
    }
    printf("fread pieces: %s\n", bad ? "BAD" : "ok");

    /* fgets with a buffer of 200 and of 20 (lines cut) */
    bad = 0;
    for (k = 200; k >= 20; k -= 180) {
        f = fopen("FASTIO.TEXT", "r");
        m = 0;
        lines = 0;
        while (fgets(line, k, f)) {
            i = strlen(line);
            if (m + i > 7000)
                break;
            memcpy(got + m, line, i);
            m += i;
            lines++;
        }
        fclose(f);
        if (m != n || memcmp(got, ref, n))
            bad++;
    }
    printf("fgets: %s, %d lines at 20\n", bad ? "BAD" : "ok", lines);

    /* fread, then fgetc, then fgets, one file, mixed */
    f = fopen("FASTIO.TEXT", "r");
    m = fread(got, 1, 100, f);
    c = fgetc(f);
    fgets(line, 200, f);
    p = got + m;
    *p++ = c;
    strcpy(p, line);
    fclose(f);
    printf("mixed: %s\n", memcmp(got, ref, m + 1 + strlen(line)) ? "BAD" : "ok");

    /* binary: fwrite in odd pieces across blocks, fread back in other pieces */
    f = fopen("FASTIO.DAT", "w");
    for (i = 0; i < 3000; i++)
        ref[i] = i * 7 + (i >> 8);
    n = 0;
    k = 1;
    while (n < 3000) {
        m = 3000 - n < k ? 3000 - n : k;
        fwrite(ref + n, 1, m, f);
        n += m;
        k = k * 3 % 701 + 1;
    }
    fclose(f);
    f = fopen("FASTIO.DAT", "r");
    n = 0;
    k = 1;
    while ((m = fread(got + n, 1, k, f)) > 0) {
        n += m;
        k = k * 5 % 613 + 1;
    }
    fclose(f);
    printf("binary: %d bytes (a whole number of blocks), %s\n", n, memcmp(got, ref, 3000) ? "BAD" : "ok");

    /* the console: LF in the middle, runs of 80 and more, an empty write */
    fwrite("console: one\ntwo\n", 1, 17, stdout);
    for (i = 0; i < 200; i++)
        line[i] = 'a' + i % 26;
    line[200 - 1] = 0;
    fwrite(line, 1, 150, stdout);
    fwrite("\n", 1, 1, stdout);
    fwrite(line, 1, 0, stdout);
    fputs("fputs a line\n", stdout);
    fputs("", stdout);
    puts("puts a line");
    remove("FASTIO.TEXT");
    remove("FASTIO.DAT");
    return 0;
}
