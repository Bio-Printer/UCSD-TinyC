/* vipage.c -- vi: a window into big files (see vipage.h) */
#include "vi.h"
#if ENABLE_FEATURE_VI_PAGING

struct pgchunk *pg_ch;   /* before: 0 up; after: PG_MAXCH - 1 down */
int pg_nb, pg_na;        /* chunks before / after the window */
int pg_lb, pg_la;        /* lines before / after */
long pg_bb, pg_ba;       /* bytes before / after */
unsigned char *pg_used;  /* PG_MAXSLOT bits: the slots in use */
FILE *pg_f;              /* VI.SWAP; NULL: no paging, all in text */
char *pg_buf;            /* PG_SLOT + 2: a chunk being searched */
int pg_cap;              /* the window's size */
int pg_lock;             /* pointers are held: the window must not move */
int pg_over;             /* ... and a command needed text outside it */
int pg_margin;           /* lines kept on both sides of the cursor */
char *pg_park;           /* pg_hold moved the cursor off its line to here: */
int pg_park_l, pg_park_c, pg_park_a;     /* its line, column; the range's start */
int pg_top;      /* the screen's top line while it is out of the window (else 0) */
int pg_keep;             /* the : command leaves the cursor where it was */
int pg_atend;    /* a delete reached the window's end, the next lines not in */


static int pg_slot(void);
static void pg_unslot(int i);
static int pg_io(int slot, char *buf, int len, int wr);
static char *pg_nl(char *p);
static int pg_count(char *a, char *b);
static int pg_abs(char *p);
static void pg_retop(void);
static char *pg_up(char *p, int n, int *got);
static char *pg_down(char *p, int n, int *got);
static void pg_shift(int d);
static void pg_cut(void);
static int pg_spill_top(char *lim);
static int pg_spill_bottom(char *lim);
static int pg_fill_top(void);
static int pg_scan(struct pgchunk *c, int next, const char *pat, int dir, int *line, int *col);
static void pg_abandon(const char *fn, const char *why);

/* ---- the swap file ---- */

static int pg_slot(void)
{
	int i;
	for (i = 0; i < PG_MAXSLOT; i++)
		if (!(pg_used[i >> 3] & (1 << (i & 7)))) {
			pg_used[i >> 3] |= 1 << (i & 7);
			return i;
		}
	return -1;
}

static void pg_unslot(int i)
{
	pg_used[i >> 3] &= ~(1 << (i & 7));
}

static int pg_io(int slot, char *buf, int len, int wr)
{
	if (fseek(pg_f, (long)slot * PG_SLOT, SEEK_SET) != 0)
		return -1;
	if (wr)
		return (int)fwrite(buf, 1, len, pg_f) == len ? 0 : -1;
	return (int)fread(buf, 1, len, pg_f) == len ? 0 : -1;
}

/* ---- lines in the window ---- */

/* the start of the line after p's (end if none) */
static char *pg_nl(char *p)
{
	while (p < end && *p != '\n')
		p++;
	return p < end ? p + 1 : end;
}

