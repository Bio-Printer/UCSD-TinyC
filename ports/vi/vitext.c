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

	if (pg_f)
		return pg_goto(li);
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
	pg_edge(FORWARD);
	dot = next_line(dot);
}

void dot_prev(void)
{
	pg_edge(BACK);
	dot = prev_line(dot);
}

void dot_scroll(int cnt, int dir)
{
	char *q;

	cnt -= pg_scroll(cnt, dir);	// (moved over as many lines as the window allows)
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
	screensize = ro * (sizeof(unsigned) + sizeof(int));
	screen = xmalloc(screensize);
	screen_erase();
	return screen;
}

int mycmp(const char *s1, const char *s2, int len)
{
	int i;

	i = strncmp(s1, s2, len);
	if (ignorecase) {
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

char *char_insert(char *p, char c) // insert the char c at 'p'
{
	if (c == 22) {		// Is this an ctrl-V?
		p = stupid_insert(p, '^');	// use ^ to indicate literal next
		p--;			// backup onto ^
		refresh();		// show the ^
		c = get_one_char();
		undo_lines(c == '\n');
		*p = c;
		p++;
		(file_modified++, scr_gen++);
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
	}
	return p;
}

static char *stupid_insert(char *p, char c) // stupidly insert the char c at 'p'
{
	char *q;

	int g = scr_gen;

	q = text_hole_make(p, 1);
	if (!q)
		return p;	// no room: the character is dropped
	p = q;
	*p = c;
	undo_lines(c == '\n');
	//(file_modified++, scr_gen++); - done by text_hole_make()
	if (c != '\n' && scr_gen == g + 1) {
		scr_gen = g;		// only the row of this line changes
		scr_soft_edit(p);
	}
	return p + 1;
}

int find_range(char **start, char **stop, char c)
{
	char *save_dot, *p, *q, *t;
	int cnt, multiline = 0, gl;

	// G, gg (the caller read the second g): the lines to the last line, the
	// first one, or line cmdcnt
	gl = c == 'G' && cmdcnt == 0 ? TOTLINES() : cmdcnt > 0 ? cmdcnt : 1;
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
		else if (c == 'G' || c == 'g')
			ok = pg_hold(l, gl);
		if (!ok) {
			pg_over = 1;
			*start = *stop = dot;
			return 0;
		}
		pg_lock++;
	}
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
	} else if (c == 'G' || c == 'g') {
		// whole lines, from this one to line gl (its lines are held)
		q = find_line(gl);
		if (q < dot) {
			p = begin_line(q);
			q = end_line(dot);
		} else {
			p = begin_line(dot);
			q = end_line(q);
		}
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
	pg_lock--;
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
	if (pg_room() < size) {
		p = pg_makeroom(p, size);
		if (!p)
			return NULL;
	}
	undo_mod(p, p, size);
	end += size;		// adjust the new END
	if (end >= (text + text_size)) {
		char *new_text;
		text_size += end - (text + text_size) + TEXT_SLACK;
		new_text = xrealloc(text, text_size);
		screenbegin = new_text + (screenbegin - text);
		dot         = new_text + (dot         - text);
		end         = new_text + (end         - text);
		p           = new_text + (p           - text);
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
		text = new_text;
	}
	memmove(p + size, p, end - size - p);
	memset(p, ' ', size);	// clear new hole
	(file_modified++, scr_gen++);
	return p;
}

//  close a hole in text[]
char *text_hole_delete(char *p, char *q) // delete "p" through "q", inclusive
{
	char *src, *dest;
	int cnt, hole_size, g, soft;

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
	g = scr_gen;
	soft = dest >= text && dest + hole_size <= end && !memchr(dest, '\n', hole_size);
	if (src < text || src > end)
		goto thd0;
	if (dest < text || dest >= end)
		goto thd0;
	undo_mod(dest, dest + hole_size - 1, -hole_size);
	undo_lines(-pg_count(dest, dest + hole_size));
	if (src >= end)
		goto thd_atend;	// just delete the end of the buffer
	memmove(dest, src, cnt);
 thd_atend:
	end = end - hole_size;	// adjust the new END
	// deleted to the window's end: the lines after it in (if they fit
	// without moving what callers point at; else yank_delete does it)
	pg_atend = dest >= end && PG_MORE() && !pg_fill_bottom();
	if (dest >= end)
		dest = end - 1;	// make sure dest in below end-1
	if (end <= text)
		dest = end = text;	// keep pointers valid
	(file_modified++, scr_gen++);
	if (soft && dest < end && scr_gen == g + 1) {
		scr_gen = g;		// only the row of this line changes
		scr_soft_edit(dest);
	}
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
	text_yank(start, stop, YDreg);
	if (yf == YANKDEL) {
		p = text_hole_delete(start, stop);
		if (pg_atend)
			p = pg_next_in(end);	// at the window's end: the next line in
	}					// delete lines
	return p;
}

void start_new_cmd_q(char c)
{
	// get buffer for new cmd
	// if there is a current cmd count put it in the buffer first
	if (cmdcnt > 0)
		lmc_len = snprintf(last_modifying_cmd, MAX_INPUT_LEN, "%d%c", cmdcnt, c);
	else { // just save char c onto queue
		last_modifying_cmd[0] = c;
		lmc_len = 1;
	}
	adding2q = 1;
}

void end_cmd_q(void)
{
	YDreg = 26;			// go back to default Yank/Delete reg
	adding2q = 0;
}

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
	undo_lines(cnt);
	status_line("Put %d lines (%d chars) from [%c]", cnt, i, what_reg());
	return p;
}

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
#ifdef __UCSD__
	t = ucsd_room(cnt + 1) ? malloc(cnt + 1) : NULL;	// (a save keeps its room)
