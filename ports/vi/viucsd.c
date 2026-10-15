/* viucsd.c -- vi on UCSD Pascal II.0 with Tiny-C: the routines that
   talk to the terminal and the files, and the library functions Tiny-C
   does not have. */
#include "vi.h"
#ifdef __UCSD__

/* ---- library functions Tiny-C does not have ---- */


void *xmalloc(size_t size)
{
	void *ptr = malloc(size);
	if (ptr) return ptr;
	gracefulExit();
	printf("vi: out of memory (%u bytes)\n", size);
	exit(65);
	return NULL;
}

void *xzalloc(size_t size)
{
	return memset(xmalloc(size), 0, size);
}

void *xstrdup(const char *s)
{
	return strcpy(xmalloc(strlen(s) + 1), s);
}

void *xrealloc(void *old, size_t size)
{
	void *ptr = realloc(old, size);
	if (ptr) return ptr;
	gracefulExit();
	printf("vi: out of memory (%u bytes)\n", size);
	exit(68);
	return NULL;
}


char* last_char_is(const char *s, int c)
{
	if (s && *s) {
		s += strlen(s) - 1;
		if ((unsigned char)*s == c)
			return (char*)s;
	}
	return NULL;
}

int bb_putchar(int ch)
{
	return putc(ch, stdout);
}

char *strchrnul(const char *s, int c)
{
	while (*s && *s != (char)c)
		s++;
	return (char *)s;
}

void *memrchr(const void *s, int c, size_t n)
{
	unsigned char *p = (unsigned char *)s + n;
	int d;
	int k;
	while (n > 0) {			// the P-System's SCAN, backwards
		d = n > 32767 ? 32767 : n;
		k = __cspi(11, -d, 0, c & 255, p - 1, 0, 0);
		if (k > -d)
			return p - 1 + k;
		p -= d;
		n -= d;
	}
	return NULL;
}

int strncasecmp(const char *a, const char *b, size_t n)
{
	int d;
	for (; n; n--, a++, b++) {
		d = tolower((unsigned char)*a) - tolower((unsigned char)*b);
		if (d || !*a)
			return d;
	}
	return 0;
}

char *strerror(int e)
{
	return "I/O error";
}

/* The library's printf family is 2.8 KB of resident code; vi needs %s %d %i %u %ld %c %%
   and nothing else (no width): this is that, into a buffer of n bytes
   (cut, always ended); the length it would have had. */
int vi_vsnprintf(char *buf, size_t n, const char *fmt, va_list ap)
{
	size_t len;
	char num[12];
	const char *s;
	unsigned long u;
	int lng;
	int c;
	int k;

	len = 0;
	for (; *fmt; fmt++) {
		s = fmt;
		k = 1;
		if (*fmt == '%') {
			fmt++;
			c = *fmt;
			lng = c == 'l';
			if (lng)
				c = *++fmt;
			if (c == 's') {
				s = va_arg(ap, const char *);
				if (!s)
					s = "(null)";
				k = strlen(s);
			} else if (c == 'd' || c == 'i' || c == 'u') {
				if (lng)
					u = va_arg(ap, unsigned long);
				else if (c == 'u')
					u = va_arg(ap, unsigned);
				else
					u = (long)va_arg(ap, int);
				if (c != 'u' && (long)u < 0) {
					if (len + 1 < n)
						buf[len] = '-';
					len++;
					u = -u;
				}
				s = num + 11;
				k = 0;
				do {
					*--s = '0' + u % 10;
					u = u / 10;
					k++;
				} while (u);
			} else if (c == 'c') {
				num[0] = va_arg(ap, int);
				s = num;
			}		/* (%%: the % itself, s = fmt) */
		}
		while (k-- > 0) {
			if (len + 1 < n)
				buf[len] = *s;
			len++;
			s++;
		}
	}
	if (n)
		buf[len + 1 < n ? len : n - 1] = 0;
	return len;
}

int snprintf(char *buf, size_t n, const char *fmt, ...)
{
	va_list ap;
	int len;
	va_start(ap, fmt);
	len = vi_vsnprintf(buf, n, fmt, ap);
	va_end(ap);
	return len;
}

int vi_printf(const char *fmt, ...)
{
	char tmp[100];
	va_list ap;
	va_start(ap, fmt);
	vi_vsnprintf(tmp, sizeof(tmp), fmt, ap);
	va_end(ap);
	return fputs(tmp, stdout);
}

