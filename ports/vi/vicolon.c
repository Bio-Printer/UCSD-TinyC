/* vicolon.c -- vi: the : commands */
#include "vi.h"

static char *get_one_address(char *p, int *addr);
static char *get_address(char *p, int *b, int *e);
static void setops(const char *args, const char *opname, int flg_no, const char *short_opname, int opt);
static char *stpcopy(char *dest, const char *src);
static void colon_set(char *args);
static int colon_s(char *orig_buf, char *q, int b, int e);

//----- The Colon commands -------------------------------------
#if ENABLE_FEATURE_VI_COLON
static char *get_one_address(char *p, int *addr)	// get colon addr, if present
{
	int st;
	char *q;
	USE_FEATURE_VI_YANKMARK(char c;)
	USE_FEATURE_VI_SEARCH(char *pat;)

	*addr = -1;			// assume no addr
	if (*p == '.') {	// the current line
		p++;
		q = begin_line(dot);
		*addr = ABSLINE(q);
	}
#if ENABLE_FEATURE_VI_YANKMARK
	else if (*p == '\'') {	// is this a mark addr
		p++;
		c = tolower(*p);
		p++;
		if (c >= 'a' && c <= 'z') {
			// we have a mark
			c = c - 'a';
#if ENABLE_FEATURE_VI_PAGING
			if (markl[(unsigned char) c])
				*addr = markl[(unsigned char) c];
#else
			q = mark[(unsigned char) c];
			if (q != NULL) {	// is mark valid
				*addr = count_lines(text, q);	// count lines
			}
#endif
		}
	}
#endif
#if ENABLE_FEATURE_VI_SEARCH
	else if (*p == '/') {	// a search pattern
		q = strchrnul(++p, '/');
		pat = xstrndup(p, q - p); // save copy of pattern
		p = q;
		if (*p == '/')
			p++;
		q = char_search(dot, pat, FORWARD, FULL);
		if (q != NULL) {
			*addr = ABSLINE(q);
		}
#if ENABLE_FEATURE_VI_PAGING
		else if (pg_find_line(pat))
			*addr = pg_find_line(pat);
#endif
		free(pat);
	}
#endif
	else if (*p == '$') {	// the last line in file
		p++;
		*addr = TOTLINES();
	} else if (isdigit(*p)) {	// specific line number
		// (by hand: sscanf would bring the library's whole scanf)
		for (*addr = 0; isdigit(*p); p++)
			*addr = *addr * 10 + (*p - '0');
	} else {
		// unrecognised address - assume -1
		*addr = -1;
	}
	return p;
}

static char *get_address(char *p, int *b, int *e)	// get two colon addrs, if present
{
	//----- get the address' i.e., 1,3   'a,'b  -----
	// get FIRST addr, if present
	while (isblank(*p))
		p++;				// skip over leading spaces
	if (*p == '%') {			// alias for 1,$
		p++;
		*b = 1;
		*e = TOTLINES();
		goto ga0;
	}
	p = get_one_address(p, b);
	while (isblank(*p))
		p++;
	if (*p == ',') {			// is there a address separator
		p++;
		while (isblank(*p))
			p++;
		// get SECOND addr, if present
		p = get_one_address(p, e);
	}
 ga0:
	while (isblank(*p))
		p++;				// skip over trailing spaces
	return p;
}

#if ENABLE_FEATURE_VI_SET && ENABLE_FEATURE_VI_SETOPTS
static void setops(const char *args, const char *opname, int flg_no,
			const char *short_opname, int opt)
{
	const char *a = args + flg_no;
	int l = strlen(opname) - 1; /* opname have + ' ' */

	if (strncasecmp(a, opname, l) == 0
	 || strncasecmp(a, short_opname, 2) == 0
	) {
		if (flg_no)
			vi_setops &= ~opt;
		else
			vi_setops |= opt;
	}
}
#endif

#if ENABLE_FEATURE_VI_SETOPTS
// show the matching char of a pair,  ()  []  {}
void showmatching(char *p)
{
	char *q, *save_dot;

	// we found half of a pair
	q = find_pair(p, *p);	// get loc of matching char
	if (q == NULL) {
		indicate_error('3');	// no matching char
	} else {
		// "q" now points to matching pair
		save_dot = dot;	// remember where we are
		dot = q;		// go to new loc
		refresh();		// let the user see it
		awaitInput(40);	// give user some time
		dot = save_dot;	// go back to old loc
		refresh();
	}
}

static char *stpcopy(char *dest, const char *src)
{
	while(*src) *dest++ = *src++;
	*dest = 0;
	return dest;
}
#endif /* FEATURE_VI_SETOPTS */

