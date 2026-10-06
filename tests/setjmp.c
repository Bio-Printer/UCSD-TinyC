/* setjmp.c -- setjmp/longjmp: from deep calls, val 0 as 1, locals kept,
   nested handlers, through a function pointer, out of other segments
   (their reference counts given back: the segments load again), many
   times over (the stack does not grow) */
#include <stdio.h>
#include <setjmp.h>
#ifdef __UCSD__
#include <psys.h>
#endif

jmp_buf top;
jmp_buf inner;
int depth;

/* the segments' reference counts: the same after each longjmp as before */
static int refs[16];

void snap(void)
{
#ifdef __UCSD__
    unsigned *s = (unsigned *)SYSCOM - 0x4A;
    int i;
    for (i = 0; i < 16; i++)
        refs[i] = s[2 * i];
#endif
}

int same(void)
{
#ifdef __UCSD__
    unsigned *s = (unsigned *)SYSCOM - 0x4A;
    int i;
    for (i = 0; i < 16; i++)
        if (refs[i] != (int)s[2 * i]) {
            printf("segment %d: reference count %d, was %d\n", i, s[2 * i], refs[i]);
            return 0;
        }
#endif
    return 1;
}

void down(int *env, int n, int val)
{
    depth++;
    if (n == 0)
        longjmp(env, val);
    down(env, n - 1, val);
    printf("not reached\n");
}

int sum(int n)
{
    return n ? n + sum(n - 1) : 0;
}

#pragma segment SEGB
int bdown(int *env, int n, int val);

#pragma segment SEGC
/* B and C call each other: every call a segment change */
int cdown(int *env, int n, int val)
{
    if (n == 0)
        longjmp(env, val);
    return bdown(env, n - 1, val) + 1;
}

int cwork(int x)
{
    return x * 3;
}

#pragma segment SEGB
int bdown(int *env, int n, int val)
{
    if (n == 0)
        longjmp(env, val);
    return cdown(env, n - 1, val) + 1;
}

int bwork(int x)
{
    return x + 1000;
}

/* setjmp in another segment, longjmp from a third */
#pragma segment SEGD
int dcatch(int n)
{
    jmp_buf here;
    int r;
    int k;
    k = 5;
    r = setjmp(here);
    if (r)
        return r * 10 + k;
    k = 6;
    if (n)
        longjmp(here, n);
    return -1;
}

int dcatch2(int n)
{
    int r;
    if ((r = setjmp(inner)) != 0)
        return r + 100;
    bdown(inner, n, 9);
    return -1;
}

#pragma segment MAIN
void (*fp)(int *, int, int) = down;

int main(void)
{
    int i;
    int r;
    int count;
    volatile int local;

    /* the first return, then the longjmp's */
    local = 1;
    r = setjmp(top);
    printf("setjmp returned %d, local %d\n", r, local);
    if (r == 0) {
        local = 2;
        down(top, 10, 7);
    }
    printf("depth %d\n", depth);

    /* val 0 comes back as 1 */
    if ((r = setjmp(top)) == 0)
        down(top, 4, 0);
    printf("longjmp(top, 0) gives %d\n", r);

    /* in a loop: the same handler many times */
    snap();
    count = 0;
    for (i = 1; i <= 300; i++) {
        r = setjmp(top);
        if (r == 0)
            down(top, i % 20, i);
        count += r == i;
    }
    printf("300 longjmps: %d right, sum(50) still %d\n", count, sum(50));

    /* through a function pointer */
    switch (setjmp(top)) {
    case 0:
        fp(top, 2, 42);
        break;
    case 42:
        printf("via pointer: 42\n");
        break;
    default:
        printf("via pointer: wrong\n");
    }

    /* out of segments B and C, 40 calls deep, again and again */
    snap();
    count = 0;
    for (i = 0; i < 50; i++) {
        r = setjmp(top);
        if (!r)
            bdown(top, 40 + i % 3, i + 1);
        count += r == i + 1;
    }
    printf("from B and C: %d right, segments %s\n", count, same() ? "ok" : "WRONG");
    printf("B and C again: %d %d\n", bwork(1), cwork(7));
    printf("segments %s\n", same() ? "ok" : "WRONG");

    /* setjmp in segment D, longjmp from it and from B below it */
    snap();
    printf("dcatch: %d %d\n", dcatch(3), dcatch(0));
    printf("dcatch2: %d\n", dcatch2(17));
    printf("segments %s\n", same() ? "ok" : "WRONG");

    /* nested handlers: the inner one passes it on */
    if ((r = setjmp(top)) == 0) {
        if ((r = setjmp(inner)) == 0)
            down(inner, 5, 3);
        printf("inner caught %d\n", r);
        longjmp(top, r + 1);
    }
    printf("outer caught %d\n", r);
    printf("done\n");
    return 0;
}
