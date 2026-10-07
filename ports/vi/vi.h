/* vi.h -- what the modules of vi share: the configuration, the global
   state (struct globals G), and the functions one module calls in another.
   vimain.c   start, main loop, keys, files, the terminal (Linux)
   viscreen.c the screen: drawing, the status line, the cursor
   vitext.c   moving in and changing the text, searching, registers
   vicolon.c  the : commands
   vicmd.c    the vi commands (do_cmd)
   vipage.c   a window into big files (vipage.h)
   viucsd.c   the P-System: keys, screen, files, missing library functions */

/* vi: set sw=4 ts=4: */
/*
 * tiny vi.c: A small 'vi' clone
 * Copyright (C) 2000, 2001 Sterling Huxley <sterling@europa.com>
 *
 * Licensed under the GPL v2 or later, see the file LICENSE in this tarball.
 * Revised:  4/23/20 brent@mbari.org -- NULL ptr deref on missing previous regex
 * Revised:	 5/21/20 brent@mbari.org -- extensive rework
 * Revised:	 2/14/24 Stefan Haubental -- added support for clang
 */

/*
 * Things To Do:
 *	EXINIT
 *	$HOME/.exrc  and  ./.exrc
 *	add magic to search	/foo.*bar
 *	add :help command
 *	:map macros
 *	if mark[] values were line numbers rather than pointers
 *	   it would be easier to change the mark when add/delete lines
 *	More intelligence in refresh()
 *	":r !cmd"  and  "!cmd"  to filter text through an external command
 *	A true "undo" facility
 *	An "ex" line oriented mode- maybe using "cmdedit"
 */

#ifdef __UCSD__
/* ---- UCSD Pascal II.0 / Tiny-C configuration ----
 * The P-System console is not a Unix tty: no termios, no signals, no
 * poll.  Keys come raw from UNITREAD (no echo, no line editing) and the
 * screen is driven with the operating system's own codes (SYSCOM's
 * CRTCTRL, and FGOTOXY for the cursor).  Files are UCSD text files read
 * and written through stdio. */
#define BB_VER "version 2.63 (UCSD)"
#define BB_BT "brent@mbari.org"
#include <stdarg.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <conio.h>
#include <psys.h>

#define vi_main			main
#define CONFIG_FEATURE_VI_MAX_LEN 132
#define TEXT_SLACK 1024	// free room in the text buffer: memory is small
#define ENABLE_FEATURE_VI_PAGING 1	// a window into big files (vipage.h)
#define VI_ROW_SUMS 1	// the screen as a checksum per row, not a copy
#define PG_RESERVE 1024	// memory left out of the window: yanks, and so on
#define PG_MAXCH 128	// chunks in VI.SWAP: files up to about 125 KB
#define ENABLE_FEATURE_VI_COLON 1
#define ENABLE_FEATURE_VI_YANKMARK 1
#define ENABLE_FEATURE_VI_SEARCH 1
#define ENABLE_FEATURE_VI_USE_SIGNALS 0
#define ENABLE_FEATURE_VI_DOT_CMD 1
#define ENABLE_FEATURE_VI_READONLY 1
#define ENABLE_FEATURE_VI_SETOPTS 1
#define ENABLE_FEATURE_VI_SET 1
#define ENABLE_FEATURE_VI_WIN_RESIZE 0
#define ENABLE_LOCALE_SUPPORT 0
#define ENABLE_FEATURE_VI_8BIT 0
#define ENABLE_FEATURE_ALLOW_EXEC 0

#define USE_FEATURE_VI_COLON(x) x
#define USE_FEATURE_VI_READONLY(x) x
#define USE_FEATURE_VI_YANKMARK(x) x
#define USE_FEATURE_VI_SEARCH(x) x

#define ALIGN1
#define FALSE 0
#define TRUE 1
#define MAIN_EXTERNALLY_VISIBLE
#define ATTRIBUTE_UNUSED

#undef isdigit
#define isdigit(a) ((unsigned)((a) - '0') <= 9)

#define ARRAY_SIZE(x) ((unsigned)(sizeof(x) / sizeof((x)[0])))

#define INIT_G()

typedef signed char smallint;

#define bb_show_usage()
#define bb_perror_msg(msg)  perror(msg)

#elif defined(STANDALONE)
#define BB_VER "version 2.63"
#define BB_BT "brent@mbari.org"

#define _GNU_SOURCE
#include <stdarg.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <setjmp.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <signal.h>
#include <errno.h>
#include <ctype.h>
#include <termios.h>
#include <poll.h>

