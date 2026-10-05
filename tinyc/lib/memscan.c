/* memscan.c -- Tiny-C library: memfill(), memgap(): how much of the free
   memory another program used, by a pattern in it (psys.h) */
#include "libint.h"
#include <psys.h>

#define MS_PAT   0xB6D9     /* both bytes above 127: no text, no small number */
#define MS_SLACK 64         /* bytes below the caller's frame: its own calls */

/* the free words between the heap (MARK) and the stack, from *start */
static unsigned msfree(unsigned **start, unsigned *here)
{
    char *h;
    unsigned lo;
    unsigned hi;
    __cspv(32, &h);                     /* MARK: the top of the heap */
    lo = ((unsigned)h + 1) & ~1;
    hi = (unsigned)here - MS_SLACK;
    *start = (unsigned *)lo;
    return hi > lo ? (hi - lo) / 2 : 0;
}

/* fill the free memory with the pattern; returns how many words */
unsigned memfill(void)
{
    unsigned *p;
    unsigned n;
    unsigned k;
    n = msfree(&p, &k);
    for (k = n; k; k--)
        *p++ = MS_PAT;
    return n;
}

/* the longest run of the pattern still intact anywhere: after another
   program, the least free memory it had -- as far as it wrote it (heap
   it allocated but never wrote still holds the pattern) */
unsigned memgap(void)
{
    unsigned *p;
    unsigned n;
    unsigned run;
    unsigned best;
    n = msfree(&p, &run);
    run = 0;
    best = 0;
    for (; n; n--)
        if (*p++ == MS_PAT) {
            if (++run > best)
                best = run;
        } else
            run = 0;
    return best;
}
