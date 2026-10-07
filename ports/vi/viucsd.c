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
	const unsigned char *cp = (const unsigned char *)s + n;
	while (n--)
		if (*--cp == (unsigned char)c)
			return (void *)cp;
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

/* snprintf: format into a buffer big enough for any status line, then cut */
int snprintf(char *buf, size_t n, const char *fmt, ...)
{
	static char tmp[300];
	va_list ap;
	int len;
	va_start(ap, fmt);
	len = vsprintf(tmp, fmt, ap);
	va_end(ap);
	if (n) {
		if ((size_t)len < n) {
			memcpy(buf, tmp, len + 1);
		} else {
			memcpy(buf, tmp, n - 1);
			buf[n - 1] = 0;
		}
	}
	return len;
}

/* getopt for the options vi knows: -R, -c CMD, -H */
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

/* the Linux version is one string: too long for the P-machine (255 bytes) */
void show_help(void)
{
	static const char *help[] = {
		"These features are available:",
		"\tPattern searches with / and ?",
		"\tLast command repeat with \'.\'",
		"\tLine marking with 'x",
		"\tNamed buffers with \"x",
		"\tReadonly if vi is called as \"view\"",
		"\tReadonly with -R command line arg",
		"\tSome colon mode commands with \':\'",
		"\tSettable options with \":set\"",
		0
	};
	int i;
	for (i = 0; help[i]; i++)
		puts(help[i]);
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
	SYSCOM->expansion[1] = PX_KEYS;	// Page Up ... Delete: one code each (psys.h)
	erase_char = SYSCOM->crtinfo.chardel;
	ticsPerChar = 1;
	return 0;
}

void cookmode(void)
{
	SYSCOM->expansion[1] = 0;	// the keys as the L2 editor wants them again
}

/* true if a key comes within tics/100 seconds: the system clock (1/60 s)
   when there is one, else a fixed number of polls */
int awaitInput(int tics)
{
	unsigned start;
	long polls;
	fflush(stdout);
	if (kbhit())
		return 1;
	if (tics <= 0)
		return 0;
	if (SYSCOM->miscinfo & MI_HASCLOCK) {
		start = SYSCOM->lowtime;
		while ((unsigned)(SYSCOM->lowtime - start) < (unsigned)(tics * 6 / 10 + 1))
			if (kbhit())
				return 1;
		return 0;
	}
	for (polls = tics * 20L; polls > 0; polls--)
		if (kbhit())
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
	fflush(stdout);
	c = getch();
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
		printf("vi: %s is too big (%ld characters, at most 30000)\n", fn, n);
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
		file_modified++;
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
	fflush(stdout);
	gotoxy(col, row);
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

#endif