#define vi_main			main
#define CONFIG_FEATURE_VI_MAX_LEN 4096
#define ENABLE_FEATURE_VI_COLON 1
#define ENABLE_FEATURE_VI_YANKMARK 1
#define ENABLE_FEATURE_VI_SEARCH 1
#define ENABLE_FEATURE_VI_USE_SIGNALS 1
#define ENABLE_FEATURE_VI_DOT_CMD 1
#define ENABLE_FEATURE_VI_READONLY 1
#define ENABLE_FEATURE_VI_SETOPTS 1
#define ENABLE_FEATURE_VI_SET 1
#define ENABLE_FEATURE_VI_WIN_RESIZE 1
#define ENABLE_LOCALE_SUPPORT 1
#define ENABLE_FEATURE_VI_8BIT 1
#define ENABLE_FEATURE_VI_YANKMARK 1
#define ENABLE_FEATURE_VI_SEARCH 1
#define	ENABLE_FEATURE_ALLOW_EXEC 1
#undef ENABLE_FEATURE_VI_OPTIMIZE_CURSOR

#define USE_FEATURE_VI_COLON(...) __VA_ARGS__
#define USE_FEATURE_VI_READONLY(...) __VA_ARGS__
#define	USE_FEATURE_VI_YANKMARK(...) __VA_ARGS__
#define	USE_FEATURE_VI_SEARCH(...) __VA_ARGS__

#define ALIGN1
#define FALSE 0
#define TRUE 1
#define MAIN_EXTERNALLY_VISIBLE
#define ATTRIBUTE_UNUSED __attribute__ ((__unused__))

#undef isdigit
#define isdigit(a) ((unsigned)((a) - '0') <= 9)

#define ARRAY_SIZE(x) ((unsigned)(sizeof(x) / sizeof((x)[0])))

#define INIT_G()

typedef signed char smallint;

#define bb_show_usage()
#define bb_perror_msg(msg)  perror(msg)


#ifdef __clang__
char *strchrnul(const char *s, int c_in)
{
        char c = c_in;

        while (*s && (*s != c))
                s++;
        return (char *) s;
}

void *memrchr(const void *s, int c_in, size_t n)
{
        if (n != 0) {
                const unsigned char *cp = (unsigned char *)s + n;
                do {
                        if (*(--cp) == (unsigned char) c_in)
                                return (void *) cp;
                } while (--n != 0);
        }
        return NULL;
}
#endif

#else  //in busybox

#include "libbb.h"
#define G (*ptr_to_globals)
#define INIT_G() do { \
	SET_PTR_TO_GLOBALS(xzalloc(sizeof(G))); \
	last_file_modified = -1; \
} while (0)

#endif

#include <limits.h>

#ifndef ENABLE_FEATURE_VI_PAGING
#define ENABLE_FEATURE_VI_PAGING 0
#endif

#ifndef TEXT_SLACK
#define TEXT_SLACK 10240	// free room in the text buffer
#endif



#if ENABLE_LOCALE_SUPPORT

#if ENABLE_FEATURE_VI_8BIT
#define Isprint(c) isprint(c)
#else
#define Isprint(c) (isprint(c) && (unsigned char)(c) < 0x7f)
#endif

#else

/* 0x9b is Meta-ESC */
#if ENABLE_FEATURE_VI_8BIT
#define Isprint(c) ((unsigned char)(c) >= ' ' && (c) != 0x7f && (unsigned char)(c) != 0x9b)
#else
#define Isprint(c) ((unsigned char)(c) >= ' ' && (unsigned char)(c) < 0x7f)
#endif

#endif

#if ENABLE_FEATURE_VI_READONLY
#define EDIT_STATUS		"%s: %s%s%s line %d/%d %d%%"
#else
#define EDIT_STATUS		"%s: %s%s line %d/%d %d%%"
#endif

enum {
	MAX_TABSTOP = 32, // sanity limit
	// User input len. Need not be extra big.
	// Lines in file being edited *can* be bigger than this.
	MAX_INPUT_LEN = 128,
	// Sanity limits. We have only one buffer of this size.
	MAX_SCR_COLS = CONFIG_FEATURE_VI_MAX_LEN,
	MAX_SCR_ROWS = CONFIG_FEATURE_VI_MAX_LEN,
};

