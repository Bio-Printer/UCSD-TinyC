/* vipage.c -- vi: a window into big files (see vipage.h) */
#include "vi.h"
#if ENABLE_FEATURE_VI_PAGING

struct pgchunk *pg_ch;   /* before: 0 up; after: PG_MAXCH - 1 down */
int pg_nb, pg_na;        /* chunks before / after the window */
int pg_lb, pg_la;        /* lines before / after */
long pg_bb, pg_ba;       /* bytes before / after */
int pg_maxch;            /* the entries of pg_ch (no more than VI.SWAP's slots) */
unsigned char *pg_used;  /* PG_MAXSLOT bits: the slots in use */
FILE *pg_f;              /* VI.SWAP; NULL: no paging, all in text */
int pg_slots;            /* the slots VI.SWAP holds (at most PG_MAXSLOT) */
char *pg_buf;            /* PG_SLOT + 2: a chunk being searched */
int pg_cap;              /* the window's size */
int pg_lock;             /* pointers are held: the window must not move */
int pg_over;             /* ... and a command needed text outside it */
int pg_margin;           /* lines kept below the cursor (and half of it above the screen) */
char *pg_park;           /* pg_hold moved the cursor off its line to here: */
int pg_park_l, pg_park_c, pg_park_a;     /* its line, column; the range's start */
int pg_top;      /* the screen's top line while it is out of the window (else 0) */
int pg_keep;             /* the : command leaves the cursor where it was */
int pg_atend;    /* a delete reached the window's end, the next lines not in */
FILE *pg_rf;             /* the file being loaded: what is not yet in the stores; NULL: all in */
int pg_carry;            /* bytes of its last, unfinished line held in pg_buf */
int pg_cr;               /* the file's lines end in CR (a data file: a compiler's listing) */
int pg_bad;              /* the file could not be loaded to its end */


int pg_slot(void);
void pg_unslot(int i);
int pg_io(int slot, char *buf, int len, int wr);
char *pg_nl(char *p);
int pg_count(char *a, char *b);
int pg_abs(char *p);
void pg_retop(void);
char *pg_up(char *p, int n, int *got);
char *pg_down(char *p, int n, int *got);
int pg_spill_top(char *lim);
int pg_spill_bottom(char *lim);
int pg_fill_top(void);

/* ---- the swap file ---- */

int pg_slot(void)
{
	int i;
	for (i = 0; i < pg_slots; i++)
		if (!(pg_used[i >> 3] & (1 << (i & 7)))) {
			pg_used[i >> 3] |= 1 << (i & 7);
			return i;
		}
	return -1;
}

void pg_unslot(int i)
{
	pg_used[i >> 3] &= ~(1 << (i & 7));
}

int pg_io(int slot, char *buf, int len, int wr)
{
	if (fseek(pg_f, (long)slot * PG_SLOT, SEEK_SET) != 0)
		return -1;
	if (wr)
		return (int)fwrite(buf, 1, len, pg_f) == len ? 0 : -1;
	return (int)fread(buf, 1, len, pg_f) == len ? 0 : -1;
}

/* ---- lines in the window ---- */

/* the start of the line after p's (end if none) */
char *pg_nl(char *p)
{
	p = memchr(p, '\n', end - p);
	return p ? p + 1 : end;
}

/* the lines in [a, b) */
int pg_count(char *a, char *b)
{
	int n;
	for (n = 0; a < b && (a = memchr(a, '\n', b - a)) != NULL; a++)
		n++;
	return n;
}

int pg_room(void)
{
	return pg_cap - (int)(end - text) - 1;
}

/* the file's lines; the absolute number of p's line */
int pg_lines(void)
{
	return pg_lb + pg_count(text, end) + pg_la;
}

int pg_abs(char *p)
{
	return pg_lb + pg_count(text, begin_line(p)) + 1;
}

long pg_bytes(void)
{
	return pg_bb + (long)(end - text) + pg_ba;
}

