/* vimain.c -- vi: start, main loop, keys, files, the terminal (Linux) */
#include "vi.h"

struct globals G;

#if ENABLE_FEATURE_VI_WIN_RESIZE
/* Report cursor positon */
static const char CtextAreaQuery[] ALIGN1 = "\033[r\033[999;999H\033[6n";
#endif

#ifndef __UCSD__
static int safe_poll(struct pollfd *ufds, nfds_t nfds, int timeout);
#endif
#ifndef __UCSD__
static ssize_t safe_read(int fd, void *buf, size_t count);
#endif
#ifndef __UCSD__
static ssize_t safe_write(int fd, const void *buf, size_t count);
#endif
#ifndef __UCSD__
static ssize_t full_write(int fd, const void *buf, size_t len);
#endif
static void clampScreenSize(void);
static const char *snchr(const char *s, int c, size_t n);
#ifndef __UCSD__
static ssize_t readResponse(char *buf, size_t bufSize, int endByte);
#endif
static void queueAnyInput(void);
static void getScreenSize(void);
static void edit_file(char *fn);
static void winch_sig(int sig ATTRIBUTE_UNUSED);
static void quit_sig(int sig);
static void cont_sig(int sig ATTRIBUTE_UNUSED);
static void suspend_sig(int sig ATTRIBUTE_UNUSED);

#if !defined(__UCSD__) && defined(STANDALONE)
void *xmalloc(size_t size)
{
	void *ptr = malloc(size);
	if (ptr) return ptr;
	perror("malloc");
	exit(65);
}

void *xzalloc(size_t size)
{
	return memset(xmalloc(size), 0, size);
}

void *xstrdup(const char *s)
{
	void *ptr = strdup(s);
	if (ptr) return ptr;
	perror("strdup");
	exit(66);
}

void *xstrndup(const char *s, size_t n)
{
	void *ptr = strndup(s, n);
	if (ptr) return ptr;
	perror("strndup");
	exit(67);
}

void *xrealloc(void *old, size_t size)
{
	void *ptr = realloc(old, size);
	if (ptr) return ptr;
	perror("realloc");
	exit(68);
}

/* Find out if the last character of a string matches the one given.
 * Don't underrun the buffer if the string length is 0.
 */
char* last_char_is(const char *s, int c)
{
	if (s && *s) {
		size_t sz = strlen(s) - 1;
		s += sz;
		if ( (unsigned char)*s == c)
			return (char*)s;
	}
	return NULL;
}

int bb_putchar(int ch)
{
	return putc(ch, stdout);
}

/* Wrapper which restarts poll on EINTR or ENOMEM.
 * On other errors does perror("poll") and returns.
 * Warning! May take longer than timeout_ms to return! */
int safe_poll(struct pollfd *ufds, nfds_t nfds, int timeout)
{
	while (1) {
		int n = poll(ufds, nfds, timeout);
		if (n >= 0)
			return n;
		/* Make sure we inch towards completion */
		if (timeout > 0)
			timeout--;
		/* E.g. strace causes poll to return this */
		if (errno == EINTR)
			continue;
		/* Kernel is very low on memory. Retry. */
		/* I doubt many callers would handle this correctly! */
		if (errno == ENOMEM)
			continue;
		bb_perror_msg("poll");
		return n;
	}
}

ssize_t safe_read(int fd, void *buf, size_t count)
{
	ssize_t n;

	do {
		n = read(fd, buf, count);
	} while (n < 0 && errno == EINTR);

	return n;
}

ssize_t safe_write(int fd, const void *buf, size_t count)
{
	ssize_t n;

	do {
		n = write(fd, buf, count);
	} while (n < 0 && errno == EINTR);

	return n;
}

ssize_t full_write(int fd, const void *buf, size_t len)
{
	ssize_t cc;
	ssize_t total;

	total = 0;

	while (len) {
		cc = safe_write(fd, buf, len);

		if (cc < 0) {
			if (total) {
				/* we already wrote some! */
				/* user can do another write to know the error code */
				return total;
			}
			return cc;	/* write() returns -1 on failure. */
		}

		total += cc;
		buf = ((const char *)buf) + cc;
		len -= cc;
	}

	return total;
}
#endif

