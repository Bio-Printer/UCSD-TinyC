/* binderc.c -- BINDERC [FILE]: the Pascal System Binder, in Tiny-C.
 *
 * The Binder (BINDER.CODE on the system disks, written in Pascal; its
 * source is lost) puts a GOTOXY procedure of your own into SYSTEM.PASCAL,
 * so the system positions the cursor properly on your terminal.  This
 * program does exactly what the Binder does, and writes the same bytes.
 *
 * FILE is a code file whose first segment (segment 0, the main program of
 * a compiled Pascal program) holds the procedure  GOTOXY(X, Y: INTEGER)
 * as its first procedure (procedure 2: the first one declared).  Without
 * FILE (and from X(ecute) the name is asked for.  A name that is not found
 * as typed is tried with .CODE added.
 *
 * SYSTEM.PASCAL of the prefix volume is read and a new one is made (the
 * old one is replaced when the new one is closed):
 *   - block 0 of the code file (the segment dictionary) is kept, with the
 *     addresses and lengths of the segments changed;
 *   - segment 0 (PASCALSYSTEM) comes first, its length grown by the length
 *     of GOTOXY.  GOTOXY's code is put in front of the old code and is
 *     called procedure 29 (the system's GOTOXY) at lex level 0; the old
 *     procedure 29 stays where it was, unused;
 *   - the other segments follow, in segment number order, unchanged.
 * Run SETUP as well so the system knows your terminal.
 *
 * A code file: block 0 holds diskinfo[0..15], a (block address, length in
 * bytes) pair of words for each segment; a segment is word aligned and
 * ends with its procedure dictionary: the last word is the number of
 * procedures (high byte) and the segment number (low byte); before it, for
 * procedure n at 2 * n + 2 bytes from the end, a word that, taken from its
 * own address backwards, reaches the procedure's JTAB (its last two bytes:
 * procedure number, lex level).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SYSPROC 29              /* the system's GOTOXY: procedure number */

char block0[512];               /* of SYSTEM.PASCAL */
char userblk[512];              /* of the user's file */
int addr[16], leng[16];         /* the system's segments */
int oldaddr[16];                /* ... and where they were */
char hdr[512];                  /* the new block 0 */
FILE *sf, *uf, *nf;
int nbytes;                     /* written to the new file so far */

int word(char *b, int i)
{
    return (b[i] & 255) | ((b[i + 1] & 255) << 8);
}

int fail(char *msg)
{
    printf("\nBINDERC: %s\n", msg);
    exit(1);
    return 1;
}

/* the byte at position pos of the segment of file f starting at block a */
int byteat(FILE *f, int a, int pos)
{
    fseek(f, (long)a * 512 + pos, SEEK_SET);
    return fgetc(f) & 255;
}

int wordat(FILE *f, int a, int pos)
{
    return byteat(f, a, pos) | (byteat(f, a, pos + 1) << 8);
}

void put(int c)
{
    fputc(c, nf);
    nbytes++;
}

void putword(int w)
{
    put(w & 255);
    put((w >> 8) & 255);
}

/* pad the new file to a whole block */
void pad(void)
{
    while (nbytes % 512)
        put(0);
}