/* the start of absolute line li, NULL if it is not in the window */
char *pg_line(int li)
{
	char *p;
	li -= pg_lb;
	if (li < 1)
		return NULL;
	for (p = text; li > 1 && p < end; li--)
		p = pg_nl(p);
	return p < end ? p : NULL;
}

/* the screen's top line is back in the window: screenbegin there again */
void pg_retop(void)
{
	char *p;
	if (pg_top && (p = pg_line(pg_top)) != NULL) {
		screenbegin = p;
		pg_top = 0;
	}
}

/* from line start p, n lines up / down: the line start reached, *got lines */
char *pg_up(char *p, int n, int *got)
{
	int k;
	for (k = 0; k < n && p > text; k++)
		p = begin_line(p - 1);
	*got = k;
	return p;
}

char *pg_down(char *p, int n, int *got)
{
	int k;
	for (k = 0; k < n && p < end; k++)
		p = pg_nl(p);
	*got = k;
	return p;
}

/* the window's contents moved by d bytes (a chunk in or out at the top) */
static void pg_shift(int d)
{
	end += d;
	dot += d;
	screenbegin += d;
	if (dot < text)
		dot = text;
	if (screenbegin < text)
		screenbegin = text;
#if ENABLE_FEATURE_VI_YANKMARK
	edit_file__cur_line = NULL;
#endif
}

/* the window got shorter at the bottom */
static void pg_cut(void)
{
	if (dot >= end)
		dot = end > text ? begin_line(end - 1) : text;
	if (screenbegin >= end)
		screenbegin = end > text ? begin_line(end - 1) : text;
#if ENABLE_FEATURE_VI_YANKMARK
	edit_file__cur_line = NULL;
#endif
}

/* ---- moving the window: a chunk at a time ---- */

/* len bytes at p (n lines) joined to chunk c, before its lines (first) or
   after them, when they fit its slot and the load is done (pg_buf is free):
   no new slot, no new entry (else VI.SWAP and the table fill up with part
   full chunks as the window moves to and fro); 1 if joined */
static int pg_join(struct pgchunk *c, char *p, int len, int n, int first)
{
	if (pg_rf || c->len + len > PG_SLOT || pg_io(c->slot, pg_buf + (first ? len : 0), c->len, 0))
		return 0;
	memcpy(first ? pg_buf : pg_buf + c->len, p, len);
	if (pg_io(c->slot, pg_buf, c->len + len, 1))
		return 0;
	c->len += len;
	c->lines += n;
	return 1;
}

/* lines from the top of the window, all of them starting before lim, onto
   the before stack (one chunk); 0 if none can go */
int pg_spill_top(char *lim)
{
	char *s;
	char *n;
	int k;
	int cnt;
	int len;
	s = text;
	while (s < lim) {
		n = pg_nl(s);
		if (n > lim || n - text > PG_SLOT)
			break;
		s = n;
	}
	len = s - text;
	if (len == 0)
		return 0;
	cnt = pg_count(text, s);
	if (!pg_nb || !pg_join(&PGB(pg_nb - 1), text, len, cnt, 0)) {
		if (pg_nb + pg_na >= pg_maxch || (k = pg_slot()) < 0)
			return 0;
		if (pg_io(k, text, len, 1)) {
			pg_unslot(k);
			return 0;
		}
		PGB(pg_nb).slot = k;
		PGB(pg_nb).len = len;
		PGB(pg_nb).lines = cnt;
		pg_nb++;
	}
	if (!pg_top && screenbegin < s)
		pg_top = pg_abs(screenbegin);
	pg_lb += cnt;
	pg_bb += len;
	memmove(text, s, end - s);
	memset(end - len, 0, len);
	pg_shift(-len);
	return 1;
}

/* lines from the bottom of the window, all of them starting at or after
   lim, onto the after stack (one chunk); 0 if none can go */