// Misc. non-Ascii keys that report an escape sequence
#define VI_K_UP			(char)128	// cursor key Up
#define VI_K_DOWN		(char)129	// cursor key Down
#define VI_K_RIGHT		(char)130	// Cursor Key Right
#define VI_K_LEFT		(char)131	// cursor key Left
#define VI_K_HOME		(char)132	// Cursor Key Home
#define VI_K_END		(char)133	// Cursor Key End
#define VI_K_INSERT		(char)134	// Cursor Key Insert
#define VI_K_DELETE		(char)135	// Cursor Key Insert
#define VI_K_PAGEUP		(char)136	// Cursor Key Page Up
#define VI_K_PAGEDOWN		(char)137	// Cursor Key Page Down
#define VI_K_FUN1		(char)138	// Function Key F1
#define VI_K_FUN2		(char)139	// Function Key F2
#define VI_K_FUN3		(char)140	// Function Key F3
#define VI_K_FUN4		(char)141	// Function Key F4
#define VI_K_FUN5		(char)142	// Function Key F5
#define VI_K_FUN6		(char)143	// Function Key F6
#define VI_K_FUN7		(char)144	// Function Key F7
#define VI_K_FUN8		(char)145	// Function Key F8
#define VI_K_FUN9		(char)146	// Function Key F9
#define VI_K_FUN10		(char)147	// Function Key F10
#define VI_K_FUN11		(char)148	// Function Key F11
#define VI_K_FUN12		(char)149	// Function Key F12

enum {
	YANKONLY = FALSE,
	YANKDEL = TRUE,
	FORWARD = 1,	// code depends on "1"  for array index
	BACK = -1,	// code depends on "-1" for array index
	LIMITED = 0,	// how much of text[] in char_search
	FULL = 1,	// how much of text[] in char_search

	S_BEFORE_WS = 1,	// used in skip_thing() for moving "dot"
	S_TO_WS = 2,		// used in skip_thing() for moving "dot"
	S_OVER_WS = 3,		// used in skip_thing() for moving "dot"
	S_END_PUNCT = 4,	// used in skip_thing() for moving "dot"
	S_END_ALNUM = 5,	// used in skip_thing() for moving "dot"
};

enum {  //cmd_modes
	CMODE_COMMAND,
	CMODE_INSERT,
	CMODE_REPLACE,
	CMODES,
	CMODE_LINE_INPUT = 1<<4
};

/* vi.c expects chars to be unsigned. */
/* busybox build system provides that, but it's better */
/* to audit and fix the source */

struct globals {
	/* many references - keep near the top of globals */
	char *text, *end;       // pointers to the user data in memory
	char *dot;              // where all the action takes place
	int text_size;		// size of the allocated buffer

	/* the rest */
	smallint vi_setops;
#define VI_AUTOINDENT 1
#define VI_SHOWMATCH  2
#define VI_IGNORECASE 4
#define VI_ERR_METHOD 8
#define autoindent (vi_setops & VI_AUTOINDENT)
#define showmatch  (vi_setops & VI_SHOWMATCH )
#define ignorecase (vi_setops & VI_IGNORECASE)
/* indicate error with beep or flash */
#define err_method (vi_setops & VI_ERR_METHOD)

#if ENABLE_FEATURE_VI_READONLY
	smallint readonly_mode;
#define SET_READONLY_FILE(flags)        ((flags) |= 0x01)
#define SET_READONLY_MODE(flags)        ((flags) |= 0x02)
#define UNSET_READONLY_FILE(flags)      ((flags) &= 0xfe)
#else
#define SET_READONLY_FILE(flags)        ((void)0)
#define SET_READONLY_MODE(flags)        ((void)0)
#define UNSET_READONLY_FILE(flags)      ((void)0)
#endif

	smallint editing;        // >0 while we are editing a file
	                         // [code audit says "can be 0 or 1 only"]
	smallint cmd_mode;       // 0=command  1=insert 2=replace
	int file_modified;       // buffer contents changed (counter, not flag!)
	int last_file_modified;  // = -1;
	int fn_start;            // index of first cmd line file name
	int save_argc;           // how many file names on cmd line
	int cmdcnt;              // repetition count
	unsigned rows, columns;	 // the terminal screen is this size
	int crow, ccol;          // cursor is on Crow x Ccol
	int offset;              // chars scrolled off the screen to the left
	char *current_filename;
	char *screenbegin;       // index into text[], of top line on the screen
	char *screen;            // pointer to the virtual screen buffer
	int screensize;          //            and its size
	int tabstop;
	char erase_char;         // the users erase character
	char last_input_char;    // last char read from user
	char last_forward_char;  // last char searched for with 'f'

#if ENABLE_FEATURE_VI_DOT_CMD
	smallint adding2q;	 // are we currently adding user input to q
	int lmc_len;             // length of last_modifying_cmd
	char *ioq, *ioq_start;   // pointer to string for get_one_char to "read"
#endif
#if ENABLE_FEATURE_VI_OPTIMIZE_CURSOR
	int last_row;		 // where the cursor was last moved to
#endif
#if ENABLE_FEATURE_VI_USE_SIGNALS
	int my_pid;
#endif
#if ENABLE_FEATURE_VI_DOT_CMD || ENABLE_FEATURE_VI_YANKMARK
	char *modifying_cmds;    // cmds that modify text[]
#endif
#if ENABLE_FEATURE_VI_SEARCH
	char *last_search_pattern; // last pattern from a '/' or '?' search
#endif
	int chars_to_parse;
	/* former statics */
#if ENABLE_FEATURE_VI_YANKMARK
	char *edit_file__cur_line;
#endif
	int refresh__old_offset;
	int format_edit_status__tot;