void write1(const char *out)
{
	fputs(out, stdout);
}

void clear_screen(void)
{
	place_cursor(0, 0, FALSE);	// put cursor in correct place
	clear_to_eos();		// tell terminal to erase display
}

void gracefulExit(void)
{
	cookmode();
	place_cursor(rows-1, 0, FALSE);	// go to bottom of screen
	clear_to_eol();		// Erase to end of line
	fflush(stdout);
}

void clampScreenSize(void)
{
	if (rows < 2)
		rows = 2;
	else if (rows > MAX_SCR_ROWS)
		rows = MAX_SCR_ROWS;
	if (columns < 2)
		columns = 2;
	else if (columns > MAX_SCR_COLS)
		columns = MAX_SCR_COLS;
}

#if ENABLE_FEATURE_VI_WIN_RESIZE
static const char *snchr(const char *s, int c, size_t n)
{
	while (n--)
		if (*s++ == c) return --s;
	return NULL;
}

static ssize_t
  readResponse(char *buf, size_t bufSize, int endByte)
//read response from STDIN into buf until timeout or endByte received
{
	size_t cursor = 0;
	while (cursor < bufSize) {
		if (!awaitInput(ticsPerChar+9))
			return -ETIME;
		int r = safe_read(STDIN_FILENO, buf + cursor, bufSize - cursor);
		if (r <= 0)
			return r < 0 ? r : -EIO;
		if (snchr(buf+cursor, endByte, r))
			return cursor + r;
		cursor += r;
	}
	return -E2BIG;
}

static void queueAnyInput(void)
//add any pending user input to readbuffer
{
	if (awaitInput(0)) {
	  char *s = readbuffer + chars_to_parse;
	  int r = safe_read(STDIN_FILENO, s, readbuffer+sizeof(readbuffer) - s);
	  if (r > 0)
	    chars_to_parse += r;
	}
}

void getScreenSize(void)
// assigns width and height to best guess as to actual screen size
// LINES and COLUMNS env vars take priority
// If either missing, query the terminal using VT100 escape codes
// 		If that fails, fall back to rows/cols info in the termios struct
// rows and columns retain their previous values if all methods fail
{
	struct winsize win = { 0, 0, 0, 0 };
	const char *lines = getenv("LINES");
	const char *cols = getenv("COLUMNS");
	if (!lines || !cols) {  //if either missing in the environment
		queueAnyInput();
		if (!awaitInput(0)) {  //can't query term if there's pending user input
			write1(CtextAreaQuery);
			char buf[16];
			int rspLen = readResponse(buf, sizeof(buf)-1, 'R');
			if (rspLen > 5 && buf[0]==27 && buf[1]=='[') {
				buf[rspLen]=0; //terminate response string
				char *term;
				unsigned long ul = strtoul(buf+2, &term, 10);
				if (*term == ';') {
					win.ws_row = ul;
					ul = strtoul(term+1, &term, 10);
					if (*term == 'R') {
						win.ws_col = ul;
					}
				}
			}
		}
		if (!win.ws_row || !win.ws_col)  //try termios if textAreaQuery failed
			ioctl(STDIN_FILENO, TIOCGWINSZ, &win);
	}
	//environment variables trump all
	if (lines)
		rows = atoi(lines);
	else if (win.ws_row)
		rows = win.ws_row;
	if (cols)
		columns = atoi(cols);
	else if (win.ws_col)
		columns = win.ws_col;
}
#endif


void createScreen(void)
{
#if ENABLE_FEATURE_VI_WIN_RESIZE
	getScreenSize();
	clampScreenSize();
#endif
	new_screen(rows, columns);	// get memory for virtual screen
}