int pg_spill_bottom(char *lim)
{
	char *s;
	char *p;
	int k;
	int cnt;
	int len;
	s = end;
	while (s > lim && s > text) {
		p = s - 1;
		while (p > text && p[-1] != '\n')
			p--;
		if (p < lim || end - p > PG_SLOT)
			break;
		s = p;
	}
	len = end - s;
	if (len == 0)
		return 0;
	cnt = pg_count(s, end);
	if (!pg_na || !pg_join(&PGA(0), s, len, cnt, 1)) {
		if (pg_nb + pg_na >= pg_maxch || (k = pg_slot()) < 0)
			return 0;
		if (pg_io(k, s, len, 1)) {
			pg_unslot(k);
			return 0;
		}
		pg_na++;
		PGA(0).slot = k;
		PGA(0).len = len;
		PGA(0).lines = cnt;
	}
	if (!pg_top && screenbegin >= s)
		pg_top = pg_abs(screenbegin);
	pg_la += cnt;
	pg_ba += len;
	memset(s, 0, len);
	end = s;
	pg_cut();
	return 1;
}

/* p at the window's end after a delete, with lines after it: they come in,
   p at the start of the first (as it is without paging), lines at the top
   going out if there is no room; p */
char *pg_next_in(char *p)
{
	if (!pg_f || p < end)
		return p;
	while (PG_MORE() && !pg_fill_bottom()) {
		if (!pg_spill_top(end))
			break;
		p = end;
	}
	return p;
}

/* the chunk above the window into it; 0 if there is none or no room */
int pg_fill_top(void)
{
	int len;
	int k;
	if (!pg_nb)
		return 0;
	len = PGB(pg_nb - 1).len;
	if (pg_room() < len)
		return 0;
	memmove(text + len, text, end - text);
	if (pg_io(PGB(pg_nb - 1).slot, text, len, 0)) {
		memmove(text, text + len, end - text);
		status_line_bold("Cannot read " PG_SWAP);
		return 0;
	}
	pg_shift(len);
	k = pg_nb - 1;
	pg_lb -= PGB(k).lines;
	pg_bb -= len;
	pg_unslot(PGB(k).slot);
	pg_nb--;
	pg_retop();
	return 1;
}

/* the chunk below the window into it */
int pg_fill_bottom(void)
{
	int len;
	if (!pg_na && !pg_more())
		return 0;
	if (!pg_na)
		return 0;
	len = PGA(0).len;
	if (pg_room() < len)
		return 0;
	if (pg_io(PGA(0).slot, end, len, 0)) {
		memset(end, 0, len);
		status_line_bold("Cannot read " PG_SWAP);
		return 0;
	}
	end += len;
	pg_la -= PGA(0).lines;
	pg_ba -= len;
	pg_unslot(PGA(0).slot);
	pg_na--;
	pg_retop();
	return 1;
}

/* The screen's top line for the cursor on line ro after the screen's top went
   out of the window (pg_top): as it was if ro is on it, scrolled just enough
   if ro is near, else ro in the middle (sync_cursor; pg_fix keeps those lines);
   tot: the file's lines (pg_fix: those loaded, it does not load the rest) */
int pg_toprow(int ro, int tot)
{
	int last;
	int h;
	h = (rows - 1) / 2;
	last = pg_top + rows - 2;	// the screen's last line
	if (last > tot)
		last = tot;
	if (ro < pg_top && pg_top - ro + 1 > h || ro > last && ro - last + 1 > h)
		ro -= h;
	else if (ro > last)
		ro = pg_top + ro - last;
	else if (ro > pg_top)
		ro = pg_top;
	return ro < 1 ? 1 : ro;
}

/* Before every command, as far as the window allows (it may hold less than a
   screen): the screen in memory, from its top down; pg_margin / 2 lines above
   it (a jump puts the cursor in the middle of the screen); pg_margin lines
   below the cursor; and PG_ROOM bytes free.  Lines go out only beyond what is
   wanted and come in only below it, so this ends. */
