/* vitext.c -- vi: moving in and changing the text, searching, registers */
#include "vi.h"

static char *stupid_insert(char *p, char c);
static int st_test(char *p, int type, int dir, char *tested);

//----- Text Movement Routines ---------------------------------
char *begin_line(char *p) // return pointer to first char cur line
{
	if (p > text) {
		p = memrchr(text, '\n', p - text);
		if (!p)
			return text;
		return p + 1;
	}
	return p;
}

char *end_line(char *p) // return pointer to NL of cur line
{
	if (p < end - 1) {
		p = memchr(p, '\n', end - p - 1);
		if (!p)
			return end - 1;
	}
	return p;
}

char *dollar_line(char *p) // return pointer to just before NL line
{
	p = end_line(p);
	// Try to stay off of the Newline
	if (*p == '\n' && (p - begin_line(p)) > 0)
		p--;
	return p;
}

char *prev_line(char *p) // return pointer first char prev line
{
	p = begin_line(p);	// goto begining of cur line
	if (p > text && p[-1] == '\n')
		p--;			// step to prev line
	p = begin_line(p);	// goto begining of prev line
	return p;
}

char *next_line(char *p) // return pointer first char next line
{
	p = end_line(p);
	if (p < end - 1 && *p == '\n')
		p++;			// step to next line
	return p;
}

//----- Text Information Routines ------------------------------
char *end_screen(void)
{
	char *q;
	int cnt;

	// find new bottom line
	q = screenbegin;
	for (cnt = 0; cnt < rows - 2; cnt++)
		q = next_line(q);
	q = end_line(q);
	return q;
}

// count line from start to stop
int count_lines(char *start, char *stop)
{
	char *q;
	int cnt;

	if (stop < start) { // start and stop are backwards- reverse them
		q = start;
		start = stop;
		stop = q;
	}
	cnt = 0;
	stop = end_line(stop);
	while (start <= stop && start <= end - 1) {
		start = end_line(start);
		if (*start == '\n')
			cnt++;
		start++;
	}
	return cnt;
}

char *find_line(int li)	// find begining of line #li
{
	char *q;

#if ENABLE_FEATURE_VI_PAGING
	if (pg_f)
		return pg_goto(li);
#endif
	for (q = text; li > 1; li--) {
		q = next_line(q);
	}
	return q;
}

//----- Dot Movement Routines ----------------------------------
void dot_left(void)
{
	if (dot > text && dot[-1] != '\n')
		dot--;
}

void dot_right(void)
{
	if (dot < end - 1 && *dot != '\n')
		dot++;
}

void dot_begin(void)
{
	dot = begin_line(dot);	// return pointer to first char cur line
}

void dot_end(void)
{
	dot = end_line(dot);	// return pointer to last char cur line
}

char *move_to_col(char *p, int l)
{
	int co;

	p = begin_line(p);
	co = 0;
	while (co < l && p < end) {
		if (*p == '\n') //vda || *p == '\0')
			break;
		if (*p == '\t') {
			co = next_tabstop(co);
		} else if (*p < ' ' || *p == 127) {
			co++; // display as ^X, use 2 columns
		}
		co++;
		p++;
	}
	return p;
}

void dot_next(void)
{
#if ENABLE_FEATURE_VI_PAGING
	pg_edge(FORWARD);
#endif
	dot = next_line(dot);
}

void dot_prev(void)
{
#if ENABLE_FEATURE_VI_PAGING
	pg_edge(BACK);
#endif
	dot = prev_line(dot);
}

void dot_scroll(int cnt, int dir)
{
	char *q;

#if ENABLE_FEATURE_VI_PAGING
	pg_scroll(cnt, dir);
#endif
	for (; cnt > 0; cnt--) {
		if (dir < 0) {
			// scroll Backwards
			// ctrl-Y scroll up one line
			screenbegin = prev_line(screenbegin);
		} else {
			// scroll Forwards
			// ctrl-E scroll down one line
			screenbegin = next_line(screenbegin);
		}
	}
	// make sure "dot" stays on the screen so we dont scroll off
	if (dot < screenbegin)
		dot = screenbegin;
	q = end_screen();	// find new bottom line
	if (dot > q)
		dot = begin_line(q);	// is dot is below bottom line?
	dot_skip_over_ws();
}