int vi_main(int argc, char **argv) MAIN_EXTERNALLY_VISIBLE;
int vi_main(int argc, char **argv)
{
	int c;

	INIT_G();
	rows = 24;
	columns = 80;
#ifdef __UCSD__
	if (SYSCOM->crtinfo.height > 0)
		rows = SYSCOM->crtinfo.height;
	if (SYSCOM->crtinfo.width > 0)
		columns = SYSCOM->crtinfo.width;
#endif
#if !ENABLE_FEATURE_VI_WIN_RESIZE
	{  //try to get terminal dimensions from environment
		char *txt = getenv("LINES");
		if (txt)
			rows = atoi(txt);
		txt = getenv("COLUMNS");
		if (txt)
			columns = atoi(txt);
		clampScreenSize();
	}
#endif

#if ENABLE_FEATURE_VI_USE_SIGNALS
	my_pid = getpid();
#endif
#ifdef NO_SUCH_APPLET_YET
	/* If we aren't "vi", we are "view" */
	if (ENABLE_FEATURE_VI_READONLY && applet_name[2]) {
		SET_READONLY_MODE(readonly_mode);
	}
#endif

	vi_setops = VI_AUTOINDENT | VI_SHOWMATCH | VI_IGNORECASE;
#if ENABLE_FEATURE_VI_DOT_CMD || ENABLE_FEATURE_VI_YANKMARK
	modifying_cmds = "aAcCdDiIJoOpPrRsxX<>~";	// cmds modifying text[]
#endif

	//  1-  process $HOME/.exrc file (not inplemented yet)
	//  2-  process EXINIT variable from environment
	//  3-  process command line args
#if ENABLE_FEATURE_VI_COLON
	{
		char *p = getenv("EXINIT");
		if (p && *p)
			initial_cmds[0] = xstrndup(p, MAX_INPUT_LEN);
	}
#endif
	while ((c = getopt(argc, argv, "hCRH-" USE_FEATURE_VI_COLON("c:"))) != -1) {
		switch (c) {
#if ENABLE_FEATURE_VI_READONLY
		case 'R':		// Read-only flag
			SET_READONLY_MODE(readonly_mode);
			break;
#endif
#if ENABLE_FEATURE_VI_COLON
		case 'c':		// cmd line vi command
			if (*optarg)
				initial_cmds[initial_cmds[0] != 0] = xstrndup(optarg, MAX_INPUT_LEN);
			break;
#endif
		case 'H':
		case '-':
			show_help();
			/* fall through */
		default:
			bb_show_usage();
			return 1;
		}
	}

	// The argv array can be used by the ":next"  and ":rewind" commands
	// save optind.
	fn_start = optind;	// remember first file name for :next and :rew
	save_argc = argc;

	//----- This is the main file handling loop --------------
	if (optind >= argc) {
#ifdef __UCSD__
		char *name = ucsd_askname();
		edit_file(*name ? name : 0);
#else
		edit_file(0);
#endif
	} else {
		for (; optind < argc; optind++) {
			edit_file(argv[optind]);
		}
	}
	//-----------------------------------------------------------

#if ENABLE_FEATURE_VI_PAGING
	pg_close();
#endif
	return 0;
}

/* read text from file or create an empty buf */
/* will also update current_filename */
int init_text_buffer(char *fn)
{
	int rc;
	int size = file_size(fn);	// file size. -1 means does not exist.

#if ENABLE_FEATURE_VI_PAGING
	/* the window: allocated once, as big as memory allows */
	if (!text) {
		pg_init();
		text_size = pg_cap;
		text = xzalloc(text_size + 1);
	}
	memset(text, 0, text_size);
	pg_reset();
	screenbegin = dot = end = text;
#else
	/* allocate/reallocate text buffer */
	free(text);
	text_size = size + TEXT_SLACK;
	screenbegin = dot = end = text = xzalloc(text_size);
#endif

	if (fn != current_filename) {
		free(current_filename);
		current_filename = xstrdup(fn);
	}
	if (size < 0) {
		// file dont exist. Start empty buf with dummy line
		char_insert(text, '\n');
		rc = 0;
	} else {
#if ENABLE_FEATURE_VI_PAGING
		rc = pg_load(fn);
#else
		rc = file_insert(fn, text, 1);
#endif
	}
	file_modified = 0;
	last_file_modified = -1;
#if ENABLE_FEATURE_VI_YANKMARK
	/* init the marks. */
	memset(mark, 0, sizeof(mark));
#if ENABLE_FEATURE_VI_PAGING
	memset(markl, 0, sizeof(markl));
	ctx_s = ctx_e = 0;
#endif
#endif
	return rc;
}

