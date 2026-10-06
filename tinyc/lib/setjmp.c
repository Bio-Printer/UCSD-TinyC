/* setjmp.c -- Tiny-C library: setjmp(), longjmp() (setjmp.h) */
#include "libint.h"
#include <psys.h>

/* The mark stack control word of a call, at the callee's MP: the static
   link, the dynamic link (the caller's MP), the caller's JTAB, its SEGP,
   the IPC to return to and the SP to return with.  The P-machine keeps its
   registers MP, JTAB and SEGP in SYSCOM->lastmp, ->jtab and ->seg. */
#define MS_DYN  1
#define MS_JTAB 2
#define MS_SEG  3
#define MS_IPC  4
#define MS_SP   5

/* INTSEGT, just below SYSCOM: per segment its reference count and its
   address once loaded (what SEGP holds while it runs) */
#define INTSEGT ((unsigned *)SYSCOM - 0x4A)

/* the segment whose code is at a, among the loaded ones */
static unsigned *jmpseg(unsigned a)
{
    unsigned *s;
    int n;
    s = INTSEGT;
    for (n = 0; n < 16; n++, s += 2)
        if (s[0] && s[1] == a)
            return s;
    fputs("longjmp: unknown segment\n", stderr);
    abort();
    return s;
}

int setjmp(int *env)
{
    unsigned *mp;
    int i;
    mp = (unsigned *)SYSCOM->lastmp;    /* this call's own */
    for (i = 0; i < 5; i++)
        env[i] = mp[MS_DYN + i];
    return 0;
}

/* Declared void in setjmp.h: never returns to its caller.  It makes its
   own MSCW the one setjmp saved, so its return (RNP 1, as an int
   function) is setjmp's again, with val.  First it does the segment
   bookkeeping of the returns it skips: leaving a function for a caller
   in another segment counts that segment's reference down.  RNP does
   that once itself, for the segment SEGP names: so that the one it does
   is the one that frees code -- in the Harvard layout the code of the
   segments left behind goes only then -- SEGP is set to the first loaded
   (the highest) of the segments no longer in use, if there is one. */
int longjmp(int *env, int val)
{
    unsigned *f;
    unsigned *s;
    unsigned *last;
    unsigned cur;
    unsigned top;
    top = env[0];
    f = (unsigned *)SYSCOM->lastmp;     /* this call */
    cur = SYSCOM->seg;                  /* the segment it runs in */
    last = NULL;
    for (;;) {
        if (f[MS_SEG] != cur) {
            s = jmpseg(cur);
            if (--s[0] == 0 && (!last || s[1] > last[1]))
                last = s;
        }
        if (f[MS_DYN] == top)
            break;
        if (f[MS_DYN] <= (unsigned)f || f[MS_DYN] > top) {
            fputs("longjmp: setjmp's caller has returned\n", stderr);
            abort();
        }
        cur = f[MS_SEG];
        f = (unsigned *)f[MS_DYN];
    }
    f = (unsigned *)SYSCOM->lastmp;
    for (cur = 0; cur < 5; cur++)
        f[MS_DYN + cur] = env[cur];
    if (!val)
        val = 1;
    if (last) {
        last[0]++;
        SYSCOM->seg = last[1];
    } else if (env[MS_SEG - 1] != SYSCOM->seg)
        jmpseg(SYSCOM->seg)[0]++;
    return val;
}