void dot_skip_over_ws(void)
{
	// skip WS
	while (isspace(*dot) && *dot != '\n' && dot < end - 1)
		dot++;
}

void dot_delete(void)	// delete the char at 'dot'
{
	text_hole_delete(dot, dot);
}

char *bound_dot(char *p) // make sure  text[0] <= P < "end"
{
	if (p >= end && end > text) {
		p = end - 1;
		indicate_error('1');
	}
	if (p < text) {
		p = text;
		indicate_error('2');
	}
	return p;
}

//----- Helper Utility Routines --------------------------------

//----------------------------------------------------------------
//----- Char Routines --------------------------------------------
/* Chars that are part of a word-
 *    0123456789_ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz
 * Chars that are Not part of a word (stoppers)
 *    !"#$%&'()*+,-./:;<=>?@[\]^`{|}~
 * Chars that are WhiteSpace
 *    TAB NEWLINE VT FF RETURN SPACE
 * DO NOT COUNT NEWLINE AS WHITESPACE
 */

char *new_screen(int ro, int co)
{
	free(screen);
#ifdef VI_ROW_SUMS
	screensize = ro * sizeof(unsigned);
#else
	screensize = ro * co + 8;
#endif
	screen = xmalloc(screensize);
	screen_erase();
	return screen;
}

#if ENABLE_FEATURE_VI_SEARCH
int mycmp(const char *s1, const char *s2, int len)
{
	int i;

	i = strncmp(s1, s2, len);
	if (ENABLE_FEATURE_VI_SETOPTS && ignorecase) {
		i = strncasecmp(s1, s2, len);
	}
	return i;
}

// search for pattern starting at p
char *char_search(char *p, const char *pat, int dir, int range)
{
#ifndef REGEX_SEARCH
	char *start, *stop;
	int len;

	len = strlen(pat);
	if (dir == FORWARD) {
		stop = end - 1;	// assume range is p - end-1
		if (range == LIMITED)
			stop = next_line(p);	// range is to next line
		for (start = p; start < stop; start++) {
			if (mycmp(start, pat, len) == 0) {
				return start;
			}
		}
	} else if (dir == BACK) {
		stop = text;	// assume range is text - p
		if (range == LIMITED)
			stop = prev_line(p);	// range is to prev line
		for (start = p - len; start >= stop; start--) {
			if (mycmp(start, pat, len) == 0) {
				return start;
			}
		}
	}
	// pattern not found
	return NULL;
#else /* REGEX_SEARCH */
	char *q;
	struct re_pattern_buffer preg;
	int i;
	int size, range;

	re_syntax_options = RE_SYNTAX_POSIX_EXTENDED;
	preg.translate = 0;
	preg.fastmap = 0;
	preg.buffer = 0;
	preg.allocated = 0;

	// assume a LIMITED forward search
	q = next_line(p);
	q = end_line(q);
	q = end - 1;
	if (dir == BACK) {
		q = prev_line(p);
		q = text;
	}
	// count the number of chars to search over, forward or backward
	size = q - p;
	if (size < 0)
		size = p - q;
	// RANGE could be negative if we are searching backwards
	range = q - p;

	q = re_compile_pattern(pat, strlen(pat), &preg);
	if (q != 0) {
		// The pattern was not compiled
		status_line_bold("bad search pattern: \"%s\": %s", pat, q);
		i = 0;			// return p if pattern not compiled
		goto cs1;
	}

	q = p;
	if (range < 0) {
		q = p - size;
		if (q < text)
			q = text;
	}
	// search for the compiled pattern, preg, in p[]
	// range < 0-  search backward
	// range > 0-  search forward
	// 0 < start < size
	// re_search() < 0  not found or error
	// re_search() > 0  index of found pattern
	//            struct pattern    char     int    int    int     struct reg
	// re_search (*pattern_buffer,  *string, size,  start, range,  *regs)
	i = re_search(&preg, q, size, 0, range, 0);
	if (i == -1) {
		p = 0;
		i = 0;			// return NULL if pattern not found
	}
 cs1:
	if (dir == FORWARD) {
		p = p + i;
	} else {
		p = p - i;
	}
	return p;
#endif /* REGEX_SEARCH */
}
#endif /* FEATURE_VI_SEARCH */