// buf must be no longer than MAX_INPUT_LEN!
#if ENABLE_FEATURE_VI_SETOPTS
// colon()'s :set, on its own to keep colon() within one P-code
// procedure's limits
static void colon_set(char *args)
{
	int i, ch;
	{
		char *argp = args;
		while (*argp) {
		  i = 0;			// offset into args
			if (strncasecmp(argp, "no", 2) == 0)
				i = 2;		// ":set noautoindent"
			setops(argp, "autoindent ", i, "ai", VI_AUTOINDENT);
			setops(argp, "flash ", i, "fl", VI_ERR_METHOD);
			setops(argp, "ignorecase ", i, "ic", VI_IGNORECASE);
			setops(argp, "showmatch ", i, "sm", VI_SHOWMATCH);
			/* tabstopXXXX */
			if (strncasecmp(argp + i, "tabstop=%d ", 7) == 0) {
				char *t = strchr(argp + i, '=') + 1;
				for (ch = 0; isdigit(*t); t++)
					ch = ch * 10 + (*t - '0');
				if (ch > 0 && ch <= MAX_TABSTOP)
					tabstop = ch;
			}
			while (*argp && *argp != ' ')
				argp++; // skip to arg delimiter (i.e. blank)
			while (*argp && *argp == ' ')
				argp++; // skip all delimiting blanks
		}
		// display values of all options in status line
		char *cursor = status_buffer;
		*cursor = 0;
		if (!autoindent)
			cursor = stpcopy(cursor,"no");
		cursor = stpcopy(cursor,"autoindent ");
		if (!err_method)
			cursor = stpcopy(cursor,"no");
		cursor = stpcopy(cursor,"flash ");
		if (!ignorecase)
			cursor = stpcopy(cursor,"no");
		cursor = stpcopy(cursor,"ignorecase ");
		if (!showmatch)
			cursor = stpcopy(cursor,"no");
		cursor = stpcopy(cursor,"showmatch ");
		cursor += sprintf(cursor,"tabstop=%d ", tabstop);
	}
}
#endif

// colon()'s :s/find/replace/, on its own to keep colon() within one
// P-code procedure's limits; returns 1 when colon() must return at once
static int colon_s(char *orig_buf, char *q, int b, int e)
{
	{
		char *ls, *F, *R, *buf1;
		int gflag, i;
		char c;

		// F points to the "find" pattern
		// R points to the "replace" pattern
		// replace the cmd line delimiters "/" with NULLs
		gflag = 0;		// global replace flag
		c = orig_buf[1];	// what is the delimiter
		F = orig_buf + 2;	// start of "find"
		R = strchr(F, c);	// middle delimiter
		if (!R) {
			status_line_bold(":s expression missing delimiters");
			return 1;
		}
		if (R == F) { // use previous search pattern if no find pattern given
			if (!(F = last_search_pattern))
			{
				status_line_bold("No previous regular expression");
				return 1;
			}
			F++;  //ignore search direction
		}
		*R++ = '\0';	// terminate "find"
		buf1 = strchr(R, c);
		if (buf1) {  //accept :s/foo/bar
			*buf1++ = '\0';	// terminate "replace"
			if (*buf1 == 'g') {	// :s/foo/bar/g
				buf1++;
				gflag++;	// turn on gflag
			}
		}
		q = begin_line(q);
		if (b < 0) {	// maybe :s/foo/bar/
			q = begin_line(dot);	// start with cur line
			b = ABSLINE(q);	// cur line number
		}
		if (e < 0)
			e = b;		// maybe :.s/foo/bar/
		for (i = b; i <= e; i++) {	// so, :20,23 s \0 find \0 replace \0
			ls = q;		// orig line start
vc4:
			buf1 = char_search(q, F, FORWARD, LIMITED);	// search cur line only for "find"
			if (buf1) {
				// we found the "find" pattern - delete it
				text_hole_delete(buf1, buf1 + strlen(F) - 1);
				// inset the "replace" patern
				string_insert(buf1, R);	// insert the string
				// check for "global"  :s/foo/bar/g
				if (gflag == 1) {
					if ((buf1 + strlen(R)) < end_line(ls)) {
						q = buf1 + strlen(R);
						goto vc4;	// don't let q move past cur line
					}
				}
			}
			q = next_line(ls);
		}
	}
	return 0;
}