static void edit_file(char *fn)
{
#if ENABLE_FEATURE_VI_YANKMARK
#define cur_line edit_file__cur_line
#endif
	char c;
	editing = 1;	// 0 = exit, 1 = one file, 2 = multiple files
	if (rawmode()) {
		perror("vi");
		exit(5);
	}
	createScreen();
	init_text_buffer(fn);

#if ENABLE_FEATURE_VI_YANKMARK
	YDreg = 26;			// default Yank/Delete reg
	Ureg = 27;			// hold orig line for "U" cmd
	mark[26] = mark[27] = text;	// init "previous context"
#if ENABLE_FEATURE_VI_PAGING
	markl[26] = markl[27] = 1;
#endif
#endif

	last_forward_char = last_input_char = '\0';
	crow = 0;
	ccol = 0;
	tabstop = 8;
	offset = 0;			// no horizontal offset
	clear_screen();

#if ENABLE_FEATURE_VI_USE_SIGNALS
	catch_sig(0);
	sigsetjmp(restart, 1);
	signal(SIGWINCH, winch_sig);
	signal(SIGTSTP, suspend_sig);
	signal(SIGQUIT, quit_sig);
	signal(SIGTERM, quit_sig);
	signal(SIGPIPE, quit_sig);
	signal(SIGHUP, quit_sig);
	signal(SIGILL, quit_sig);
	signal(SIGSEGV, quit_sig);
	signal(SIGBUS, quit_sig);
	signal(SIGABRT, quit_sig);
#endif

	cmd_mode = CMODE_COMMAND;
	cmdcnt = 0;
	c = '\0';
#if ENABLE_FEATURE_VI_DOT_CMD
	free(ioq_start);
	ioq = ioq_start = NULL;
	lmc_len = 0;
	adding2q = 0;
#endif

#if ENABLE_FEATURE_VI_COLON
	{
		char *p, *q;
		int n = 0;

		while ((p = initial_cmds[n])) {
			do {
				q = p;
				p = strchr(q, '\n');
				if (p)
					while (*p == '\n')
						*p++ = '\0';
				if (*q)
					colon(q);
			} while (p);
			free(initial_cmds[n]);
			initial_cmds[n] = NULL;
			n++;
		}
	}
#endif

	//------This is the main Vi cmd handling loop -----------------------
	while (editing > 0) {
#if ENABLE_FEATURE_VI_PAGING
		pg_fix();
#endif
		refresh();
		last_input_char = c = get_one_char();	// get a cmd from user
		*status_buffer=0;
#if ENABLE_FEATURE_VI_YANKMARK
		// save a copy of the current line- for the 'U" command
		if (begin_line(dot) != cur_line) {
			cur_line = begin_line(dot);
			text_yank(begin_line(dot), end_line(dot), Ureg);
		}
#endif
#if ENABLE_FEATURE_VI_DOT_CMD
		// These are commands that change text[].
		// Remember the input for the "." command
		if (!adding2q && ioq_start == NULL
		 && strchr(modifying_cmds, c)
		) {
			start_new_cmd_q(c);
		}
#endif
		do_cmd(c);		// execute the user command
	}
	//-------------------------------------------------------------------
	refresh();
	gracefulExit();
#undef cur_line
}


//----- Set terminal attributes --------------------------------
#ifndef __UCSD__
int rawmode(void)
{
	int err = tcgetattr(0, &term_orig);
	if (err)
		return err;
	term_vi = term_orig;
	term_vi.c_lflag &= (~ICANON & ~ECHO);	// leave ISIG ON- allow intr's
	term_vi.c_iflag &= (~IXON & ~ICRNL);
	term_vi.c_oflag &= (~ONLCR);
	term_vi.c_cc[VMIN] = 1;
	term_vi.c_cc[VTIME] = 0;
	erase_char = term_vi.c_cc[VERASE];
	tcsetattr(0, TCSANOW, &term_vi);

	unsigned tics = 1;
    switch (cfgetispeed(&term_vi)) {
	case B600:
		tics = 2;
		break;
	case B300:
		tics = 4;
		break;
	case B200:
		tics = 6;
		break;
	case B150:
		tics = 7;
		break;
	case B134:
		tics = 8;
		break;
	case B110:
		tics = 10;
		break;
	case B75:
		tics = 15;
		break;
	case B50:
		tics = 21;
	} //determines how long to wait for ESCAPE sequences
	ticsPerChar = tics;
	return 0;
}
#endif