char *char_insert(char *p, char c) // insert the char c at 'p'
{
	if (c == 22) {		// Is this an ctrl-V?
		p = stupid_insert(p, '^');	// use ^ to indicate literal next
		p--;			// backup onto ^
		refresh();		// show the ^
		c = get_one_char();
		*p = c;
		p++;
		file_modified++;
	} else if (c == 27) {	// Is this an ESC?
		cmd_mode = CMODE_COMMAND;
		cmdcnt = 0;
		end_cmd_q();	// stop adding to q
		if ((p[-1] != '\n') && (dot > text)) {
			p--;
		}
	} else if (c == erase_char || c == 8 || c == 127) { // Is this a BS
		//     123456789
		if ((p[-1] != '\n') && (dot>text)) {
			p--;
			p = text_hole_delete(p, p);	// shrink buffer 1 char
		}
	} else {
		// insert a char into text[]
		char *sp;		// "save p"

		if (c == 13)
			c = '\n';	// translate \r to \n
		sp = p;			// remember addr of insert
		p = stupid_insert(p, c);	// insert the char
#if ENABLE_FEATURE_VI_SETOPTS
		if (showmatch && strchr(")]}", *sp) != NULL) {
			showmatching(sp);
		}
		if (autoindent && c == '\n') {	// auto indent the new line
			char *q;

			q = prev_line(p);	// use prev line as templet
			for (; isblank(*q); q++) {
				p = stupid_insert(p, *q);	// insert the char
			}
		}
#endif
	}
	return p;
}

static char *stupid_insert(char *p, char c) // stupidly insert the char c at 'p'
{
	char *q;

	q = text_hole_make(p, 1);
	if (!q)
		return p;	// no room: the character is dropped
	p = q;
	*p = c;
	//file_modified++; - done by text_hole_make()
	return p + 1;
}

int find_range(char **start, char **stop, char c)
{
	char *save_dot, *p, *q, *t;
	int cnt, multiline = 0;

#if ENABLE_FEATURE_VI_PAGING
	{
		int n, l, ok;
		n = cmdcnt > 1 ? cmdcnt : 1;
		l = ABSLINE(dot);
		ok = 1;
		pg_over = 0;
		if (strchr("cdy><", c))
			ok = pg_hold(l, l + n - 1);
		else if (strchr("L+j}\r\n", c))
			ok = pg_hold(l, l + n);
		else if (strchr("H-k{", c))
			ok = pg_hold(l - n, l);
		if (!ok) {
			pg_over = 1;
			*start = *stop = dot;
			return 0;
		}
		pg_lock++;
	}
#endif
	save_dot = dot;
	p = q = dot;

	if (strchr("cdy><", c)) {
		// these cmds operate on whole lines
		p = q = begin_line(p);
		for (cnt = 1; cnt < cmdcnt; cnt++) {
			q = next_line(q);
		}
		q = end_line(q);
	} else if (strchr("^%$0bBeEfth\b\177", c)) {
		// These cmds operate on char positions
		do_cmd(c);		// execute movement cmd
		q = dot;
	} else if (strchr("wW", c)) {
		do_cmd(c);		// execute movement cmd
		// if we are at the next word's first char
		// step back one char
		// but check the possibilities when it is true
		if (dot > text && ((isspace(dot[-1]) && !isspace(dot[0]))
				|| (ispunct(dot[-1]) && !ispunct(dot[0]))
				|| (isalnum(dot[-1]) && !isalnum(dot[0]))))
			dot--;		// move back off of next word
		if (dot > text && *dot == '\n')
			dot--;		// stay off NL
		q = dot;
	} else if (strchr("H-k{", c)) {
		// these operate on multi-lines backwards
		q = end_line(dot);	// find NL
		do_cmd(c);		// execute movement cmd
		dot_begin();
		p = dot;
	} else if (strchr("L+j}\r\n", c)) {
		// these operate on multi-lines forwards
		p = begin_line(dot);
		do_cmd(c);		// execute movement cmd
		dot_end();		// find NL
		q = dot;
	} else {
	    // nothing -- this causes any other values of c to
	    // represent the one-character range under the
	    // cursor.  this is correct for ' ' and 'l', but
	    // perhaps no others.
	    //
	}
	if (q < p) {
		t = q;
		q = p;
		p = t;
	}

	// backward char movements don't include start position
	if (q > p && strchr("^0bBh\b\177", c)) q--;

	multiline = 0;
	for (t = p; t <= q; t++) {
		if (*t == '\n') {
			multiline = 1;
			break;
		}
	}

	*start = p;
	*stop = q;
	dot = save_dot;
#if ENABLE_FEATURE_VI_PAGING
	pg_lock--;
#endif
	return multiline;
}