	/* a few references only */
#if ENABLE_FEATURE_VI_YANKMARK
	int YDreg, Ureg;        // default delete register and orig line for "U"
	char *reg[28];          // named register a-z, "D", and "U" 0-25,26,27
	char *mark[28];         // user marks points somewhere in text[]-  a-z and previous context ''
	char *context_start, *context_end;
#if ENABLE_FEATURE_VI_PAGING
	int markl[28];          // marks as line numbers: the window moves
	int ctx_s, ctx_e;       // the context, as line numbers
#endif
#endif
#if ENABLE_FEATURE_VI_USE_SIGNALS
	sigjmp_buf restart;     // catch_sig()
#endif
#ifndef __UCSD__
	struct termios term_orig, term_vi; // remember what the cooked mode was
#endif
	unsigned ticsPerChar;	//# of 100hz tics per character received
#if ENABLE_FEATURE_VI_COLON
	char *initial_cmds[3];  // currently 2 entries, NULL terminated
#endif
	// Should be just enough to hold a key sequence,
	// but CRASME mode uses it as generated command buffer too
	char readbuffer[128];
#define STATUS_BUFFER_LEN  200
	char status_buffer[STATUS_BUFFER_LEN]; // messages to the user
	char displayed_buffer[STATUS_BUFFER_LEN];  //  displayed status
#if ENABLE_FEATURE_VI_DOT_CMD
	char last_modifying_cmd[MAX_INPUT_LEN];	// last modifying cmd for "."
#endif
	char get_input_line__buf[MAX_INPUT_LEN]; /* former static */

	char scr_out_buf[MAX_SCR_COLS + MAX_TABSTOP * 2];
};
extern struct globals G;
#define text           (G.text          )
#define text_size      (G.text_size     )
#define end            (G.end           )
#define dot            (G.dot           )
#define reg            (G.reg           )

#define vi_setops               (G.vi_setops          )
#define editing                 (G.editing            )
#define cmd_mode                (G.cmd_mode           )
#define file_modified           (G.file_modified      )
#define last_file_modified      (G.last_file_modified )
#define fn_start                (G.fn_start           )
#define save_argc               (G.save_argc          )
#define cmdcnt                  (G.cmdcnt             )
#define rows                    (G.rows               )
#define columns                 (G.columns            )
#define crow                    (G.crow               )
#define ccol                    (G.ccol               )
#define offset                  (G.offset             )
#define status_buffer           (G.status_buffer      )
#define displayed_buffer        (G.displayed_buffer   )
#define current_filename        (G.current_filename   )
#define screen                  (G.screen             )
#define screensize              (G.screensize         )
#define screenbegin             (G.screenbegin        )
#define tabstop                 (G.tabstop            )
#define erase_char              (G.erase_char         )
#define last_input_char         (G.last_input_char    )
#define last_forward_char       (G.last_forward_char  )
#if ENABLE_FEATURE_VI_READONLY
#define readonly_mode           (G.readonly_mode      )
#else
#define readonly_mode           0
#endif
#define adding2q                (G.adding2q           )
#define lmc_len                 (G.lmc_len            )
#define ioq                     (G.ioq                )
#define ioq_start               (G.ioq_start          )
#define last_row                (G.last_row           )
#define my_pid                  (G.my_pid             )
#define modifying_cmds          (G.modifying_cmds     )
#define last_search_pattern     (G.last_search_pattern)
#define chars_to_parse          (G.chars_to_parse     )

#define edit_file__cur_line     (G.edit_file__cur_line)
#define refresh__old_offset     (G.refresh__old_offset)
#define format_edit_status__tot (G.format_edit_status__tot)

