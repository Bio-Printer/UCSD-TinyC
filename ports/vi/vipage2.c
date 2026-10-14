/* vipage2.c -- vi: the rest of the window into big files: jumps, ranges, searching the stores,
   loading and saving (vipage.c has the window's moves; split to compile in less memory) */
#include "vi.h"

static void pg_abandon(const char *fn, const char *why);
static int pg_scan(struct pgchunk *c, int next, const char *pat, int dir, int *line, int *col);

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
	if (li > pg_lines())
		pg_complete();
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
	if (b > pg_lines())
		pg_complete();
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
	pg_complete();
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

/* the absolute line of pat's first match after the window, else (wrapping
   around) from the file's start up to the window; 0 if none.  The window
   does not move. */
int pg_find_line(const char *pat)
{
	int i;
	int first;
	int line;
	int col;
	if (!pg_f)
		return 0;
	pg_complete();
	first = pg_lb + pg_count(text, end) + 1;
	for (i = 0; i < pg_na; i++) {
		if (pg_scan(&PGA(i), 0, pat, FORWARD, &line, &col) >= 0)
			return first + line - 1;
		first += PGA(i).lines;
	}
	first = 1;
	for (i = 0; i < pg_nb; i++) {
		if (pg_scan(&PGB(i), i == pg_nb - 1 ? *text : 0, pat, FORWARD, &line, &col) >= 0)
			return first + line - 1;
		first += PGB(i).lines;
	}
	return 0;
}

/* ---- the file ---- */

/* the file being read is closed: the page buffer and file information block it held
   (malloc gave it the blocks ucsd_reserve() had set aside for saving) go back to the
   reserve, or the save finds none and its fopen takes the memory the stack needs */
static void pg_rclose(void)
{
	fclose(pg_rf);
	pg_rf = NULL;
#ifdef __UCSD__
	ucsd_reserve();
#endif
}

void pg_close(void)
{
	if (pg_rf) {
		pg_rclose();
	}
	if (pg_f) {
		fclose(pg_f);
		remove(PG_SWAP);
		pg_f = NULL;
	}
}

/* everything for paging, once: the stores, VI.SWAP, the window (text) as
   big as memory allows; without VI.SWAP the window is the whole file */
void pg_init(const char *fn)
{
	long cap;
	pg_used = (unsigned char *)xzalloc(PG_MAXSLOT / 8);
	pg_buf = (char *)xzalloc(PG_SLOT + 3);
	pg_maxch = PG_MAXCH;
	pg_slots = PG_MAXSLOT;
#ifdef __UCSD__
	/* the file: the place it was left (its header page) and its size.  VI.SWAP holds the file and
	   25 KB for what is typed in, and the chunk table no more than its slots (every chunk has one;
	   the memory goes to the window: in Z80 mode it is small) */
	hdr_read(fn);
	if (hdr_blocks / 2 + 25 < pg_slots)
		pg_slots = hdr_blocks / 2 + 25;
	if (pg_slots < pg_maxch)
		pg_maxch = pg_slots;
#endif
	pg_ch = (struct pgchunk *)xzalloc(pg_maxch * sizeof(struct pgchunk));
	pg_f = fopen(PG_SWAP, "w+b");
#ifdef __UCSD__
	/* A new file takes the largest free area of the volume: VI.SWAP's last slot is written, it is
	   closed (the rest of the area is free again, so a :w on the same volume finds room) and
	   opened again to read and write */
	if (pg_f) {
		fseek(pg_f, (long)(pg_slots - 1) * PG_SLOT, SEEK_SET);
		fwrite(pg_buf, 1, PG_SLOT, pg_f);
		fclose(pg_f);
		pg_f = fopen(PG_SWAP, "r+b");
	}
	/* MEMAVAIL words, less malloc's room for the stack and some for yanks */
	cap = 2L * __cspi(40) - 600 - PG_RESERVE;
#else
	cap = getenv("VI_PAGECAP") ? atol(getenv("VI_PAGECAP")) : 32000L;
#endif
	if (cap > 30000L)
		cap = 30000L;
	if (cap < PG_SLOT + 2 * PG_ROOM)
		cap = PG_SLOT + 2 * PG_ROOM;
	pg_cap = (int)cap;
	pg_margin = rows;
}

