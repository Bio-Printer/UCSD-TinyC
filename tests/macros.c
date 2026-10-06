/* macros.c -- a macro name is not expanded again inside its own expansion,
   also when it comes from an expanded argument that is rescanned; #if
   inside a call's arguments spread over lines */
#include <stdio.h>
struct g { int rows; int cols; } G;
#define rows (G.rows)
#define cols (G.cols)
#define pc(a, b, c) pcur(a, b)
#define str(x) #x
#define xstr(x) str(x)
#define f(x) (x + f(1))
#define cat(a, b) a ## b
int pcur(int a, int b) { return a * 100 + b; }
int (f)(int x) { return x * 2; }
int main(void)
{
    rows = 7; cols = 3;
    printf("%d %d %d %s %d\n", pc(rows - 1, cols, 0), pc(rows, pc(cols, rows, 1), 2), f(f(2)), xstr(rows), cat(co, ls));
    printf("%s %d\n", "one"
#if 1
           " two"
#else
           " nine"
#endif
#ifdef NOT_DEFINED
           " ten"
#endif
           , pc(
#if 0
                1,
#endif
                rows, cols, 0));
    return 0;
}