/* ---- the terminal and the files ---- */
/* The memory a file needs (stdio's 1 KB page and the 80-byte file
   information block) is set aside at the start, so that when the text has
   taken all the rest, the file can still be saved.  Tiny-C's malloc hands
   out free blocks whole (best fit), so these are the blocks fopen gets. */
static char *ucsd_res[2];

void ucsd_reserve(void)
{
	if (!ucsd_res[0])
		ucsd_res[0] = malloc(1024);
	if (!ucsd_res[1])
		ucsd_res[1] = malloc(80);
}

void ucsd_unreserve(void)
{
	free(ucsd_res[1]);
	free(ucsd_res[0]);
	ucsd_res[0] = ucsd_res[1] = NULL;
}

/* ---- a file in a sub-volume ----
   The DIRS segment that finds a path and the 2 KB directory it reads take 5 KB: in Z80 mode
   more than the window leaves.  So the file being edited is found once, before the window
   takes the memory (its place: ucsd_place), and then read and saved by that place, its
   directory read into 2 KB of the window (pg_load, pg_save): only the library's small SVAT
   segment comes in.  Any other file in a sub-volume (:r, :w NAME) is opened only when DIRS and
   its directory still fit. */
int __istext(char *name);
struct svat *ucsd_at;			/* (allocated for the first file in a sub-volume) */
static char *ucsd_atfn;			/* the file whose place it is */

/* is fn the file whose place ucsd_at is */
int ucsd_isat(const char *fn)
{
	return ucsd_atfn && !strncasecmp(fn, ucsd_atfn, 64);
}

/* fopen: the file being edited by its place, to read (its directory read a block at a time),
   or to write when dir is given (2 KB for its directory when it is closed: with __svat =
   ucsd_at, pg_save); another file in a sub-volume only if DIRS still fits (else NULL) */
FILE *ucsd_fopen(const char *fn, char *mode, char *dir)
{
	FILE *f;
	if (ucsd_isat(fn) && (dir || *mode == 'r')) {
		ucsd_at->dir = (int *)dir;
		__svat = ucsd_at;
	} else if (__svname((char *)fn) && __cspi(40) < SV_NEED)
		return NULL;
	f = fopen((char *)fn, mode);
	__svat = NULL;
	return f;
}

/* The console is always "raw" for vi: getch() reads keys unechoed, one at
   a time, straight from UNITREAD. */
int rawmode(void)
{
	ucsd_reserve();
	// Page Up ... Delete: one code each, a block cursor, the scroll codes and the count of
	// keys waiting in SYSCOM->expansion[2] (psys.h PX_KEYS_AHEAD; emulator 2.04 answers
	// PX_AHEAD_OK at its next P-code instruction)
	SYSCOM->expansion[1] = PX_KEYS_AHEAD;
	if (SYSCOM->expansion[1] != PX_AHEAD_OK) {
		fputs("vi needs emulator 2.04 or later\n", stderr);
		exit(1);
	}
	scroll_ok = 1;
	erase_char = SYSCOM->crtinfo.chardel;
	ticsPerChar = 1;
	return 0;
}

void cookmode(void)
{
	SYSCOM->expansion[1] = 0;	// the keys as the L2 editor wants them again, the cursor a line
}

/* true if a key comes within tics/100 seconds: the system clock (1/60 s)
   when there is one, else a fixed number of polls */
static int pushed = -1;	/* a key read ahead while adding up wheel notches */

int awaitInput(int tics)
{
	unsigned start;
	long polls;
	fflush(stdout);
	if (pushed >= 0 || SYSCOM->expansion[2] != 0)
		return 1;
	if (tics <= 0)
		return 0;
	if (SYSCOM->miscinfo & MI_HASCLOCK) {
		start = SYSCOM->lowtime;
		while ((unsigned)(SYSCOM->lowtime - start) < (unsigned)(tics * 6 / 10 + 1))
			if (SYSCOM->expansion[2] != 0)
				return 1;
		return 0;
	}
	for (polls = tics * 20L; polls > 0; polls--)
		if (SYSCOM->expansion[2] != 0)
			return 1;
	return 0;
}

/* one key; the P-System's cursor keys (SYSCOM's CRTINFO: the emulator
   sends ^T ^R ^Q ^U for the arrows) become vi's arrow keys */