void pg_fix(void)
{
	char *d;
	char *t;
	char *kt;
	char *kb;
	int up;
	int down;
	int scr;
	int upm;
	int dm;
	int moved;
	int guard;
	if (!pg_f || pg_lock)
		return;
	for (guard = 0; guard < 200; guard++) {
		d = begin_line(dot);
		t = begin_line(screenbegin);	// the screen stays in the window
		upm = pg_margin / 2;
		dm = pg_margin;
		if (t > d || pg_top) {		// (a jump: the screen is to be put round the cursor, its lines
			t = d;			// above the cursor's as sync_cursor will have them, the rest below)
			upm = (rows - 1) / 2;	// (just above the screen: scrolled to it, or it is in the middle)
			if (pg_top) {
				upm = pg_abs(d) - pg_toprow(pg_abs(d), pg_lines());
				dm = rows - upm;
			}
		}
		pg_down(t, dm, &scr);
		kt = pg_up(t, upm, &up);
		kb = pg_down(d < end ? pg_nl(d) : end, dm, &down);
		moved = 0;
		if (scr < dm && PG_MORE())
			moved = pg_fill_bottom() || pg_spill_top(t);
		if (!moved && up < upm && pg_nb)
			moved = pg_fill_top() || pg_spill_bottom(kb);
		if (!moved && down < dm && PG_MORE())
			moved = pg_fill_bottom() || pg_spill_top(kt);
		if (!moved && pg_room() < PG_ROOM)
			moved = pg_spill_bottom(kb) || pg_spill_top(kt);
		if (!moved)
			break;
	}
}

/* the line after (dir FORWARD) or before the cursor's, if the file has one */
void pg_edge(int dir)
{
	if (!pg_f)
		return;
	if (dir == FORWARD) {
		while (pg_nl(begin_line(dot)) >= end && PG_MORE()) {
			if (pg_lock) {
				pg_over = 1;
				return;
			}
			if (!pg_fill_bottom() && !pg_spill_top(begin_line(dot)))
				return;
		}
	} else {
		while (begin_line(dot) == text && pg_nb) {
			if (pg_lock) {
				pg_over = 1;
				return;
			}
			if (!pg_fill_top() && !pg_spill_bottom(pg_nl(begin_line(dot))))
				return;
		}
	}
}

/* a scroll by cnt lines: the lines screenbegin moves over, and a screen.  The
   window may hold less than that (a screen is 45 lines): screenbegin moves as
   far as the window allows with a whole screen below it (or above it, backwards),
   lines go out behind it, more come in ahead.  The lines it moved over; dot_scroll
   moves it over the rest, which are all in the window. */
int pg_scroll(int cnt, int dir)
{
	char *s;
	char *k;
	int got;
	int n;
	int moved;
	if (!pg_f || pg_lock)
		return 0;
	moved = 0;
	for (;;) {
		s = begin_line(screenbegin);
		if (dir > 0) {
			pg_down(s, cnt - moved + rows, &got);
			if (got >= cnt - moved + rows || !PG_MORE())
				return moved;
			n = got - rows;
			if (n > 0) {
				screenbegin = pg_down(s, n, &got);
				moved += n;
				if (dot < screenbegin)
					dot = screenbegin;
			}
			k = begin_line(dot < screenbegin ? dot : screenbegin);
			if (!pg_fill_bottom() && !pg_spill_top(k))
				return moved;
		} else {
			pg_up(s, cnt - moved, &got);
			if (got >= cnt - moved || !pg_nb)
				return moved;
			if (got > 0) {
				screenbegin = pg_up(s, got, &n);
				moved += got;
				s = begin_line(screenbegin);
				k = end_screen();
				if (dot > k)
					dot = begin_line(k);
			}
			/* the lines that stay on the screen; those below go out (the cursor
			   goes up onto the last of them, where the scroll would leave it) */
			n = cnt - moved < rows ? rows - (cnt - moved) : 1;
			k = pg_down(s, n, &got);
			if (dot >= k && k > text)
				dot = begin_line(k - 1);
			if (!pg_fill_top() && !pg_spill_bottom(k))
				return moved;
		}
	}
}

#endif
