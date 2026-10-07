/* vipage.h -- vi.c: a window into a file bigger than memory
   (ENABLE_FEATURE_VI_PAGING), the way the UCSD L2 editor does it.

   text..end holds a window of whole lines.  The rest of the file is in one
   temporary file, VI.SWAP, in slots of PG_SLOT bytes, each holding a chunk
   of whole lines; two stacks of chunk descriptors say which: "before" (the
   lines above the window, in file order, its top the chunk just above the
   window) and "after" (its top the chunk just below).  Moving the window
   writes the lines leaving it at one edge onto that side's stack and reads
   the top chunk of the other side's stack into it.  Every line is in exactly
   one place -- the window or one slot -- so saving writes before, window,
   after, and the file being edited is not kept open.  Slots are reused, so
   VI.SWAP holds at most about the file.

   Line numbers count from the file's first line: pg_lb lines are above the
   window.  Before every command pg_fix() keeps pg_margin lines (a screen)
   on both sides of the cursor in memory and PG_ROOM bytes free for typing.
   Commands that hold pointers into the window across motions (a range for
   d, c, y, <, >; a : command's line range) first bring the lines they need
   in (pg_hold) and lock the window (pg_lock); what would need text outside
   it then sets pg_over, and the command is refused rather than cut short.
   Searches go on into the stores, and G, :N, 'a move the window. */

#define PG_SLOT    1024         /* bytes per slot: a chunk, and the longest line */
#ifndef PG_MAXCH
#define PG_MAXCH   256          /* chunks in the stores (6 bytes each) */
#endif
#define PG_MAXSLOT 2048         /* slots in VI.SWAP: 2 MB */
#define PG_ROOM    512          /* free bytes kept in the window for typing */
#define PG_SWAP    "VI.SWAP"

struct pgchunk {
	int slot;               /* where in VI.SWAP: slot * PG_SLOT */
	int len;                /* bytes */
	int lines;
};

extern struct pgchunk *pg_ch;   /* before: 0 up; after: PG_MAXCH - 1 down */
extern int pg_nb, pg_na;        /* chunks before / after the window */
extern int pg_lb, pg_la;        /* lines before / after */
extern long pg_bb, pg_ba;       /* bytes before / after */
extern unsigned char *pg_used;  /* PG_MAXSLOT bits: the slots in use */
extern FILE *pg_f;              /* VI.SWAP; NULL: no paging, all in text */
extern char *pg_buf;            /* PG_SLOT + 2: a chunk being searched */
extern int pg_cap;              /* the window's size */
extern int pg_lock;             /* pointers are held: the window must not move */
extern int pg_over;             /* ... and a command needed text outside it */
extern int pg_margin;           /* lines kept on both sides of the cursor */
extern char *pg_park;           /* pg_hold moved the cursor off its line to here: */
extern int pg_park_l, pg_park_c, pg_park_a;     /* its line, column; the range's start */
extern int pg_top;      /* the screen's top line while it is out of the window (else 0) */
extern int pg_keep;             /* the : command leaves the cursor where it was */
extern int pg_atend;    /* a delete reached the window's end, the next lines not in */


#define PGB(i) pg_ch[i]                         /* 0 = the file's first chunk */
#define PGA(i) pg_ch[PG_MAXCH - pg_na + (i)]    /* 0 = the chunk after the window */

/* ---- what vi.c calls ---- */
int pg_room(void);
int pg_lines(void);
long pg_bytes(void);
char *pg_line(int li);
char *pg_next_in(char *p);
int pg_fill_bottom(void);
void pg_fix(void);
void pg_edge(int dir);
void pg_scroll(int cnt, int dir);
char *pg_goto(int li);
int pg_hold(int a, int b);
char *pg_makeroom(char *p, int size);
char *pg_search(const char *pat, int dir, int far);
int pg_find_line(const char *pat);
void pg_close(void);
void pg_init(void);
void pg_reset(void);
int pg_load(const char *fn);
int pg_save(const char *fn);