#ifndef __UCSD__
void cookmode(void)
{
	tcsetattr(0, TCSANOW, &term_orig);
}
#endif

//----- Come here when we get a window resize signal ---------
#if ENABLE_FEATURE_VI_USE_SIGNALS
static void winch_sig(int sig ATTRIBUTE_UNUSED)
{
	createScreen();
	redraw();
	fflush(stdout);
}

//----- Come here on QUIT, PIPE, TERM, HUP ----------------------
static void quit_sig(int sig)
{
	gracefulExit();
	signal(sig, SIG_DFL);
	kill(my_pid, sig);
}


//----- Come here when we get a continue signal -------------------
static void cont_sig(int sig ATTRIBUTE_UNUSED)
{
	rawmode();			// terminal to "raw"
	*status_buffer=0;	// force status update
	redraw();
	fflush(stdout);
	signal(SIGTSTP, suspend_sig);
	signal(SIGCONT, SIG_DFL);
	kill(my_pid, SIGCONT);
}

//----- Come here when we get a Suspend signal -------------------
static void suspend_sig(int sig ATTRIBUTE_UNUSED)
{
	gracefulExit();
	signal(SIGCONT, cont_sig);
	signal(SIGTSTP, SIG_DFL);
	kill(my_pid, SIGTSTP);
}

//----- Come here when we get a INT signal ---------------------------
void catch_sig(int sig)
{
	signal(SIGINT, catch_sig);
	if (sig)
		siglongjmp(restart, sig);
}
#endif /* FEATURE_VI_USE_SIGNALS */

#ifndef __UCSD__
int awaitInput(int tics)
//returns true if input is becomes available within tics/100 seconds
{
	fflush(stdout);
	tcdrain(STDOUT_FILENO);
	struct pollfd pfd[1];

	pfd[0].fd = 0;
	pfd[0].events = POLLIN;
	return safe_poll(pfd, 1, tics*10) > 0;
}
#endif