static int st_test(char *p, int type, int dir, char *tested)
{
	char c, c0, ci;
	int test, inc;

	inc = dir;
	c = c0 = p[0];
	ci = p[inc];
	test = 0;

	if (type == S_BEFORE_WS) {
		c = ci;
		test = ((!isspace(c)) || c == '\n');
	}
	if (type == S_TO_WS) {
		c = c0;
		test = ((!isspace(c)) || c == '\n');
	}
	if (type == S_OVER_WS) {
		c = c0;
		test = ((isspace(c)));
	}
	if (type == S_END_PUNCT) {
		c = ci;
		test = ((ispunct(c)));
	}
	if (type == S_END_ALNUM) {
		c = ci;
		test = ((isalnum(c)) || c == '_');
	}
	*tested = c;
	return test;
}

char *skip_thing(char *p, int linecnt, int dir, int type)
{
	char c;

	while (st_test(p, type, dir, &c)) {
		// make sure we limit search to correct number of lines
		if (c == '\n' && --linecnt < 1)
			break;
		if (dir >= 0 && p >= end - 1)
			break;
		if (dir < 0 && p <= text)
			break;
		p += dir;		// move to next char
	}
	return p;
}

// find matching char of pair  ()  []  {}
char *find_pair(char *p, const char c)
{
	char match, *q;
	int dir, level;

	match = ')';
	level = 1;
	dir = 1;			// assume forward
	switch (c) {
	case '(': match = ')'; break;
	case '[': match = ']'; break;
	case '{': match = '}'; break;
	case ')': match = '('; dir = -1; break;
	case ']': match = '['; dir = -1; break;
	case '}': match = '{'; dir = -1; break;
	}
	for (q = p + dir; text <= q && q < end; q += dir) {
		// look for match, count levels of pairs  (( ))
		if (*q == c)
			level++;	// increase pair levels
		if (*q == match)
			level--;	// reduce pair level
		if (level == 0)
			break;		// found matching pair
	}
	if (level != 0)
		q = NULL;		// indicate no match
	return q;
}

//  open a hole in text[]
char *text_hole_make(char *p, int size)	// at "p", make a 'size' byte hole
{
	if (size <= 0)
		return p;
#if ENABLE_FEATURE_VI_PAGING
	if (pg_room() < size) {
		p = pg_makeroom(p, size);
		if (!p)
			return NULL;
	}
#endif
	end += size;		// adjust the new END
	if (end >= (text + text_size)) {
		char *new_text;
		text_size += end - (text + text_size) + TEXT_SLACK;
		new_text = xrealloc(text, text_size);
		screenbegin = new_text + (screenbegin - text);
		dot         = new_text + (dot         - text);
		end         = new_text + (end         - text);
		p           = new_text + (p           - text);
#if ENABLE_FEATURE_VI_YANKMARK
		{
			int k;
			for (k = 0; k < 28; k++)
				if (mark[k])
					mark[k] = new_text + (mark[k] - text);
			if (context_start)
				context_start = new_text + (context_start - text);
			if (context_end)
				context_end = new_text + (context_end - text);
			edit_file__cur_line = NULL;
		}
#endif
		text = new_text;
	}
	memmove(p + size, p, end - size - p);
	memset(p, ' ', size);	// clear new hole
	file_modified++;
	return p;
}