char readit(void)
{
	int c;
	struct crtinforec *ci;
	if (chars_to_parse > 0) {
		c = readbuffer[0];
		chars_to_parse--;
		memmove(readbuffer, readbuffer + 1, chars_to_parse);
		return c;
	}
	if (pushed >= 0) {
		c = pushed;
		pushed = -1;
	} else {
		PG_IDLE();
		fflush(stdout);
		c = getch();
		/* the mouse wheel (emulator 2.01): 3 lines a notch, in command mode
		   only (typing, or a : or / line, ignores it).  The notches that are
		   there (or come within about 70 ms) are added up and made one scroll,
		   so the screen is painted once.  Up and down cancel. */
		for (;;) {
			if (c == KEY_WHEELUP || c == KEY_WHEELDN) {
				if (cmd_mode == CMODE_COMMAND)
					wheel_n += c == KEY_WHEELUP ? 3 : -3;
				if (!wheel_n || awaitInput(6)) {
					c = getch();
					continue;
				}
			} else if (wheel_n)
				pushed = c;	// (a key behind the notches)
			break;
		}
		if (wheel_n) {
			c = wheel_n > 0 ? 25 : 5;
			if (wheel_n < 0)
				wheel_n = -wheel_n;
			return c;
		}
	}
	ci = &SYSCOM->crtinfo;
	if (c == ci->up)
		return VI_K_UP;
	if (c == ci->down)
		return VI_K_DOWN;
	if (c == ci->left)
		return VI_K_LEFT;
	if (c == ci->right)
		return VI_K_RIGHT;
	// KEY_HOME .. KEY_PGDN (psys.h) are vi's own VI_K_HOME .. VI_K_PAGEDOWN
	return c;
}

/* the size of a file in bytes as vi will hold it (text files: the lines
   without the UCSD page structure), -1 if it does not exist */
int file_size(const char *fn)
{
	FILE *f;
	long n;
	int c;
	if (!fn || !fn[0])
		return -1;
	ucsd_unreserve();
	f = ucsd_fopen(fn, "r", NULL);
	n = 0;
	if (f) {
		while ((c = getc(f)) != EOF)
			n++;
		fclose(f);
	}
	ucsd_reserve();
	if (!f)
		return -1;
	/* paging: only :r takes a whole file into memory, and it must fit */
	return n > 32000L ? 32000 : (int)n;
}

int file_insert(const char *fn, char *p, int update_ro_status)
{
	FILE *f;
	int cnt, size, c;

	if (p < text || p > end) {
		status_line_bold("Trying to insert file outside of memory");
		return -1;
	}
	size = file_size(fn);
	if (size < 0) {
		status_line_bold("\"%s\" cannot open", fn);
		return -1;
	}
	p = text_hole_make(p, size);
	if (!p)
		return -1;
	ucsd_unreserve();
	f = ucsd_fopen(fn, "r", NULL);
	cnt = 0;
	if (f) {
		for (; cnt < size && (c = getc(f)) != EOF; cnt++)
			p[cnt] = c;
		fclose(f);
	}
	ucsd_reserve();
	if (cnt < size) {
		text_hole_delete(p + cnt, p + size - 1);
		status_line_bold("cannot read all of file \"%s\"", fn);
	} else
		(file_modified++, scr_gen++);
	return cnt;
}

int file_write(char *fn, char *first, char *last)
{
	FILE *f;
	int cnt;
	char *p;

	if (fn == 0) {
		status_line_bold("No current filename");
		return -2;
	}
	if (pg_f && first == text && last == end - 1) {	/* the whole file */
		cnt = pg_save(fn);
		return cnt < 0 ? -1 : cnt ? last - first + 1 : 0;
	}
	ucsd_unreserve();
	f = ucsd_fopen(fn, "w", NULL);
	if (!f) {
		ucsd_reserve();
		return -1;
	}
	cnt = last - first + 1;
	for (p = first; p <= last; p++)
		if (putc(*p, f) == EOF)
			break;
	if (fclose(f) == EOF || p <= last)
		cnt = 0;
	ucsd_reserve();
	return cnt;
}