//----- IO Routines --------------------------------------------
#ifndef __UCSD__
char readit(void)	// read (maybe cursor) key from stdin
{
	char c;
	size_t n;
	struct esc_cmds {
		const char seq[4];
		char val;
	};

	static const struct esc_cmds esccmds[] = {
		{"OA"  , VI_K_UP      },   // cursor key Up
		{"OB"  , VI_K_DOWN    },   // cursor key Down
		{"OC"  , VI_K_RIGHT   },   // Cursor Key Right
		{"OD"  , VI_K_LEFT    },   // cursor key Left
		{"OH"  , VI_K_HOME    },   // Cursor Key Home
		{"OF"  , VI_K_END     },   // Cursor Key End
		{"[A"  , VI_K_UP      },   // cursor key Up
		{"[B"  , VI_K_DOWN    },   // cursor key Down
		{"[C"  , VI_K_RIGHT   },   // Cursor Key Right
		{"[D"  , VI_K_LEFT    },   // cursor key Left
		{"[H"  , VI_K_HOME    },   // Cursor Key Home
		{"[F"  , VI_K_END     },   // Cursor Key End
		{"[1~" , VI_K_HOME    },   // Cursor Key Home
		{"[2~" , VI_K_INSERT  },   // Cursor Key Insert
		{"[3~" , VI_K_DELETE  },   // Cursor Key Delete
		{"[4~" , VI_K_END     },   // Cursor Key End
		{"[5~" , VI_K_PAGEUP  },   // Cursor Key Page Up
		{"[6~" , VI_K_PAGEDOWN},   // Cursor Key Page Down
		{"OP"  , VI_K_FUN1    },   // Function Key F1
		{"OQ"  , VI_K_FUN2    },   // Function Key F2
		{"OR"  , VI_K_FUN3    },   // Function Key F3
		{"OS"  , VI_K_FUN4    },   // Function Key F4
		// careful: these have no terminating NUL!
		{"[11~", VI_K_FUN1    },   // Function Key F1
		{"[12~", VI_K_FUN2    },   // Function Key F2
		{"[13~", VI_K_FUN3    },   // Function Key F3
		{"[14~", VI_K_FUN4    },   // Function Key F4
		{"[15~", VI_K_FUN5    },   // Function Key F5
		{"[17~", VI_K_FUN6    },   // Function Key F6
		{"[18~", VI_K_FUN7    },   // Function Key F7
		{"[19~", VI_K_FUN8    },   // Function Key F8
		{"[20~", VI_K_FUN9    },   // Function Key F9
		{"[21~", VI_K_FUN10   },   // Function Key F10
		{"[23~", VI_K_FUN11   },   // Function Key F11
		{"[24~", VI_K_FUN12   },   // Function Key F12
	};
	enum { ESCCMDS_COUNT = ARRAY_SIZE(esccmds) };

	n = chars_to_parse;
	// get input from User- are there already input chars in Q?
	if (n <= 0) {
		// the Q is empty, wait for a typed char
		fflush(stdout);
		n = safe_read(STDIN_FILENO, readbuffer, sizeof(readbuffer));
		if (n < 0) {
			if (errno == EBADF || errno == EFAULT || errno == EINVAL
			 || errno == EIO)
				editing = 0; // want to exit
			errno = 0;
		}
		if (n <= 0)
			return 0;       // error
		if (readbuffer[0] == 27) {
			// This is an ESC char. Is this Esc sequence?
			// Could be bare Esc key. See if there are any
			// more chars to read after the ESC. This would
			// be a Function or Cursor Key sequence.
			// keep reading while there are input chars and room in buffer
			// for a complete ESC sequence (assuming 8 chars is enough)
			while(awaitInput(ticsPerChar)) {
				// read the rest of the ESC string
				int r = safe_read(STDIN_FILENO,
						readbuffer + n, sizeof(readbuffer) - n);
				if (r <= 0)
					break;
				n += r;
				if (n > sizeof(readbuffer)-8)
					break;
			}
		}
		chars_to_parse = n;
	}
	c = readbuffer[0];
	if (c == 27 && n > 1) {
		// Maybe cursor or function key?
		const struct esc_cmds *eindex;

		for (eindex = esccmds; eindex < &esccmds[ESCCMDS_COUNT]; eindex++) {
			int cnt = strnlen(eindex->seq, 4);
			if (n <= cnt)
				continue;
			if (strncmp(eindex->seq, readbuffer + 1, cnt) != 0)
				continue;
			c = eindex->val; // magic char value
			n = cnt + 1; // squeeze out the ESC sequence
			goto found;
		}
		// defined ESC sequence not found
	}
	n = 1;
found:
	// remove key sequence from Q
	chars_to_parse -= n;
	memmove(readbuffer, readbuffer + n, sizeof(readbuffer) - n);
	return c;
}
#endif

//----- IO Routines --------------------------------------------
char get_one_char(void)
{
	char c;

#if ENABLE_FEATURE_VI_DOT_CMD
	if (!adding2q) {
		// we are not adding to the q.
		// but, we may be reading from a q
		if (ioq == 0) {
			// there is no current q, read from STDIN
			c = readit();	// get the users input
		} else {
			// there is a queue to get chars from first
			c = *ioq++;
			if (c == '\0') {
				// the end of the q, read from STDIN
				free(ioq_start);
				ioq_start = ioq = 0;
				c = readit();	// get the users input
			}
		}
	} else {
		// adding STDIN chars to q
		c = readit();	// get the users input
		if (lmc_len >= MAX_INPUT_LEN - 1) {
			status_line_bold("last_modifying_cmd overrun");
		} else {
			// add new char to q
			last_modifying_cmd[lmc_len++] = c;
		}
	}
#else
	c = readit();		// get the users input
#endif /* FEATURE_VI_DOT_CMD */
	return c;
}