//  close a hole in text[]
char *text_hole_delete(char *p, char *q) // delete "p" through "q", inclusive
{
	char *src, *dest;
	int cnt, hole_size;

	// move forwards, from beginning
	// assume p <= q
	src = q + 1;
	dest = p;
	if (q < p) {		// they are backward- swap them
		src = p + 1;
		dest = q;
	}
	hole_size = q - p + 1;
	cnt = end - src;
	if (src < text || src > end)
		goto thd0;
	if (dest < text || dest >= end)
		goto thd0;
	if (src >= end)
		goto thd_atend;	// just delete the end of the buffer
	memmove(dest, src, cnt);
 thd_atend:
	end = end - hole_size;	// adjust the new END
#if ENABLE_FEATURE_VI_PAGING
	// deleted to the window's end: the lines after it in (if they fit
	// without moving what callers point at; else yank_delete does it)
	pg_atend = dest >= end && pg_na && !pg_fill_bottom();
#endif
	if (dest >= end)
		dest = end - 1;	// make sure dest in below end-1
	if (end <= text)
		dest = end = text;	// keep pointers valid
	file_modified++;
 thd0:
	return dest;
}

// copy text into register, then delete text.
// if dist <= 0, do not include, or go past, a NewLine
//
char *yank_delete(char *start, char *stop, int dist, int yf)
{
	char *p;

	// make sure start <= stop
	if (start > stop) {
		// they are backwards, reverse them
		p = start;
		start = stop;
		stop = p;
	}
	if (dist <= 0) {
		// we cannot cross NL boundaries
		p = start;
		if (*p == '\n')
			return p;
		// dont go past a NewLine
		for (; p + 1 <= stop; p++) {
			if (p[1] == '\n') {
				stop = p;	// "stop" just before NewLine
				break;
			}
		}
	}
	p = start;
#if ENABLE_FEATURE_VI_YANKMARK
	text_yank(start, stop, YDreg);
#endif
	if (yf == YANKDEL) {
		p = text_hole_delete(start, stop);
#if ENABLE_FEATURE_VI_PAGING
		if (pg_atend)
			p = pg_next_in(end);	// at the window's end: the next line in
#endif
	}					// delete lines
	return p;
}

#ifndef __UCSD__
void show_help(void)
{
	puts("These features are available:"
#if ENABLE_FEATURE_VI_SEARCH
	"\n\tPattern searches with / and ?"
#endif
#if ENABLE_FEATURE_VI_DOT_CMD
	"\n\tLast command repeat with \'.\'"
#endif
#if ENABLE_FEATURE_VI_YANKMARK
	"\n\tLine marking with 'x"
	"\n\tNamed buffers with \"x"
#endif
#if ENABLE_FEATURE_VI_READONLY
#ifdef NO_SUCH_APPLET_YET
	"\n\tReadonly if vi is called as \"view\""
#endif
	"\n\tReadonly with -R command line arg"
#endif
#if ENABLE_FEATURE_VI_SET
	"\n\tSome colon mode commands with \':\'"
#endif
#if ENABLE_FEATURE_VI_SETOPTS
	"\n\tSettable options with \":set\""
#endif
#if ENABLE_FEATURE_VI_USE_SIGNALS
	"\n\tSignal catching- ^C"
	"\n\tJob suspend and resume with ^Z"
#endif
	"\n\tLINES and COLUMNS env vars determine window size"
#if ENABLE_FEATURE_VI_WIN_RESIZE
	"\n\tAdapt to window re-sizes (if LINES and COLUMNS env vars unset!)"
#endif
	);
}
#endif

#if ENABLE_FEATURE_VI_DOT_CMD
void start_new_cmd_q(char c)
{
	// get buffer for new cmd
	// if there is a current cmd count put it in the buffer first
	if (cmdcnt > 0)
		lmc_len = sprintf(last_modifying_cmd, "%d%c", cmdcnt, c);
	else { // just save char c onto queue
		last_modifying_cmd[0] = c;
		lmc_len = 1;
	}
	adding2q = 1;
}