#define YDreg          (G.YDreg         )
#define Ureg           (G.Ureg          )
#define mark           (G.mark          )
#define context_start  (G.context_start )
#define context_end    (G.context_end   )
#define markl          (G.markl         )
#define ctx_s          (G.ctx_s         )
#define ctx_e          (G.ctx_e         )
#define restart        (G.restart       )
#define term_orig      (G.term_orig     )
#define ticsPerChar	   (G.ticsPerChar   )
#define term_vi        (G.term_vi       )
#define initial_cmds   (G.initial_cmds  )
#define readbuffer     (G.readbuffer    )
#define scr_out_buf    (G.scr_out_buf   )
#define last_modifying_cmd  (G.last_modifying_cmd )
#define get_input_line__buf (G.get_input_line__buf)


#ifdef __UCSD__
/* in viucsd.c */
#define isblank(c) ((c) == ' ' || (c) == '\t')
#define errno (SYSCOM->iorslt)
extern int optind;
extern char *optarg;
#endif

#if !ENABLE_FEATURE_VI_OPTIMIZE_CURSOR
#define place_cursor(a, b, optimize) place_cursor(a, b)
#endif
#define indicate_error(c) Indicate_Error()
#if !ENABLE_FEATURE_VI_DOT_CMD
#define end_cmd_q() ((void)0)
#endif

#if ENABLE_FEATURE_VI_PAGING
#include "vipage.h"
#define ABSLINE(p) (pg_lb + count_lines(text, p))	// p's line in the file
#define TOTLINES() (pg_lines())			// the file's lines
#else
#define ABSLINE(p) count_lines(text, p)
#define TOTLINES() count_lines(text, end - 1)
#endif

/* ---- the functions the modules share ---- */
void *xmalloc(size_t size);
void *xzalloc(size_t size);
void *xstrdup(const char *s);
void *xstrndup(const char *s, size_t n);
void *xrealloc(void *old, size_t size);
char* last_char_is(const char *s, int c);
int bb_putchar(int ch);
void write1(const char *out);
void clear_screen(void);
void gracefulExit(void);
void createScreen(void);
int init_text_buffer(char *fn);
int rawmode(void);
void cookmode(void);
void catch_sig(int sig);
int awaitInput(int tics);
char readit(void);
char get_one_char(void);
char *get_input_line(const char *prompt);
int file_size(const char *fn);
int file_insert(const char *fn, char *p, int update_ro_status);
int file_write(char *fn, char *first, char *last);
void Hit_Return(void);
int next_tabstop(int col);
void place_cursor(int row, int col, int optimize);
void clear_to_eol(void);
void clear_to_eos(void);
void standout_start(void);
void standout_end(void);
void flash(int h);
void Indicate_Error(void);
void screen_erase(void);
void status_line_bold(const char *format, ...);
void status_line(const char *format, ...);
void not_implemented(const char *s);
int format_edit_status(const char *fmt);
void redraw(void);
void refresh(void);
char *begin_line(char *p);
char *end_line(char *p);
char *dollar_line(char *p);
char *prev_line(char *p);
char *next_line(char *p);
char *end_screen(void);
int count_lines(char *start, char *stop);
char *find_line(int li);
void dot_left(void);
void dot_right(void);
void dot_begin(void);
void dot_end(void);
char *move_to_col(char *p, int l);
void dot_next(void);
void dot_prev(void);
void dot_scroll(int cnt, int dir);
void dot_skip_over_ws(void);
void dot_delete(void);
char *bound_dot(char *p);
char *new_screen(int ro, int co);
int mycmp(const char *s1, const char *s2, int len);
char *char_search(char *p, const char *pat, int dir, int range);
char *char_insert(char *p, char c);
int find_range(char **start, char **stop, char c);
char *skip_thing(char *p, int linecnt, int dir, int type);
char *find_pair(char *p, const char c);
char *text_hole_make(char *p, int size);
char *text_hole_delete(char *p, char *q);
char *yank_delete(char *start, char *stop, int dist, int yf);
void show_help(void);
void start_new_cmd_q(char c);
void end_cmd_q(void);
char *string_insert(char *p, char *s);
char *text_yank(char *p, char *q, int dest);
char what_reg(void);
void check_context(char cmd);
char *swap_context(char *p);
void showmatching(char *p);
void colon(char *buf);
void colon1(char *buf);
void do_cmd(char c);
#ifdef __UCSD__
/* the P-System: viucsd.c */
char *strchrnul(const char *s, int c);
void *memrchr(const void *s, int c, size_t n);
int strncasecmp(const char *a, const char *b, size_t n);
char *strerror(int e);
int snprintf(char *buf, size_t n, const char *fmt, ...);
int getopt(int argc, char **argv, const char *opts);
void ucsd_reserve(void);
void ucsd_unreserve(void);
char *ucsd_askname(void);
#endif