int main(int argc, char **argv)
{
    char name[40];
    int i, n, len, next;
    int ulen, ua, uents, entpos, rel, jtab, v4, sysent, c;
    printf("Pascal System Binder (C)\n\n");
    printf("This program modifies the SYSTEM.PASCAL of your default prefix\n");
    printf("disk.  If any of the files it expects to be around are missing,\n");
    printf("i.e. SYSTEM.PASCAL, or enough room (60 blocks) to re-create it,\n");
    printf("it will stop with a message.\n\n");
    printf("You also need to execute the program SETUP to get the system to\n");
    printf("work intelligently with your terminal.\n\n");
    if (argc > 1)
        strcpy(name, argv[1]);
    else {
        printf(" File with GOTOXY(X,Y: INTEGER) procedure:");
        gets(name);
    }
    uf = fopen(name, "rb");
    if (!uf) {
        strcat(name, ".CODE");
        uf = fopen(name, "rb");
    }
    if (!uf)
        fail("cannot open the GOTOXY code file");
    sf = fopen("SYSTEM.PASCAL", "rb");
    if (!sf)
        fail("cannot open SYSTEM.PASCAL");
    if (fread(userblk, 1, 512, uf) != 512 || fread(block0, 1, 512, sf) != 512)
        fail("a code file is too short");

    /* the user's segment 0 */
    ua = word(userblk, 0);
    ulen = word(userblk, 2);
    if (ulen < 8 || ulen > 4000)
        fail("segment 0 of the GOTOXY file is missing or too long");
    uents = byteat(uf, ua, ulen - 1);
    if (uents < 2 || byteat(uf, ua, ulen - 2) != 0)
        fail("segment 0 of the GOTOXY file has no procedure 2 (incompatible byte sex?)");
    entpos = ulen - 2 * (2 + 1);                /* procedure 2's dictionary entry */
    rel = wordat(uf, ua, entpos);
    jtab = entpos - rel;                        /* its JTAB: procedure, lex */
    if (jtab < 8 || jtab + 2 > entpos || byteat(uf, ua, jtab) != 2)
        fail("procedure 2 of the GOTOXY file is not a procedure 2");
    v4 = jtab + 2;                              /* its code ends here; it starts at 0 */

    /* the system's segments */
    for (i = 0; i < 16; i++) {
        addr[i] = word(block0, i * 4);
        leng[i] = word(block0, i * 4 + 2);
    }
    if (leng[0] < 2 * (SYSPROC + 2) || leng[0] > 30000 || addr[0] == 0)
        fail("SYSTEM.PASCAL has no segment 0 (incompatible byte sex?)");
    if (byteat(sf, addr[0], leng[0] - 1) < SYSPROC)
        fail("segment 0 of SYSTEM.PASCAL has no procedure 29");
    sysent = leng[0] - 2 * (SYSPROC + 1);       /* the dictionary entry to change */

    printf("\n Moving procedures around \n");
    nf = fopen("SYSTEM.PASCAL", "wb");
    if (!nf)
        fail("cannot create the new SYSTEM.PASCAL (room for 60 blocks?)");

    /* block 0: the dictionary, new addresses and the grown segment 0 */
    memcpy(hdr, block0, 512);
    next = 1;
    leng[0] = leng[0] + v4;
    for (i = 0; i < 16; i++) {
        oldaddr[i] = addr[i];
        if (leng[i]) {
            addr[i] = next;
            next = next + (leng[i] + 511) / 512;
        }
        hdr[i * 4] = addr[i] & 255;
        hdr[i * 4 + 1] = (addr[i] >> 8) & 255;
        hdr[i * 4 + 2] = leng[i] & 255;
        hdr[i * 4 + 3] = (leng[i] >> 8) & 255;
    }
    for (i = 0; i < 512; i++)
        put(hdr[i] & 255);

    /* segment 0: GOTOXY's code as procedure 29, then the system's old code */
    for (i = 0; i < v4; i++) {
        c = byteat(uf, ua, i);
        if (i == jtab)
            c = SYSPROC;
        else if (i == jtab + 1)
            c = 0;
        put(c);
    }
    n = leng[0] - v4;
    fseek(sf, (long)oldaddr[0] * 512, SEEK_SET);
    for (i = 0; i < n; i++) {
        c = fgetc(sf) & 255;
        if (i == sysent)                        /* procedure 29 now: from its entry back to the front */
            c = (sysent + v4 - jtab) & 255;
        else if (i == sysent + 1)
            c = ((sysent + v4 - jtab) >> 8) & 255;
        put(c);
    }
    pad();

    /* the other segments, in order */
    for (i = 1; i < 16; i++) {
        if (!leng[i])
            continue;
        len = leng[i];
        fseek(sf, (long)oldaddr[i] * 512, SEEK_SET);
        for (n = 0; n < len; n++)
            put(fgetc(sf) & 255);
        pad();
    }
    printf("\n Writing the new SYSTEM.PASCAL\n");
    fclose(uf);
    fclose(sf);
    fclose(nf);
    return 0;
}