void place_cursor(int row, int col, int optimize)
{
	if (row < 0) row = 0;
	if (row >= rows) row = rows - 1;
	if (col < 0) col = 0;
	if (col >= columns) col = columns - 1;
	// the OS's FGOTOXY (MYGOTOXY.TEXT, BIGGY 1.11 or later) is CHR(1), X+32, Y+32:
	// into the output buffer with the text, not a flush and a call for each move
	putc(1, stdout);
	putc(col + 32, stdout);
	putc(row + 32, stdout);
}

void clear_to_eol(void)
{
	putc(SYSCOM->crtctrl.eraseeol, stdout);
}

void clear_to_eos(void)
{
	putc(SYSCOM->crtctrl.eraseeos, stdout);
	*displayed_buffer=0;   //status line was also cleared
}


/* ---- the place where the file was left ----
   A text file starts with a 1 KB header page (blocks 0 and 1) that the Pascal Editor owns: its
   markers, environment and dates, then 892 bytes it never uses.  Vi keeps 6 bytes there, at
   HDR_OFF, in binary:  'V' 'I' 1 column line-low line-high.  (Small on purpose: in Z80 mode vi's
   window is the memory its code leaves.) */
#define HDR_OFF 1000
int hdr_line, hdr_col, hdr_blocks;

/* rd: the header page of the text file fn into pg_buf; else pg_buf into it; 1 if done.  The
   file is opened to find its blocks (and their count, the chunk table's), then read and written
   by them (in a sub-volume: opened by its place) */
static int hdr_io(const char *fn, int rd)
{
	FILE *f;
	int *r;
	int u, b, n;
	f = ucsd_fopen(fn, "r", NULL);
	if (!f)
		return 0;
	r = (int *)f->fib;
	if (f->flags & __F_SUB) {			/* (its record: dirs.c) */
		u = r[0];
		b = r[1];
		n = r[3];
	} else {					/* (FUNIT, FHEADER.DFIRSTBLK, DLASTBLK) */
		u = r[7];
		b = r[16];
		n = r[17] - b;
	}
	fclose(f);
	if (rd)
		hdr_blocks = n;
	if (!__istext((char *)fn) || n < 2)
		return 0;
	if (rd)
		__cspv(5, u, pg_buf, 0, 1024, b, 0);	/* UNITREAD */
	else
		__cspv(6, u, pg_buf, 0, 1024, b, 0);	/* UNITWRITE */
	return __cspi(34) == 0;
}

#pragma segment VISTART
void hdr_read(const char *fn)
{
	unsigned char *r;
	int n;
	r = (unsigned char *)pg_buf + HDR_OFF;
	hdr_line = hdr_col = hdr_blocks = 0;
	ucsd_unreserve();			/* (its open takes the reserve's blocks, and gives them back) */
	n = hdr_io(fn, 1);
	ucsd_reserve();
	if (n && r[0] == 'V' && r[1] == 'I' && r[2] == 1) {
		hdr_col = r[3];
		hdr_line = r[4] + 256 * r[5];
	}
}

/* started from X(ecute): no arguments, so ask */
char *ucsd_askname(void)
{
	static char name[40];
	int n;
	printf("Edit what file? ");
	fflush(stdout);
	if (!fgets(name, sizeof(name), stdin))
		name[0] = 0;
	n = strlen(name);
	while (n > 0 && (name[n - 1] == '\n' || name[n - 1] == ' '))
		name[--n] = 0;
	return name;
}

/* fn's place, if it is in a sub-volume, unless it is the one found before (DIRS and a 2 KB
   directory: at the start, or for :e when pg_load has seen that they fit) */
void ucsd_place(const char *fn)
{
	if (!__svname((char *)fn) || ucsd_isat(fn))
		return;
	free(ucsd_atfn);
	ucsd_atfn = NULL;
	if (!ucsd_at)
		ucsd_at = xmalloc(sizeof(struct svat));
	if (__svplace((char *)fn, ucsd_at))		/* (else: no such sub-volume, fopen says so) */
		ucsd_atfn = xstrdup(fn);
}
#pragma segment MAIN

void hdr_write(const char *fn, int line, int col)
{
	unsigned char *r;
	r = (unsigned char *)pg_buf + HDR_OFF;
	if (!hdr_io(fn, 1))
		return;
	r[0] = 'V';
	r[1] = 'I';
	r[2] = 1;
	r[3] = col > 255 ? 255 : col;
	r[4] = line & 255;
	r[5] = line >> 8;
	hdr_io(fn, 0);
}
#endif