// Get input line (uses "status line" area)
char *get_input_line(const char *prompt)
{
	// char [MAX_INPUT_LEN]
#define buf status_buffer

	char c;
	int i;

	*displayed_buffer = 0;	// force status update
	cmd_mode |= CMODE_LINE_INPUT;
	strcpy(buf, prompt);
	place_cursor(rows - 1, 0, FALSE);	// go to Status line, bottom of screen
	clear_to_eol();		// clear the line
	write1(prompt);      // write out the :, /, or ? prompt

	i = strlen(buf);
	while (i < MAX_INPUT_LEN) {
		c = get_one_char();
		if (c == '\n' || c == '\r' || c == 27)
			break;		// this is end of input
		if (c == erase_char || c == 8 || c == 127) {
			// user wants to erase prev char
			buf[--i] = '\0';
			write1("\b \b"); // erase char on screen
			if (i <= 0) // user backs up before b-o-l, exit
				break;
		} else {
			buf[i] = c;
			buf[++i] = '\0';
			bb_putchar(c);
		}
	}
	cmd_mode &= ~CMODE_LINE_INPUT;
	return strcpy(get_input_line__buf, buf);
#undef buf
}

#ifndef __UCSD__
int file_size(const char *fn) // what is the byte size of "fn"
{
	struct stat st_buf;
	int cnt;

	cnt = -1;
	if (fn && fn[0] && stat(fn, &st_buf) == 0)	// see if file exists
		cnt = (int) st_buf.st_size;
	return cnt;
}
#endif

#ifndef __UCSD__
int file_insert(const char *fn, char *p
		, int update_ro_status)
{
	int cnt = -1;
	int fd, size;
	struct stat statbuf;

	/* Validate file */
	if (stat(fn, &statbuf) < 0) {
		status_line_bold("\"%s\" %s", fn, strerror(errno));
		goto fi0;
	}
	if (!S_ISREG(statbuf.st_mode)) {
		// This is not a regular file
		status_line_bold("\"%s\" Not a regular file", fn);
		goto fi0;
	}
	if (p < text || p > end) {
		status_line_bold("Trying to insert file outside of memory");
		goto fi0;
	}

	// read file to buffer
	fd = open(fn, O_RDONLY);
	if (fd < 0) {
		status_line_bold("\"%s\" %s", fn, strerror(errno));
		goto fi0;
	}
	size = statbuf.st_size;
	p = text_hole_make(p, size);
	if (!p) {
		close(fd);
		return -1;
	}
	cnt = safe_read(fd, p, size);
	if (cnt < 0) {
		status_line_bold("\"%s\" %s", fn, strerror(errno));
		p = text_hole_delete(p, p + size - 1);	// un-do buffer insert
	} else if (cnt < size) {
		// There was a partial read, shrink unused space text[]
		p = text_hole_delete(p + cnt, p + (size - cnt) - 1);	// un-do buffer insert
		status_line_bold("cannot read all of file \"%s\"", fn);
	}
	if (cnt >= size)
		file_modified++;
	close(fd);
 fi0:
#if ENABLE_FEATURE_VI_READONLY
	if (update_ro_status
	 && ((access(fn, W_OK) < 0) ||
		/* root will always have access()
		 * so we check fileperms too */
		!(statbuf.st_mode & (S_IWUSR | S_IWGRP | S_IWOTH))
	    )
	) {
		SET_READONLY_FILE(readonly_mode);
	}
#endif
	return cnt;
}
#endif

#ifndef __UCSD__
int file_write(char *fn, char *first, char *last)
{
	int fd, cnt, charcnt;

	if (fn == 0) {
		status_line_bold("No current filename");
		return -2;
	}
#if ENABLE_FEATURE_VI_PAGING
	if (pg_f && first == text && last == end - 1) {	// the whole file
		cnt = pg_save(fn);
		return cnt < 0 ? -1 : cnt ? last - first + 1 : 0;
	}
#endif
	charcnt = 0;
	/* By popular request we do not open file with O_TRUNC,
	 * but instead ftruncate() it _after_ successful write.
	 * Might reduce amount of data lost on power fail etc.
	 */
	fd = open(fn, (O_WRONLY | O_CREAT), 0666);
	if (fd < 0)
		return -1;
	cnt = last - first + 1;
	charcnt = full_write(fd, first, cnt);
	ftruncate(fd, charcnt);
	if (charcnt == cnt) {
		// good write
		//file_modified = FALSE;
	} else {
		charcnt = 0;
	}
	close(fd);
	return charcnt;
}
#endif
