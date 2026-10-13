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

void *xstrndup(const char *s, size_t n)
{
	char *p;
	size_t l;
	for (l = 0; l < n && s[l]; l++)
		;
	p = xmalloc(l + 1);
	memcpy(p, s, l);
	p[l] = 0;
	return p;
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

/* getopt for the options vi knows: -R, -c CMD */
static int optind = 1;
static char *optarg;
int getopt(int argc, char **argv, const char *opts)
{
	static int sp = 1;
	char *o;
	int c;
	if (sp == 1 && (optind >= argc || argv[optind][0] != '-' || argv[optind][1] == 0))
		return -1;
	c = argv[optind][sp];
	o = strchr(opts, c);
	if (!o || c == ':') {
		if (!argv[optind][++sp]) {
			optind++;
			sp = 1;
		}
		return '?';
	}
	if (o[1] == ':') {
		if (argv[optind][sp + 1])
			optarg = argv[optind] + sp + 1;
		else if (optind + 1 < argc)
			optarg = argv[++optind];
		else
			optarg = "";
		optind++;
		sp = 1;
	} else if (!argv[optind][++sp]) {
		optind++;
		sp = 1;
	}
	return c;
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
	f = fopen(fn, "r");
	n = 0;
	if (f) {
		while ((c = getc(f)) != EOF)
			n++;
		fclose(f);
	}
	ucsd_reserve();
	if (!f)
		return -1;
#if ENABLE_FEATURE_VI_PAGING
	/* paging: only :r takes a whole file into memory, and it must fit */
	return n > 32000L ? 32000 : (int)n;
#else
	if (n > 30000L) {	/* an int holds no more; never edit (and save) part of it */
		gracefulExit();
		printf("vi: %s is too big (more than 30000 characters)\n", fn);
		exit(1);
	}
	return (int)n;
#endif
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
	f = fopen(fn, "r");
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
#if ENABLE_FEATURE_VI_PAGING
	if (pg_f && first == text && last == end - 1) {	/* the whole file */
		cnt = pg_save(fn);
		return cnt < 0 ? -1 : cnt ? last - first + 1 : 0;
	}
#endif
	ucsd_unreserve();
	f = fopen(fn, "w");
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
int hdr_line, hdr_col;
int __istext(char *name);
void __ptitle(char *name, char *title);

/* rd: the header page of the text file fn into pg_buf; else pg_buf into it; 1 if done */
static int hdr_io(const char *fn, int rd)
{
	char fib[80];
	char title[32];
	int n;
	if (!__istext((char *)fn))
		return 0;
	__ptitle((char *)fn, title);
	__cxp0v(3, fib, 0, -1);				/* FINIT(fib, NIL, untyped) */
	__cxp0v(5, fib, title, 1, 0);			/* FOPEN, an old file */
	if (__cspi(34) != 0)
		return 0;
	n = __cxp0i(28, fib, pg_buf, 0, 2, 0, rd, 0, 0) == 2 && __cspi(34) == 0;	/* FBLOCKIO, blocks 0 and 1 */
	__cxp0v(6, fib, 0);				/* FCLOSE */
	return n;
}

void hdr_read(const char *fn)
{
	unsigned char *r;
	r = (unsigned char *)pg_buf + HDR_OFF;
	hdr_line = hdr_col = 0;
	if (hdr_io(fn, 1) && r[0] == 'V' && r[1] == 'I' && r[2] == 1) {
		hdr_col = r[3];
		hdr_line = r[4] + 256 * r[5];
	}
}

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
