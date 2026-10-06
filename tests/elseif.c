/* elseif.c -- long if / else if chains (parsed in a loop, not one nesting
   level each), the dangling else, chains inside chains */
#include <stdio.h>

int classify(int n)
{
    if (n == 0) return 100;
    else if (n == 1) return 101;
    else if (n == 2) return 102;
    else if (n == 3) return 103;
    else if (n == 4) return 104;
    else if (n == 5) return 105;
    else if (n == 6) return 106;
    else if (n == 7) return 107;
    else if (n == 8) return 108;
    else if (n == 9) return 109;
    else if (n == 10) return 110;
    else if (n == 11) return 111;
    else if (n == 12) return 112;
    else if (n == 13) return 113;
    else if (n == 14) return 114;
    else if (n == 15) return 115;
    else if (n == 16) return 116;
    else if (n == 17) return 117;
    else if (n == 18) return 118;
    else if (n == 19) return 119;
    else if (n == 20) return 120;
    else if (n == 21) return 121;
    else if (n == 22) return 122;
    else if (n == 23) return 123;
    else if (n == 24) return 124;
    else if (n == 25) return 125;
    else if (n == 26) return 126;
    else if (n == 27) return 127;
    else if (n == 28) return 128;
    else if (n == 29) return 129;
    else if (n == 30) return 130;
    else if (n == 31) return 131;
    else if (n == 32) return 132;
    else if (n == 33) return 133;
    else if (n == 34) return 134;
    else if (n == 35) return 135;
    else if (n == 36) return 136;
    else if (n == 37) return 137;
    else if (n == 38) return 138;
    else if (n == 39) return 139;
    return -1;
}

/* no final else, statements instead of returns, blocks */
int chain(int n)
{
    int r;
    r = 0;
    if (n < 0) {
        r = -1;
    } else if (n < 10) {
        r = 1;
        if (n < 5)
            r = 2;
        else if (n < 8)
            r = 3;
    } else if (n < 100)
        r = 4;
    else if (n < 1000) {
        if (n & 1)
            if (n & 2)
                r = 5;
            else            /* belongs to if (n & 2) */
                r = 6;
        else
            r = 7;
    }
    return r;
}

int main(void)
{
    int i;
    int s;
    s = 0;
    for (i = -2; i < 42; i++)
        s = (s * 3 + classify(i)) % 1000;
    printf("classify: %d %d %d %d sum %d\n", classify(0), classify(17), classify(39), classify(40), s);
    printf("chain:");
    for (i = -3; i < 1200; i += 37)
        printf(" %d", chain(i));
    printf("\n%d %d %d %d %d %d %d\n", chain(-5), chain(3), chain(6), chain(9), chain(50), chain(503), chain(501));
    return 0;
}