#if ENABLE_FEATURE_VI_PAGING
// the lines a : command works on stay where they are until it is done
void colon(char *buf)
{
	int lines, l;

	lines = TOTLINES();
	pg_park = NULL;
	pg_keep = 0;
	colon1(buf);
	pg_lock = 0;
	if (pg_park && pg_keep && dot == pg_park) {
		// its lines did not fit with the cursor's: back to the cursor's line
		l = pg_park_l;
		if (pg_park_a < l)
			l += TOTLINES() - lines;
		if (pg_park_a < pg_top)
			pg_top += TOTLINES() - lines;
		dot = find_line(l);
		while (pg_park_c-- > 0 && *dot != '\n')
			dot++;
	}
	pg_park = NULL;
}
#else
#define colon1 colon
#endif
void colon1(char *buf)
{
	char c, *orig_buf, *buf1, *q, *r;
	char *fn, cmd[MAX_INPUT_LEN], args[MAX_INPUT_LEN];
	int i, l, li, ch, b, e;
	int useforce, forced = FALSE;

	// :3154	// if (-e line 3154) goto it  else stay put
	// :4,33w! foo	// write a portion of buffer to file "foo"
	// :w		// write all of buffer to current file
	// :q		// quit
	// :q!		// quit- dont care about modified file
	// :'a,'z!sort -u   // filter block through sort
	// :'f		// goto mark "f"
	// :'fl		// list literal the mark "f" line
	// :.r bar	// read file "bar" into buffer before dot
	// :/123/,/abc/d    // delete lines from "123" line to "abc" line
	// :/xyz/	// goto the "xyz" line
	// :s/find/replace/ // substitute pattern "find" with "replace"
	// :!<cmd>	// run <cmd> then return
	//

	if (!buf[0])
		goto vc1;
	if (*buf == ':')
		buf++;			// move past the ':'

	li = ch = i = 0;
	b = e = -1;
	q = text;			// assume 1,$ for the range
	r = end - 1;
	li = TOTLINES();
	fn = current_filename;

	// look for optional address(es)  :.  :1  :1,9   :'q,'a   :%
	buf = get_address(buf, &b, &e);
#if ENABLE_FEATURE_VI_PAGING
	if (b >= 0 && !pg_hold(b, e >= 0 ? e : b)) {
		status_line_bold("Lines %d to %d do not fit in memory", b, e >= 0 ? e : b);
		return;
	}
	pg_lock = 1;
	q = text;			// the window may have moved
	r = end - 1;
#endif

	// remember orig command line
	orig_buf = buf;

	// get the COMMAND into cmd[]
	buf1 = cmd;
	while (*buf != '\0') {
		if (isspace(*buf))
			break;
		*buf1++ = *buf++;
	}
	*buf1 = '\0';
	// get any ARGuments
	while (isblank(*buf))
		buf++;
	strcpy(args, buf);
	useforce = FALSE;
	buf1 = last_char_is(cmd, '!');
	if (buf1) {
		useforce = TRUE;
		*buf1 = '\0';   // get rid of !
	}
	if (b >= 0) {
		// if there is only one addr, then the addr
		// is the line number of the single line the
		// user wants. So, reset the end
		// pointer to point at end of the "b" line
		q = find_line(b);	// what line is #b
		r = end_line(q);
		li = 1;
	}
	if (e >= 0) {
		// we were given two addrs.  change the
		// end pointer to the addr given by user.
		r = find_line(e);	// what line is #e
		r = end_line(r);
		li = e - b + 1;
	}
	// ------------ now look for the command ------------
	i = strlen(cmd);
#if ENABLE_FEATURE_VI_PAGING
	// these leave the cursor where it was (colon() puts it back if pg_hold moved it)
	pg_keep = i > 0 && strchr("swy=lf", cmd[0]) != NULL;
#endif
	if (i == 0) {		// :123CR goto line #123
		if (b >= 0) {
			dot = find_line(b);	// what line is #b
			dot_skip_over_ws();
		}
	}
#if ENABLE_FEATURE_ALLOW_EXEC
	else if (strncmp(cmd, "!", 1) == 0) {	// run a cmd
		int retcode;
		// :!ls   run the <cmd>
		clear_screen();
		standout_start();
		write1(status_buffer+2);
		standout_end();
		cookmode();
		write1("\n");
		retcode = system(orig_buf + 1) >> 8;	// run the cmd
		if (retcode)
			printf("\nshell returned %i\n\n", retcode);
		rawmode();
		Hit_Return();			// let user see results
	}
#endif
	else if (strncmp(cmd, "=", i) == 0) {	// where is the address
		if (b < 0) {	// no addr given- use defaults
			b = e = ABSLINE(dot);
		}
		status_line("%d", b);
	} else if (strncasecmp(cmd, "delete", i) == 0) {	// delete lines
		if (b < 0) {	// no addr given- use defaults
			q = begin_line(dot);	// assume .,. for the range
			r = end_line(dot);
		}
		dot = yank_delete(q, r, 1, YANKDEL);	// save, then delete lines
		dot_skip_over_ws();
	} else if (strncasecmp(cmd, "edit", i) == 0) {	// Edit a file
		// don't edit, if the current file has been modified
		if (file_modified && !useforce) {
			status_line_bold("No write since last change (:edit! overrides)");
			goto vc1;
		}
		if (args[0]) {
			// the user supplied a file name
			fn = args;
		} else if (current_filename && current_filename[0]) {
			// no user supplied name- use the current filename
			// fn = current_filename;  was set by default
		} else {
			// no user file name, no current name- punt
			status_line_bold("No current filename");
			goto vc1;
		}

		if (init_text_buffer(fn) < 0)
			goto vc1;

#if ENABLE_FEATURE_VI_YANKMARK
		if (Ureg >= 0 && Ureg < 28 && reg[Ureg] != 0) {
			free(reg[Ureg]);	//   free orig line reg- for 'U'
			reg[Ureg]= 0;
		}
		if (YDreg >= 0 && YDreg < 28 && reg[YDreg] != 0) {
			free(reg[YDreg]);	//   free default yank/delete register
			reg[YDreg]= 0;
		}
#endif
		// how many lines in text[]?
		li = TOTLINES();
		status_line("\"%s\"%s"
			USE_FEATURE_VI_READONLY("%s")
			" %dL, %dC", current_filename,
			(file_size(fn) < 0 ? " [New file]" : ""),
			((readonly_mode) ? " [Readonly]" : ""),
			li, ch);
	} else if (strncasecmp(cmd, "file", i) == 0) {	// what File is this
		if (b != -1 || e != -1) {
			not_implemented("No address allowed on this command");
			goto vc1;
		}
		if (args[0]) {
			// user wants a new filename
			free(current_filename);
			current_filename = xstrdup(args);
		}
	} else if (strncasecmp(cmd, "features", i) == 0) {	// what features are available
		// print out values of all features
		place_cursor(rows - 1, 0, FALSE);	// go to Status line, bottom of screen
		clear_to_eol();	// clear the line
		cookmode();
		show_help();
		rawmode();
		Hit_Return();
	} else if (strncasecmp(cmd, "list", i) == 0) {	// literal print line
		if (b < 0) {	// no addr given- use defaults
			q = begin_line(dot);	// assume .,. for the range
			r = end_line(dot);
		}
		place_cursor(rows - 1, 0, FALSE);	// go to Status line, bottom of screen
		clear_to_eol();	// clear the line
		puts("\r");
		for (; q <= r; q++) {
			int c_is_no_print;

			c = *q;
			c_is_no_print = (c & 0x80) && !Isprint(c);
			if (c_is_no_print) {
				c = '.';
				standout_start();
			}
			if (c == '\n') {
				write1("$\r");
			} else if (c < ' ' || c == 127) {
				bb_putchar('^');
				if (c == 127)
					c = '?';
				else
					c += '@';
			}
			bb_putchar(c);
			if (c_is_no_print)
				standout_end();
		}
		Hit_Return();
	} else if (strncasecmp(cmd, "quit", i) == 0 // Quit
	        || strncasecmp(cmd, "next", i) == 0 // edit next file
	) {
		if (useforce) {
			// force end of argv list
			if (*cmd == 'q') {
				optind = save_argc;
			}
			editing = 0;
			goto vc1;
		}
		// don't exit if the file been modified
		if (file_modified) {
			status_line_bold("No write since last change (:%s! overrides)",
				 (*cmd == 'q' ? "quit" : "next"));
			goto vc1;
		}
		// are there other file to edit
		if (*cmd == 'q' && optind < save_argc - 1) {
			status_line_bold("%d more file to edit", (save_argc - optind - 1));
			goto vc1;
		}
		if (*cmd == 'n' && optind >= save_argc - 1) {
			status_line_bold("No more files to edit");
			goto vc1;
		}
		editing = 0;
	} else if (strncasecmp(cmd, "read", i) == 0) {	// read file into text[]
		fn = args;
		if (!fn[0]) {
			status_line_bold("No filename given");
			goto vc1;
		}
		if (b < 0) {	// no addr given- use defaults
			q = begin_line(dot);	// assume "dot"
		}
		// read after current line- unless user said ":0r foo"
		if (b != 0)
			q = next_line(q);
		ch = file_insert(fn, q, 0);
		if (ch < 0)
			goto vc1;	// nothing was inserted
		// how many lines in text[]?
		li = count_lines(q, q + ch - 1);
		status_line("\"%s\""
			USE_FEATURE_VI_READONLY("%s")
			" %dL, %dC", fn,
			(readonly_mode ? " [Readonly]" : ""),
			li, ch);
		if (ch > 0) {
			// if the insert is before "dot" then we need to update
			if (q <= dot)
				dot += ch;
			file_modified++;
		}
	} else if (strncasecmp(cmd, "rewind", i) == 0) {	// rewind cmd line args
		if (file_modified && !useforce) {
			status_line_bold("No write since last change (:rewind! overrides)");
		} else {
			// reset the filenames to edit
			optind = fn_start - 1;
			editing = 0;
		}
#if ENABLE_FEATURE_VI_SET
	} else if (strncasecmp(cmd, "set", i) == 0) {	// set or clear features
#if ENABLE_FEATURE_VI_SETOPTS
		colon_set(args);
#endif /* FEATURE_VI_SETOPTS */
#endif /* FEATURE_VI_SET */
#if ENABLE_FEATURE_VI_SEARCH
	} else if (strncasecmp(cmd, "s", 1) == 0) {	// substitute a pattern with a replacement pattern
		if (colon_s(orig_buf, q, b, e))
			return;
#endif /* FEATURE_VI_SEARCH */
	} else if (strncasecmp(cmd, "version", i) == 0) {  // show software version
		status_line(BB_VER " " BB_BT);
	} else if (strncasecmp(cmd, "write", i) == 0  // write text to file
	        || strncasecmp(cmd, "wq", i) == 0
	        || strncasecmp(cmd, "wn", i) == 0
	        || strncasecmp(cmd, "x", i) == 0
	) {
		// is there a file name to write to?
		if (args[0]) {
			fn = args;
		}
#if ENABLE_FEATURE_VI_READONLY
		if (readonly_mode && !useforce) {
			status_line_bold("\"%s\" File is read only", fn);
			goto vc3;
		}
#endif
		// how many lines in text[]?
		li = count_lines(q, r);
		ch = r - q + 1;
#if ENABLE_FEATURE_VI_PAGING
		if (q == text && r == end - 1)
			li = TOTLINES();
#endif
		// see if file exists- if not, its just a new file request
		if (useforce) {
			// if "fn" is not write-able, chmod u+w
			// sprintf(syscmd, "chmod u+w %s", fn);
			// system(syscmd);
			forced = TRUE;
		}
		l = file_write(fn, q, r);
		if (useforce && forced) {
			// chmod u-w
			// sprintf(syscmd, "chmod u-w %s", fn);
			// system(syscmd);
			forced = FALSE;
		}
		if (l < 0) {
			if (l == -1)
				status_line_bold("\"%s\" %s", fn, strerror(errno));
		} else {
#if ENABLE_FEATURE_VI_PAGING
			if (q == text && r == end - 1)
				status_line("\"%s\" %dL, %ldC", fn, li, pg_bytes());
			else
#endif
			status_line("\"%s\" %dL, %dC", fn, li, l);
			if (q == text && r == end - 1 && l == ch) {
				file_modified = 0;
				last_file_modified = -1;
			}
			if ((cmd[0] == 'x' || cmd[1] == 'q' || cmd[1] == 'n' ||
			     cmd[0] == 'X' || cmd[1] == 'Q' || cmd[1] == 'N')
			     && l == ch) {
				editing = 0;
			}
		}
#if ENABLE_FEATURE_VI_READONLY
 vc3:;
#endif
#if ENABLE_FEATURE_VI_YANKMARK
	} else if (strncasecmp(cmd, "yank", i) == 0) {	// yank lines
		if (b < 0) {	// no addr given- use defaults
			q = begin_line(dot);	// assume .,. for the range
			r = end_line(dot);
		}
		text_yank(q, r, YDreg);
		li = count_lines(q, r);
		status_line("Yank %d lines (%d chars) into [%c]",
				li, reg[YDreg] ? (int)strlen(reg[YDreg]) : 0, what_reg());
#endif
	} else {
		// cmd unknown
		not_implemented(cmd);
	}
 vc1:
	dot = bound_dot(dot);	// make sure "dot" is valid
	return;
#if ENABLE_FEATURE_VI_SEARCH
colon_s_fail:
	status_line_bold(":s expression missing delimiters");
	return;
colon_no_regex:
	status_line_bold ("No previous regular expression");
#endif
}

#endif /* FEATURE_VI_COLON */
