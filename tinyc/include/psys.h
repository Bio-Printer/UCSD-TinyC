/* psys.h -- the P-System's SYSCOM record: SYSCOM->sysunit, SYSCOM->memtop, ... */
#ifndef __PSYS_H
#define __PSYS_H

/* SYSCOM is the operating system's first global variable (its ^SYSCOMREC,
   passed by the boot). Tiny-C functions are lex level 1, so the OS's
   globals are two static levels up: __osvar(1) is one LOD 2,1. The
   address differs between engine layouts (0x02E4, or 0x0164 in P-Code mode
   with reclaimed memory), so ask the OS rather than use a constant. */
#define SYSCOM ((struct syscomrec *)__osvar(1))

/* SYSCOMREC of U134.4 OS source 1.07, GLOBALS.TEXT. The II.0 compiler lays
   out each list of names declared together ("A,B,C: T") last first, so
   e.g. LOWTIME comes before HIGHTIME and BOMBP before LASTMP. Packed
   fields fill each word from bit 0; chars here are bytes. */
struct crtctrlrec {                 /* CRTCTRL: output control codes */
    char escape, home, eraseeos, eraseeol, ndfs, rlf;
    char backspace;
    unsigned char fillcount;
    char clearline, clearscreen;
    unsigned prefixed;              /* bit n: code n needs the prefix */
};

struct crtinforec {                 /* CRTINFO: screen size, input keys */
    int height, width;
    char up, down, left, right;
    char eof, flush, brk, stop, chardel, badch;     /* brk: BREAK */
    char linedel, altmode;
    char prefix, etx, alpha_lock;
    unsigned prefixed;
};

struct segentry {                   /* SEGTABLE[n]: where segment n is */
    int codeunit;
    int diskaddr;                   /* absolute block */
    int codeleng;                   /* bytes */
};

struct syscomrec {
    int iorslt;                     /* result of the last I/O */
    int xeqerr;                     /* reason for the last execution error */
    int sysunit;                    /* unit booted from */
    int bugstate;
    char *gdirp;                    /* global directory (see VOLSEARCH) */
    int *bombp, *stkbase, *lastmp;  /* mark stack pointers */
    int jtab, seg, memtop;
    int bombipc;                    /* where the execution error was */
    int hltline;
    int brkpts[4];
    int retries;
    int expansion[9];
    unsigned lowtime, hightime;     /* the clock, 1/60 s */
    unsigned miscinfo;              /* MI_ bits below */
    int crttype;
    struct crtctrlrec crtctrl;
    struct crtinforec crtinfo;
    struct segentry segtable[16];   /* 0..MAX_SEG (15) */
};

/* SYSCOM->miscinfo */
#define MI_HASCLOCK   0x0001
#define MI_HAS8510A   0x0002
#define MI_HASLCCRT   0x0004
#define MI_HASXYCRT   0x0008
#define MI_SLOWTERM   0x0010
#define MI_STUPID     0x0020
#define MI_NOBREAK    0x0040
#define MI_USERKIND(m) (((m) >> 7) & 3)    /* NORMAL, AQUIZ, BOOKER, PQUIZ */
#define MI_IS_FLIPT   0x0200
#define MI_WORD_MACH  0x0400

#endif
