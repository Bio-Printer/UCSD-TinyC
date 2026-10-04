/* syscom.c -- <psys.h>: SYSCOM found from main, deep calls and another segment */
#include <stdio.h>
#include <psys.h>

#define OFF(f) ((char *)&SYSCOM->f - (char *)SYSCOM)

void check(char *where)
{
    struct syscomrec *sc = SYSCOM;
    unsigned a = (unsigned)sc;
    printf("%s: address %s, sysunit %d, OS on unit %d, memtop above it %s\n", where,
           a == 0x02E4 || a == 0x0164 ? "ok" : "WRONG", sc->sysunit,
           sc->segtable[0].codeunit, (unsigned)sc->memtop > a ? "yes" : "NO");
}

int deep(int n)
{
    if (n)
        return deep(n - 1);
    check("deep");
    return 0;
}

#pragma segment OTHER
void other(void)
{
    check("segment 7");
}
#pragma segment MAIN

int main(void)
{
    struct crtinforec *ci = &SYSCOM->crtinfo;
    check("main");
    deep(5);
    other();
    printf("offsets: memtop %d lowtime %d miscinfo %d", OFF(memtop), OFF(lowtime), OFF(miscinfo));
    printf(" crtctrl %d crtinfo %d", OFF(crtctrl), OFF(crtinfo));
    printf(" eof %d segtable %d\n", OFF(crtinfo.eof), OFF(segtable));
    printf("sizeof: crtctrl %d crtinfo %d syscom %d\n",
           (int)sizeof(struct crtctrlrec), (int)sizeof(struct crtinforec), (int)sizeof(struct syscomrec));
    printf("keys: eof %d stop %d flush %d chardel %d badch '%c' linedel %d altmode %d etx %d\n",
           ci->eof, ci->stop, ci->flush, ci->chardel, ci->badch, ci->linedel, ci->altmode, ci->etx);
    printf("clearscreen %d backspace %d xy crt %s lower case %s\n",
           SYSCOM->crtctrl.clearscreen, SYSCOM->crtctrl.backspace,
           SYSCOM->miscinfo & MI_HASXYCRT ? "yes" : "no", SYSCOM->miscinfo & MI_HASLCCRT ? "yes" : "no");
    return 0;
}