void end_cmd_q(void)
{
#if ENABLE_FEATURE_VI_YANKMARK
	YDreg = 26;			// go back to default Yank/Delete reg
#endif
	adding2q = 0;
}
#endif /* FEATURE_VI_DOT_CMD */

#if ENABLE_FEATURE_VI_YANKMARK \
 || (ENABLE_FEATURE_VI_COLON && ENABLE_FEATURE_VI_SEARCH)
char *string_insert(char *p, char *s) // insert the string at 'p'
{
	int cnt, i;
	char *s2;

	i = strlen(s);
	s2 = text_hole_make(p, i);
	if (!s2)
		return p;	// no room: nothing put
	p = s2;
	strncpy(p, s, i);
	for (cnt = 0; *s != '\0'; s++) {
		if (*s == '\n')
			cnt++;
	}
#if ENABLE_FEATURE_VI_YANKMARK
	status_line("Put %d lines (%d chars) from [%c]", cnt, i, what_reg());
#endif
	return p;
}
#endif

#if ENABLE_FEATURE_VI_YANKMARK
char *text_yank(char *p, char *q, int dest)	// copy text into a register
{
	char *t;
	int cnt;

	if (q < p) {		// they are backwards- reverse them
		t = q;
		q = p;
		p = t;
	}
	cnt = q - p + 1;
	t = reg[dest];
	free(t);		//  if already a yank register, free it
	t = malloc(cnt + 1);	// get a new register
	reg[dest] = t;
	if (!t) {
		status_line_bold("Too much to yank for memory");
		return p;
	}
	memset(t, '\0', cnt + 1);	// clear new text[]
	strncpy(t, p, cnt);	// copy text[] into bufer
	reg[dest] = t;
	return p;
}

char what_reg(void)
{
	char c;

	c = 'D';			// default to D-reg
	if (0 <= YDreg && YDreg <= 25)
		c = 'a' + (char) YDreg;
	if (YDreg == 26)
		c = 'D';
	if (YDreg == 27)
		c = 'U';
	return c;
}

void check_context(char cmd)
{
	// A context is defined to be "modifying text"
	// Any modifying command establishes a new context.

#if ENABLE_FEATURE_VI_PAGING
	int l = ABSLINE(dot);
	if (l < ctx_s || l > ctx_e) {
		if (strchr(modifying_cmds, cmd) != NULL) {
			markl[27] = markl[26];
			markl[26] = l;
			ctx_s = ABSLINE(prev_line(prev_line(dot)));
			ctx_e = ABSLINE(next_line(next_line(dot)));
		}
	}
	return;
#endif
	if (dot < context_start || dot > context_end) {
		if (strchr(modifying_cmds, cmd) != NULL) {
			// we are trying to modify text[]- make this the current context
			mark[27] = mark[26];	// move cur to prev
			mark[26] = dot;	// move local to cur
			context_start = prev_line(prev_line(dot));
			context_end = next_line(next_line(dot));
			//loiter= start_loiter= now;
		}
	}
}

char *swap_context(char *p) // goto new context for '' command make this the current context
{
	char *tmp;

	// the current context is in mark[26]
	// the previous context is in mark[27]
	// only swap context if other context is valid
#if ENABLE_FEATURE_VI_PAGING
	int t;
	if (markl[27] >= 1 && markl[27] <= TOTLINES()) {
		t = markl[27];
		markl[27] = markl[26];
		markl[26] = t;
		p = find_line(t);
		ctx_s = t > 3 ? t - 3 : 1;
		ctx_e = t + 3;
	}
	return p;
#endif
	if (text <= mark[27] && mark[27] <= end - 1) {
		tmp = mark[27];
		mark[27] = mark[26];
		mark[26] = tmp;
		p = mark[26];	// where we are going- previous context
		context_start = prev_line(prev_line(prev_line(p)));
		context_end = next_line(next_line(next_line(p)));
	}
	return p;
}
#endif /* FEATURE_VI_YANKMARK */