#else
	t = malloc(cnt + 1);	// get a new register
#endif
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

/* ---- u: the last change undone, and u again redoes it ----
   A change (one command, with the text an insert types) touches the lines ua .. ua+un-1 as
   they are now (u_cur bytes); u_old holds what they were, whole lines (u_len bytes).  Lines
   are numbers (the window moves); a line comes into the record before it is first changed,
   and the change keeps un and u_cur counting the lines and bytes it makes and takes away.  u
   swaps u_old with those lines.  A change too big for memory, or one whose lines leave the
   window, has no undo. */
static char *u_old;
static int u_len, ua, un, u_cur;
static int u_on;		// 1: a record; 0: none; -1: the change could not be kept
static int u_busy;		// (u itself changes the text)
int u_new;

static char *u_alloc(int n)
{
#ifdef __UCSD__
	return ucsd_room(n) ? malloc(n) : NULL;	// (a save keeps its room)
#else
	return malloc(n);
#endif
}

/* the start of the line after p's (end after the last; next_line stops at its newline) */
static char *u_next(char *p)
{
	p = end_line(p);
	return p < end ? p + 1 : end;
}

/* the record grows by a .. b-1 (in the window): before it, or after it */
static void u_add(char *a, char *b, int before)
{
	char *t;
	int n;
	n = b - a;
	if (n <= 0)
		return;
	t = u_alloc(u_len + n);
	if (!t) {
		undo_reset();
		u_on = -1;
		return;
	}
	if (before) {
		memcpy(t, a, n);
		memcpy(t + n, u_old, u_len);
	} else {
		memcpy(t, u_old, u_len);
		memcpy(t + u_len, a, n);
	}
	free(u_old);
	u_old = t;
	u_len += n;
	u_cur += n;
}

void undo_reset(void)
{
	free(u_old);
	u_old = NULL;
	u_len = un = u_cur = 0;
	u_on = 0;
	u_new = 1;
}

void undo_mod(char *p, char *q, int d)
{
	int x, y, k;
	char *a;
	if (u_busy)
		return;
	if (u_new) {
		undo_reset();
		u_new = 0;
		u_on = 1;
		ua = pg_abs(p);
	}
	if (u_on <= 0)
		return;
	if (q >= end)
		q = end - 1;
	x = pg_abs(p);
	y = q < p ? x - 1 : x + pg_count(p, begin_line(q));	// (at the end: no line yet)
	if (un == 0 && u_len == 0 && x != ua) {
		ua = x;		// (nothing kept yet: the record starts here)
	}
	if (x < ua) {		// lines x .. ua-1 before it
		a = begin_line(p);
		for (k = ua - x; k > 0 && a < end; k--)
			a = u_next(a);
		if (k > 0)
			goto lost;
		u_add(begin_line(p), a, 1);
		un += ua - x;
		ua = x;
	}
	if (u_on > 0 && y >= ua + un) {	// lines ua+un .. y after it
		a = begin_line(q);
		for (k = y - (ua + un); k > 0 && a > text; k--)
			a = prev_line(a);
		if (k > 0)
			goto lost;
		u_add(a, u_next(q), 0);
		un = y - ua + 1;
	}
	u_cur += d;
	return;
 lost:
	undo_reset();
	u_on = -1;
	u_new = 0;
}

void undo_lines(int d)
{
	if (!u_busy && u_on > 0)
		un += d;
}

/* u itself: a code segment of its own, in memory only while u runs (vi's resident code is
   memory the window does not have) */
#ifdef __UCSD__
#pragma segment VIUNDO
#endif

/* the start of line li, brought into the window; end if li is after the last line */
static char *u_line(int li)
{
	int k;
	k = TOTLINES();
	if (li <= k)
		return find_line(li);
	if (k > 0)
		find_line(k);	// (the last line in: nothing after the window)
	return end;
}

/* s (len bytes) in at line ua, a piece at a time (a hole the window can make) */
static void u_put(char *s, int len)
{
	char *p, *q;
	int i, k;
	p = u_line(ua);
	for (i = 0; i < len; i += k) {
		k = len - i < 256 ? len - i : 256;
		q = text_hole_make(p, k);
		if (!q)
			break;
		memcpy(q, s + i, k);
		p = q + k;
	}
}

void undo_cmd(void)
{
	char *p, *q, *t;
	int n, tl, i;
	if (u_on <= 0) {
		status_line_bold(u_on ? "Cannot undo: the change was too big" : "Nothing to undo");
		return;
	}
	t = u_alloc(u_cur + 1);		// (the room it leaves, a save's, holds this segment and its calls)
	if (!t) {
		status_line_bold("Cannot undo: no memory");
		return;
	}
	n = pg_count(u_old, u_old + u_len);	// (the lines u puts back)
	u_busy = 1;
	/* the lines there now (un, u_cur bytes) out, a line at a time (the window moves): the
	   record for u again */
	tl = 0;
	for (i = 0; i < un; i++) {
		p = u_line(ua);
		q = u_next(p);
		if (p >= end || tl + (q - p) > u_cur)
			break;
		memcpy(t + tl, p, q - p);
		tl += q - p;
		text_hole_delete(p, q - 1);
	}
	u_put(u_old, u_len);		/* the record's lines in */
	u_busy = 0;
	/* what u took out is the record now: u again puts it back */
	free(u_old);
	u_old = t;
	u_cur = u_len;
	u_len = tl;
	un = n;
	p = u_line(ua);
	dot = p < end ? p : end > text ? begin_line(end - 1) : text;
}
#ifdef __UCSD__
#pragma segment MAIN
#endif