/* the lines in [a, b) */
static int pg_count(char *a, char *b)
{
	int n;
	for (n = 0; a < b; a++)
		if (*a == '\n')
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

static int pg_abs(char *p)
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
static void pg_retop(void)
{
	char *p;
	if (pg_top && (p = pg_line(pg_top)) != NULL) {
		screenbegin = p;
		pg_top = 0;
	}
}

/* from line start p, n lines up / down: the line start reached, *got lines */
static char *pg_up(char *p, int n, int *got)
{
	int k;
	for (k = 0; k < n && p > text; k++)
		p = begin_line(p - 1);
	*got = k;
	return p;
}

static char *pg_down(char *p, int n, int *got)
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

/* lines from the top of the window, all of them starting before lim, onto
   the before stack (one chunk); 0 if none can go */
static int pg_spill_top(char *lim)
{
	char *s;
	char *n;
	int k;
	int len;
	s = text;
	while (s < lim) {
		n = pg_nl(s);
		if (n > lim || n - text > PG_SLOT)
			break;
		s = n;
	}
	len = s - text;
	if (len == 0 || pg_nb + pg_na >= PG_MAXCH)
		return 0;
	k = pg_slot();
	if (k < 0)
		return 0;
	if (pg_io(k, text, len, 1)) {
		pg_unslot(k);
		return 0;
	}
	if (!pg_top && screenbegin < s)
		pg_top = pg_abs(screenbegin);
	PGB(pg_nb).slot = k;
	PGB(pg_nb).len = len;
	PGB(pg_nb).lines = pg_count(text, s);
	pg_lb += PGB(pg_nb).lines;
	pg_bb += len;
	pg_nb++;
	memmove(text, s, end - s);
	memset(end - len, 0, len);
	pg_shift(-len);
	return 1;
}

/* lines from the bottom of the window, all of them starting at or after
   lim, onto the after stack (one chunk); 0 if none can go */
static int pg_spill_bottom(char *lim)
{
	char *s;
	char *p;
	int k;
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
	if (len == 0 || pg_nb + pg_na >= PG_MAXCH)
		return 0;
	k = pg_slot();
	if (k < 0)
		return 0;
	if (pg_io(k, s, len, 1)) {
		pg_unslot(k);
		return 0;
	}
	if (!pg_top && screenbegin >= s)
		pg_top = pg_abs(screenbegin);
	pg_na++;
	PGA(0).slot = k;
	PGA(0).len = len;
	PGA(0).lines = pg_count(s, end);
	pg_la += PGA(0).lines;
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
	while (pg_na && !pg_fill_bottom()) {
		if (!pg_spill_top(end))
			break;
		p = end;
	}
	return p;
}

/* the chunk above the window into it; 0 if there is none or no room */
static int pg_fill_top(void)
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

/* Before every command: pg_margin lines in memory on both sides of the
   cursor (as far as the file has them), and PG_ROOM bytes free.  Lines go
   out only beyond the margins and come in only below them, so this ends. */
void pg_fix(void)
{
	char *d;
	char *kt;
	char *kb;
	int up;
	int down;
	int moved;
	int guard;
	if (!pg_f || pg_lock)
		return;
	for (guard = 0; guard < 200; guard++) {
		d = begin_line(dot);
		kt = pg_up(d, pg_margin, &up);
		kb = pg_down(d < end ? pg_nl(d) : end, pg_margin, &down);
		if (up < pg_margin && pg_nb)
			moved = pg_fill_top() || pg_spill_bottom(kb);
		else if (down < pg_margin && pg_na)
			moved = pg_fill_bottom() || pg_spill_top(kt);
		else if (pg_room() < PG_ROOM)
			moved = pg_spill_bottom(kb) || pg_spill_top(kt);
		else
			moved = 0;
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
		while (pg_nl(begin_line(dot)) >= end && pg_na) {
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

/* a scroll by cnt lines: the lines screenbegin moves over, and a screen */
void pg_scroll(int cnt, int dir)
{
	char *s;
	char *k;
	int got;
	if (!pg_f || pg_lock)
		return;
	for (;;) {
		s = begin_line(screenbegin);
		if (dir > 0) {
			pg_down(s, cnt + rows, &got);
			if (got >= cnt + rows || !pg_na)
				return;
			k = begin_line(dot < screenbegin ? dot : screenbegin);
			if (!pg_fill_bottom() && !pg_spill_top(k))
				return;
		} else {
			pg_up(s, cnt, &got);
			if (got >= cnt || !pg_nb)
				return;
			k = pg_down(s, rows, &got);
			if (pg_nl(begin_line(dot)) > k)
				k = pg_nl(begin_line(dot));
			if (!pg_fill_top() && !pg_spill_bottom(k))
				return;
		}
	}
}

/* Absolute line li (1 .. the last) into the window; its start.  Far away:
   the window goes out whole to the side away from li, the chunks between
   change stacks without being read, and li's chunk comes in.  Locked: only
   in the window (else pg_over and the window's nearest line). */
char *pg_goto(int li)
{
	char *p;
	int n;
	if (li < 1)
		li = 1;
	n = pg_lines();
	if (li > n)
		li = n;
	p = pg_line(li);
	if (p || !pg_f)
		return p ? p : (li <= pg_lb ? text : begin_line(end - 1));
	if (pg_lock) {
		pg_over = 1;
		return li <= pg_lb ? text : begin_line(end - 1);
	}
	if (li <= pg_lb) {
		while (end > text && pg_spill_bottom(text))
			;
		while (end == text && pg_nb && li <= pg_lb - PGB(pg_nb - 1).lines) {
			pg_nb--;
			pg_na++;
			PGA(0) = PGB(pg_nb);
			pg_lb -= PGA(0).lines;
			pg_la += PGA(0).lines;
			pg_bb -= PGA(0).len;
			pg_ba += PGA(0).len;
		}
		while (li <= pg_lb && (pg_fill_top() || pg_spill_bottom(text)))
			;
	} else {
		while (end > text && pg_spill_top(end))
			;
		while (end == text && pg_na && li > pg_lb + PGA(0).lines) {
			PGB(pg_nb) = PGA(0);
			pg_na--;
			pg_lb += PGB(pg_nb).lines;
			pg_la -= PGB(pg_nb).lines;
			pg_bb += PGB(pg_nb).len;
			pg_ba -= PGB(pg_nb).len;
			pg_nb++;
		}
		while (li > pg_lb + pg_count(text, end) && (pg_fill_bottom() || pg_spill_top(end)))
			;
	}
	pg_retop();
	p = pg_line(li);
	return p ? p : text;
}

/* Absolute lines a..b all in the window at once, the cursor staying on its
   line; 0 if they do not fit (or the file is not paged and they are not
   all there). */
int pg_hold(int a, int b)
{
	int dl;
	int dc;
	int t;
	char *p;
	if (a > b) {
		t = a;
		a = b;
		b = t;
	}
	t = pg_lines();
	if (a < 1)
		a = 1;
	if (b > t)
		b = t;
	if (a > pg_lb && b <= pg_lb + pg_count(text, end))
		return 1;
	if (!pg_f || pg_lock)
		return 0;
	dl = pg_abs(dot);
	dc = dot - begin_line(dot);
	pg_park = NULL;
	pg_goto(a);
	while (b > pg_lb + pg_count(text, end)) {
		p = pg_line(a);
		if (!pg_fill_bottom() && !(p && pg_spill_top(p)))
			break;
	}
	p = pg_line(dl);
	t = !p;
	if (t) {
		p = pg_line(a);         /* the cursor's line did not fit as well */
		pg_park_l = dl;
		pg_park_c = dc;
		pg_park_a = a;
	}
	if (p) {
		dot = p;
		while (dc-- > 0 && *dot != '\n')
			dot++;
		if (t)
			pg_park = dot;
	}
	pg_retop();
	return a > pg_lb && b <= pg_lb + pg_count(text, end);
}

/* make room for size bytes at p (lines go out below p's line, else above
   it); the new p, NULL if there is no room (locked, or nothing can go) */
char *pg_makeroom(char *p, int size)
{
	int off;
	int len;
	off = p - text;
	while (pg_room() < size && pg_f && !pg_lock) {
		if (pg_spill_bottom(pg_nl(begin_line(text + off))))
			continue;
		len = end - text;
		if (!pg_spill_top(begin_line(text + off)))
			break;
		off -= len - (int)(end - text);
	}
	if (pg_room() < size) {
		status_line_bold("Not enough memory");
		return NULL;
	}
	return text + off;
}

/* ---- searching the stores ---- */

/* pat in chunk c (read into pg_buf after a '\n' -- the end of the line
   before it -- and, if next is not 0, followed by the byte next): the
   first (dir FORWARD) or last match.  Returns the index in pg_buf, -1 if
   none; in the line count before the match (0: the line before the chunk),
   *col its offset in that line (-1: at the line's end). */
static int pg_scan(struct pgchunk *c, int next, const char *pat, int dir, int *line, int *col)
{
	int len;
	int plen;
	int i;
	int s;
	len = c->len;
	pg_buf[0] = '\n';
	if (pg_io(c->slot, pg_buf + 1, len, 0))
		return -1;
	pg_buf[len + 1] = next;
	pg_buf[len + 2] = 0;
	plen = strlen(pat);
	s = -1;
	if (dir == FORWARD) {
		for (i = 0; i <= len; i++)
			if (mycmp(pg_buf + i, pat, plen) == 0) {
				s = i;
				break;
			}
	} else {
		for (i = len + 1 - plen; i >= 0; i--)
			if (mycmp(pg_buf + i, pat, plen) == 0) {
				s = i;
				break;
			}
	}
	if (s < 0)
		return -1;
	if (s == 0) {
		*line = 0;
		*col = -1;
		return 0;
	}
	*line = 1 + pg_count(pg_buf + 1, pg_buf + s);
	for (i = s; i > 1 && pg_buf[i - 1] != '\n'; i--)
		;
	*col = s - i;
	return s;
}

/* pat in the stores, on the window's near side (far 0: below it for
   FORWARD, above it for BACK) or, wrapping around, the far side (far 1:
   from the file's start for FORWARD, from its end for BACK).  The window
   moves to the match; NULL if none (or locked: pg_over). */
char *pg_search(const char *pat, int dir, int far)
{
	int i;
	int n;
	int first;
	int line;
	int col;
	char *p;
	if (!pg_f)
		return NULL;
	line = 0;
	if ((dir == FORWARD) != far) {          /* the chunks after the window */
		first = pg_lb + pg_count(text, end) + 1;
		n = pg_na;
		if (dir == FORWARD) {
			for (i = 0; i < n; i++) {
				if (pg_scan(&PGA(i), 0, pat, dir, &line, &col) >= 0)
					break;
				first += PGA(i).lines;
			}
		} else {
			first = pg_lines() + 1;
			for (i = n - 1; i >= 0; i--) {
				first -= PGA(i).lines;
				if (pg_scan(&PGA(i), 0, pat, dir, &line, &col) >= 0)
					break;
			}
		}
		if (i < 0 || i >= n)
			return NULL;
	} else {                                /* the chunks before it */
		n = pg_nb;
		if (dir == BACK) {
			first = pg_lb + 1;
			for (i = n - 1; i >= 0; i--) {
				first -= PGB(i).lines;
				if (pg_scan(&PGB(i), i == n - 1 ? *text : 0, pat, dir, &line, &col) >= 0)
					break;
			}
		} else {
			first = 1;
			for (i = 0; i < n; i++) {
				if (pg_scan(&PGB(i), i == n - 1 ? *text : 0, pat, dir, &line, &col) >= 0)
					break;
				first += PGB(i).lines;
			}
		}
		if (i < 0 || i >= n)
			return NULL;
	}
	if (pg_lock) {
		pg_over = 1;
		return NULL;
	}
	/* the match: line first + line - 1 (line 0: the end of the line before) */
	p = pg_goto(first + line - 1);
	if (col < 0)
		return end_line(p);
	while (col-- > 0 && *p != '\n')
		p++;
	return p;
}

/* the absolute line of pat's first match after the window, 0 if none */
int pg_find_line(const char *pat)
{
	int i;
	int first;
	int line;
	int col;
	if (!pg_f)
		return 0;
	first = pg_lb + pg_count(text, end) + 1;
	for (i = 0; i < pg_na; i++) {
		if (pg_scan(&PGA(i), 0, pat, FORWARD, &line, &col) >= 0)
			return first + line - 1;
		first += PGA(i).lines;
	}
	return 0;
}

/* ---- the file ---- */

void pg_close(void)
{
	if (pg_f) {
		fclose(pg_f);
		remove(PG_SWAP);
		pg_f = NULL;
	}
}

/* everything for paging, once: the stores, VI.SWAP, the window (text) as
   big as memory allows; without VI.SWAP the window is the whole file */
void pg_init(void)
{
	long cap;
	pg_ch = (struct pgchunk *)xzalloc(PG_MAXCH * sizeof(struct pgchunk));
	pg_used = (unsigned char *)xzalloc(PG_MAXSLOT / 8);
	pg_buf = (char *)xzalloc(PG_SLOT + 3);
	pg_f = fopen(PG_SWAP, "w+b");
#ifdef __UCSD__
	/* MEMAVAIL words, less malloc's room for the stack and some for yanks */
	cap = 2L * __cspi(40) - 600 - PG_RESERVE;
#else
	cap = getenv("VI_PAGECAP") ? atol(getenv("VI_PAGECAP")) : 32000L;
#endif
	if (cap > 30000L)
		cap = 30000L;
	if (cap < 2 * PG_SLOT + PG_ROOM)
		cap = 2 * PG_SLOT + PG_ROOM;
	pg_cap = (int)cap;
	pg_margin = rows;
}

void pg_reset(void)
{
	pg_nb = pg_na = pg_lb = pg_la = pg_top = 0;
	pg_bb = pg_ba = 0;
	memset(pg_used, 0, PG_MAXSLOT / 8);
	pg_lock = pg_over = 0;
}

/* a load that cannot be finished: vi stops, the file untouched */
static void pg_abandon(const char *fn, const char *why)
{
	gracefulExit();
	pg_close();
	printf("vi: %s: %s\n", fn, why);
	exit(1);
}

/* the file into the stores (or, without VI.SWAP, into the window), then the
   window filled from its start; its size (at most 32767) */
int pg_load(const char *fn)
{
	FILE *f;
	int c;
	int len;
	int last;
	int n;
	int k;
	long total;
	pg_reset();
	end = dot = screenbegin = text;
#ifdef __UCSD__
	ucsd_unreserve();
#endif
	f = fopen(fn, "r");
#ifdef __UCSD__
	ucsd_reserve();
#endif
	if (!f) {
		status_line_bold("\"%s\" cannot open", fn);
		return -1;
	}
	len = 0;
	last = 0;
	n = 0;
	total = 0;
	for (;;) {
		c = getc(f);
		if (c != EOF) {
			if (len >= pg_cap - 1)
				pg_abandon(fn, "too big for memory (no " PG_SWAP ")");
			text[len++] = c;
			total++;
			if (c == '\n')
				last = len;
		} else if (len > last) {
			text[len++] = '\n';     /* the last line gets its end */
			total++;
			last = len;
		}
		if (pg_f && (len >= PG_SLOT || (c == EOF && last > 0))) {
			if (last == 0)
				pg_abandon(fn, "a line is longer than 1023 characters");
			if (n >= PG_MAXCH)
				pg_abandon(fn, "too big (more than 256 chunks)");
			k = pg_slot();
			if (k < 0 || pg_io(k, text, last, 1))
				pg_abandon(fn, "cannot write " PG_SWAP);
			pg_ch[n].slot = k;
			pg_ch[n].len = last;
			pg_ch[n].lines = pg_count(text, text + last);
			pg_la += pg_ch[n].lines;
			pg_ba += last;
			n++;
			memmove(text, text + last, len - last);
			len -= last;
			last = 0;
		}
		if (c == EOF)
			break;
	}
	fclose(f);
	if (pg_f) {
		memmove(&pg_ch[PG_MAXCH - n], &pg_ch[0], n * sizeof(struct pgchunk));
		pg_na = n;
		memset(text, 0, pg_cap);
		end = text;
		pg_fix();
	} else
		end = text + len;
	dot = screenbegin = text;
	return total > 32767L ? 32767 : (int)total;
}

/* the whole file: before, window, after; 1 if written, -1 if it cannot be
   opened, 0 if writing failed */
int pg_save(const char *fn)
{
	FILE *f;
	int i;
	int ok;
#ifdef __UCSD__
	ucsd_unreserve();
#endif
	f = fopen(fn, "w");
	if (!f) {
#ifdef __UCSD__
		ucsd_reserve();
#endif
		return -1;
	}
	ok = 1;
	for (i = 0; ok && i < pg_nb; i++)
		ok = !pg_io(PGB(i).slot, pg_buf, PGB(i).len, 0)
		  && (int)fwrite(pg_buf, 1, PGB(i).len, f) == PGB(i).len;
	if (ok && end > text)
		ok = fwrite(text, 1, end - text, f) == (size_t)(end - text);
	for (i = 0; ok && i < pg_na; i++)
		ok = !pg_io(PGA(i).slot, pg_buf, PGA(i).len, 0)
		  && (int)fwrite(pg_buf, 1, PGA(i).len, f) == PGA(i).len;
	if (fclose(f) == EOF)
		ok = 0;
#ifdef __UCSD__
	ucsd_reserve();
#endif
	return ok;
}

#endif