void pg_reset(void)
{
	if (pg_rf) {
		pg_rclose();
	}
	pg_carry = pg_bad = pg_cr = 0;
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

/* Load: without VI.SWAP the file goes into the window; with it, only as far
   as it is needed.  pg_load opens the file and pg_fix() reads the first
   chunks; the rest is read a chunk at a time onto the bottom of the after
   stack -- when the window needs the lines, when vi waits for a key
   (pg_idle), or all of it at once for what needs the whole file (G, a
   search, a : command, saving: pg_complete).  pg_lines() counts what is
   loaded, pg_total() all of it. */

/* the next chunk of the file onto the bottom of the after stack; 0 when
   there is no more to read (or it cannot be: pg_bad) */
int pg_more(void)
{
	int n;
	int len;
	int last;
	int k;
	if (!pg_rf)
		return 0;
	n = fread(pg_buf + pg_carry, 1, PG_SLOT - pg_carry, pg_rf);
	len = pg_carry + n;
	for (last = pg_carry; last < len; last++)
		if (pg_buf[last] == '\r') {    /* (a text file's come as '\n': a data file's, as in a listing) */
			pg_buf[last] = '\n';
			pg_cr = 1;
		}
	last = len;
	k = n < PG_SLOT - pg_carry;             /* the end of the file */
	if (k) {
		pg_rclose();
		last_file_modified = -1;        /* the status line's total */
		while (len > 0 && pg_buf[len - 1] == 0)
			len--;                  /* (a data file's last block is filled out with NULs) */
		if (len > 0 && pg_buf[len - 1] != '\n')
			pg_buf[len++] = '\n';         /* the last line gets its end */
		last = len;
	} else
		while (last > 0 && pg_buf[last - 1] != '\n')
			last--;
	/* no whole line in a full chunk (a line over 1023 characters), the chunk
	   table or VI.SWAP full or not written: the file stays loaded as far as it is */
	if (last == 0 && !k || last > 0 && (pg_nb + pg_na >= pg_maxch || (n = pg_slot()) < 0
	 || pg_io(n, pg_buf, last, 1) && (pg_unslot(n), 1))) {
		if (pg_nb + pg_na == 0 && end == text)
			pg_abandon(current_filename, "cannot be loaded");     /* nothing shown yet: vi stops */
		if (pg_rf) {
			pg_rclose();
		}
		pg_bad = 1;
		status_line_bold("%s: not loaded to its end", current_filename);
		return 0;
	}
	if (last > 0) {
		memmove(&pg_ch[pg_maxch - pg_na - 1], &pg_ch[pg_maxch - pg_na], pg_na * sizeof(struct pgchunk));
		pg_na++;
		pg_ch[pg_maxch - 1].slot = n;
		pg_ch[pg_maxch - 1].len = last;
		pg_ch[pg_maxch - 1].lines = pg_count(pg_buf, pg_buf + last);
		pg_la += pg_ch[pg_maxch - 1].lines;
		pg_ba += last;
	}
	if (pg_rf) {
		memmove(pg_buf, pg_buf + last, len - last);
		pg_carry = len - last;
	}
	return 1;
}

/* all of the file in */
void pg_complete(void)
{
	while (pg_rf && pg_more())
		;
}

/* the lines of the whole file */
int pg_total(void)
{
	pg_complete();
	return pg_lines();
}

/* while vi waits for a key: the file goes on loading, a chunk at a time, until
   one comes (the status line shows the total at the next key) */
void pg_idle(void)
{
	if (!pg_rf)
		return;
#ifndef __UCSD__
	if (getenv("VI_NOIDLE"))		// (tests: the file stays unloaded until a command needs it)
		return;
#endif
	while (pg_rf && !awaitInput(0))
		pg_more();
	if (!pg_rf) {			// all in: the status line's total (counted again), and its L (still loading) gone
		last_file_modified = -1;
		*status_buffer = 0;		// (a message there gives way to the status line)
		refresh();
	}
}

/* the file; -1 if it cannot be opened, else a number >= 0 */
int pg_load(const char *fn)
{
	FILE *f;
	int len;
	pg_reset();
	end = dot = screenbegin = text;
#ifdef __UCSD__
	ucsd_unreserve();
#endif
	f = fopen(fn, "r");
#ifdef __UCSD__
	ucsd_reserve();
#endif
	if (!f)
		return -1;
	if (!pg_f) {                            /* no VI.SWAP: the whole file in the window */
		len = fread(text, 1, pg_cap - 1, f);
		if (len >= pg_cap - 1 && fgetc(f) != EOF)
			pg_abandon(fn, "too big for memory (no " PG_SWAP ")");
		fclose(f);
#ifdef __UCSD__
		ucsd_reserve();
#endif
		if (len > 0 && text[len - 1] != '\n')
			text[len++] = '\n';             /* the last line gets its end */
		end = text + len;
		return len;
	}
	pg_rf = f;
	pg_carry = 0;
	pg_fix();                               /* the window, and what it needs in */
	return 1;
}

/* len bytes of b to f, 1 if written; a file read with CR line ends gets them back */
static int pg_put(char *b, int len, FILE *f)
{
	if (!pg_cr)
		return (int)fwrite(b, 1, len, f) == len;
	for (; len > 0; len--, b++)
		if (putc(*b == '\n' ? '\r' : *b, f) == EOF)
			return 0;
	return 1;
}

/* the whole file: before, window, after; 1 if written, -1 if it cannot be
   opened, 0 if writing failed */
int pg_save(const char *fn)
{
	FILE *f;
	int i;
	int ok;
	pg_complete();
	if (pg_bad) {
		status_line_bold("Not written: the file was not loaded to its end");
		return 0;
	}
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
		ok = !pg_io(PGB(i).slot, pg_buf, PGB(i).len, 0) && pg_put(pg_buf, PGB(i).len, f);
	if (ok && end > text)
		ok = pg_put(text, end - text, f);
	for (i = 0; ok && i < pg_na; i++)
		ok = !pg_io(PGA(i).slot, pg_buf, PGA(i).len, 0) && pg_put(pg_buf, PGA(i).len, f);
	if (fclose(f) == EOF)
		ok = 0;
#ifdef __UCSD__
	ucsd_reserve();
#endif
	return ok;
}

