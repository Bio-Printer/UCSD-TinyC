/* viscreen.c -- vi: the screen, the status line, the cursor */
#include "vi.h"

/* vt102 typical ESC sequence */
/* terminal standout start/normal ESC sequence */
#define SOlen (4)  //length of SO escape sequence
#ifdef __UCSD__
static const char SOs[] ALIGN1 = "\017\017\017\017";
static const char SOn[] ALIGN1 = "\016\016\016\016";
#else
static const char SOs[] ALIGN1 = "\033[7m";
static const char SOn[] ALIGN1 = "\033[0m";
#endif
/* terminal bell sequence */
static const char bell[] ALIGN1 = "\007";
/* Clear-end-of-line and Clear-end-of-screen ESC sequence */
static const char Ceol[] ALIGN1 = "\033[0K";
static const char Ceos[] ALIGN1 = "\033[0J";
/* Cursor motion arbitrary destination ESC sequence */
static const char CMrc[] ALIGN1 = "\033[%d;%dH";
#ifdef ENABLE_FEATURE_VI_OPTIMIZE_CURSOR
/* Cursor motion up and down ESC sequence */
static const char CMup[] ALIGN1 = "\033[A";
static const char CMdown[] ALIGN1 = "\n";
#endif


static const char *cmd_mode_indicator[] =
	{"COMMAND", "INSERT", "REPLACE", "?!?" };

static void sync_cursor(char *d, int *row, int *col);
static const char *scompare(const char *s, const char *ref);
static void show_status_line(void);
static void print_literal(char *buf, const char *s);
static char* format_line(char *src);

void Hit_Return(void)
{
	char c;

	standout_start();
	write1("[Hit return to continue]");
	standout_end();
	while ((c = get_one_char()) != '\n' && c != '\r' && c != 27)
		continue;
	redraw();
}

int next_tabstop(int col)
{
	return col + ((tabstop - 1) - (col % tabstop));
}

//----- Synchronize the cursor to Dot --------------------------
static void sync_cursor(char *d, int *row, int *col)
{
	char *beg_cur;	// begin and end of "d" line
	char *tp;
	int cnt, ro, co;

	beg_cur = begin_line(d);	// first char of cur line

#if ENABLE_FEATURE_VI_PAGING
	if (pg_top) {
		// the screen's top line went out of the window: the same moves,
		// counted in line numbers
		ro = ABSLINE(beg_cur);
		co = TOTLINES();
		cnt = pg_top + rows - 2;	// the screen's last line
		if (cnt > co)
			cnt = co;
		if (ro < pg_top && pg_top - ro + 1 > (rows - 1) / 2
		 || ro > cnt && ro - cnt + 1 > (rows - 1) / 2)
			ro -= (rows - 1) / 2;	// dot in the middle of the screen
		else if (ro > cnt)
			ro = pg_top + ro - cnt;	// scrolled up just enough
		else if (ro > pg_top)
			ro = pg_top;		// on the screen as it was
		pg_top = 0;
		screenbegin = pg_line(ro < 1 ? 1 : ro);
		if (!screenbegin)
			screenbegin = text;
	} else
#endif
	if (beg_cur < screenbegin) {
		// "d" is before top line on screen
		// how many lines do we have to move
		cnt = count_lines(beg_cur, screenbegin);
 sc1:
		screenbegin = beg_cur;
		if (cnt > (rows - 1) / 2) {
			// we moved too many lines. put "dot" in middle of screen
			for (cnt = 0; cnt < (rows - 1) / 2; cnt++) {
				screenbegin = prev_line(screenbegin);
			}
		}
	} else {
		char *end_scr;	// begin and end of screen
		end_scr = end_screen();	// last char of screen
		if (beg_cur > end_scr) {
			// "d" is after bottom line on screen
			// how many lines do we have to move
			cnt = count_lines(end_scr, beg_cur);
			if (cnt > (rows - 1) / 2)
				goto sc1;	// too many lines
			for (ro = 0; ro < cnt - 1; ro++) {
				// move screen begin the same amount
				screenbegin = next_line(screenbegin);
				// now, move the end of screen
				end_scr = next_line(end_scr);
				end_scr = end_line(end_scr);
			}
		}
	}
	// "d" is on screen- find out which row
	tp = screenbegin;
	for (ro = 0; ro < rows - 1; ro++) {	// drive "ro" to correct row
		if (tp == beg_cur)
			break;
		tp = next_line(tp);
	}

	// find out what col "d" is on
	co = 0;
	while (tp < d) { // drive "co" to correct column
		if (*tp == '\n') //vda || *tp == '\0')
			break;
		if (*tp == '\t') {
			// handle tabs like real vi
			if (d == tp && (cmd_mode & CMODES) != CMODE_COMMAND) {
				break;
			}
			co = next_tabstop(co);
		} else if ((unsigned char)*tp < ' ' || *tp == 0x7f) {
			co++; // display as ^X, use 2 columns
		}
		co++;
		tp++;
	}

	// "co" is the column where "dot" is.
	// The screen has "columns" columns.
	// The currently displayed columns are  0+offset -- columns+ofset
	// |-------------------------------------------------------------|
	//               ^ ^                                ^
	//        offset | |------- columns ----------------|
	//
	// If "co" is already in this range then we do not have to adjust offset
	//      but, we do have to subtract the "offset" bias from "co".
	// If "co" is outside this range then we have to change "offset".
	// If the first char of a line is a tab the cursor will try to stay
	//  in column 7, but we have to set offset to 0.

	if (co < 0 + offset) {
		offset = co;
	}
	if (co >= columns + offset) {
		offset = co - columns + 1;
	}
	// if the first char of the line is a tab, and "dot" is sitting on it
	//  force offset to 0.
	if (d == beg_cur && *d == '\t') {
		offset = 0;
	}
	co -= offset;

	*row = ro;
	*col = co;
}


//----- Terminal Drawing ---------------------------------------
// The terminal is made up of 'rows' line of 'columns' columns.
// classically this would be 24 x 80.
//  screen coordinates
//  0,0     ...     0,79
//  1,0     ...     1,79
//  .       ...     .
//  .       ...     .
//  22,0    ...     22,79
//  23,0    ...     23,79   <- status line

//----- Move the cursor to row x col (count from 0, not 1) -------
#ifndef __UCSD__
void place_cursor(int row, int col, int optimize)
{
	char cm1[sizeof(CMrc) + sizeof(int)*3 * 2];
	char *cm;

	if (row < 0) row = 0;
	if (row >= rows) row = rows - 1;
	if (col < 0) col = 0;
	if (col >= columns) col = columns - 1;

	//----- 1.  Try the standard terminal ESC sequence
	sprintf(cm1, CMrc, row + 1, col + 1);
	cm = cm1;

#if ENABLE_FEATURE_VI_OPTIMIZE_CURSOR
	if (optimize && col < 16) {
		enum {
			SZ_UP = sizeof(CMup),
			SZ_DN = sizeof(CMdown),
			SEQ_SIZE = SZ_UP > SZ_DN ? SZ_UP : SZ_DN,
		};
		char cm2[SEQ_SIZE * 5 + 32]; // bigger than worst case size
		char *screenp;
		int Rrow = last_row;
		int diff = Rrow - row;

		if (diff < -5 || diff > 5)
			goto skip;

		//----- find the minimum # of chars to move cursor -------------
		//----- 2.  Try moving with discreet chars (Newline, [back]space, ...)
		cm2[0] = '\0';

		// move to the correct row
		while (row < Rrow) {
			// the cursor has to move up
			strcat(cm2, CMup);
			Rrow--;
		}
		while (row > Rrow) {
			// the cursor has to move down
			strcat(cm2, CMdown);
			Rrow++;
		}

		// now move to the correct column
		strcat(cm2, "\r");			// start at col 0
		// just send out orignal source char to get to correct place
		screenp = &screen[row * columns];	// start of screen line
		strncat(cm2, screenp, col);

		// pick the shortest cursor motion to send out
		if (strlen(cm2) < strlen(cm)) {
			cm = cm2;
		}
 skip: ;
	}
	last_row = row;
#endif /* FEATURE_VI_OPTIMIZE_CURSOR */
	write1(cm);
}
#endif

//----- Erase from cursor to end of line -----------------------
#ifndef __UCSD__
void clear_to_eol(void)
{
	write1(Ceol);   // Erase from cursor to end of line
}
#endif

//----- Erase from cursor to end of screen -----------------------
#ifndef __UCSD__
void clear_to_eos(void)
{
	write1(Ceos);   // Erase from cursor to end of screen
	*displayed_buffer=0;   //status line was also cleared
}
#endif

//----- Start standout mode ------------------------------------
void standout_start(void) // send "start reverse video" sequence
{
	write1(SOs);     // Start reverse video mode
}

//----- End standout mode --------------------------------------
void standout_end(void) // send "end reverse video" sequence
{
	write1(SOn);     // End reverse video mode
}

//----- Flash the screen  --------------------------------------
void flash(int h)
{
	standout_start();	// send "start reverse video" sequence
	redraw();
	awaitInput(h);
	standout_end();		// send "end reverse video" sequence
	redraw();
}

void Indicate_Error(void)
{
	if (!err_method) {
		write1(bell);   // send out a bell character
	} else {
		flash(10);
	}
}

//----- Screen[] Routines --------------------------------------
//----- Erase the Screen[] memory ------------------------------
void screen_erase(void)
{
#ifdef VI_ROW_SUMS
	memset(screen, 1, screensize);	// odd: no row's sum (they are even)
#else
	memset(screen, ' ', screensize);	// clear new screen
#endif
}

static const char *scompare(const char *s, const char *ref)
// why isn't this in the ANSI 'C' library?
{
	while(*s && *s == *ref)
		s++, ref++;
	return *s == *ref ? NULL : s;
}

//----- Draw the status line at bottom of the screen -------------
static void show_status_line(void)
{
	// either we already have an error or status message, or we
	// create one.
	const char *buffer = status_buffer;
	if (!*buffer)
	  format_edit_status(EDIT_STATUS);
	const char *changed = scompare(buffer, displayed_buffer);
	if (changed) {
		size_t unchanged = changed - buffer;
    	strcpy(displayed_buffer+unchanged, changed);
    	//place cursor on correct column if line begins with standout text
		size_t escapes = *buffer == *SOs ? 2*SOlen : 0;
		place_cursor(rows - 1,	// put cursor on status line
			escapes ? unchanged - SOlen : unchanged, FALSE);
		clear_to_eol(); //NOTE: assumes entire status text was in stand-out mode
		if (unchanged && escapes)   //need to start in stand-out mode
			fwrite(buffer, SOlen, 1, stdout);
		size_t len = unchanged + strlen(buffer=changed);
		if (len - escapes > columns) {
			const char *limit = status_buffer + columns;
			if (escapes)
				limit += SOlen;
			fwrite(buffer, limit-buffer, 1, stdout);
			buffer = status_buffer + len;
			if (escapes && len > 2*SOlen) //end w/restore to normal ESC sequence
				buffer-=SOlen;
		}
		write1(buffer);  //this leaves cursor correctly placed for LINE_INPUT
		if (!(cmd_mode & CMODE_LINE_INPUT))
			place_cursor(crow, ccol, TRUE); //otherwise, replace it in text area
	}else if (cmd_mode & CMODE_LINE_INPUT)  //in case status area was correct
		place_cursor(rows-1, strlen(buffer), FALSE);  //put cursor at its end
	else  //put cursor back in text area
		place_cursor(crow, ccol, TRUE);
}

//----- format the status buffer, the bottom line of screen ------
// format status buffer, with STANDOUT mode
void status_line_bold(const char *format, ...)
{
	va_list args;

	va_start(args, format);
	strcpy(status_buffer, SOs);	// Terminal standout mode on
	vsprintf(status_buffer + sizeof(SOs)-1, format, args);
	strcat(status_buffer, SOn);	// Terminal standout mode off
	va_end(args);
}

// format status buffer
void status_line(const char *format, ...)
{
	va_list args;

	va_start(args, format);
	vsprintf(status_buffer, format, args);
	va_end(args);
}

// copy s to buf, convert unprintable
static void print_literal(char *buf, const char *s)
{
	unsigned char c;
	char b[2];

	b[1] = '\0';
	buf[0] = '\0';
	if (!s[0])
		s = "(NULL)";
	for (; *s; s++) {
		int c_is_no_print;

		c = *s;
		c_is_no_print = (c & 0x80) && !Isprint(c);
		if (c_is_no_print) {
			strcat(buf, SOn);
			c = '.';
		}
		if (c < ' ' || c == 127) {
			strcat(buf, "^");
			if (c == 127)
				c = '?';
			else
				c += '@';
		}
		b[0] = c;
		strcat(buf, b);
		if (c_is_no_print)
			strcat(buf, SOs);
		if (*s == '\n')
			strcat(buf, "$");
		if (strlen(buf) > MAX_INPUT_LEN - 10) // paranoia
			break;
	}
}

void not_implemented(const char *s)
{
	char buf[MAX_INPUT_LEN];

	print_literal(buf, s);
	status_line_bold("\'%s\' is not implemented", buf);
}

// show file status on status line
int format_edit_status(const char *fmt)
{
#define tot format_edit_status__tot

	int cur, percent, ret, trunc_at;

	// file_modified is now a counter rather than a flag.  this
	// helps reduce the amount of line counting we need to do.
	// (this will cause a mis-reporting of modified status
	// once every MAXINT editing operations.)

	// it would be nice to do a similar optimization here -- if
	// we haven't done a motion that could have changed which line
	// we're on, then we shouldn't have to do this count_lines()
	cur = ABSLINE(dot);

	// reduce counting -- the total lines can't have
	// changed if we haven't done any edits.
	if (file_modified != last_file_modified) {
		tot = TOTLINES();
		last_file_modified = file_modified;
	}

	//    current line         percent
	//   -------------    ~~ ----------
	//    total lines            100
	if (tot > 0) {
		percent = (int)(100L * cur / tot);
	} else {
		cur = tot = 0;
		percent = 100;
	}

	trunc_at = columns < STATUS_BUFFER_LEN-1 ?
		columns : STATUS_BUFFER_LEN-1;

	ret = snprintf(status_buffer, trunc_at+1, fmt,
		cmd_mode_indicator[cmd_mode & CMODES],
		(current_filename != NULL ? current_filename : "No file"),
#if ENABLE_FEATURE_VI_READONLY
		(readonly_mode ? " [Readonly]" : ""),
#endif
		(file_modified ? " [Modified]" : ""),
		cur, tot, percent, offset+ccol+1, cmdcnt);

	if (ret >= 0 && ret < trunc_at)
		return ret;  /* it all fit */

	return trunc_at;  /* had to truncate */
#undef tot
}

//----- Force refresh of all Lines -----------------------------
void redraw(void)
{
	clear_screen();		//clear teminal screen and our image of it
	screen_erase();
	refresh();
}

//----- Format a text[] line into a buffer ---------------------
static char* format_line(char *src /*, int li*/)
{
	unsigned char c;
	int co;
	int ofs = offset;
	char *dest = scr_out_buf; // [MAX_SCR_COLS + MAX_TABSTOP * 2]

	c = '~'; // char in col 0 in non-existent lines is '~'
	co = 0;
	while (co < columns + tabstop) {
		// have we gone past the end?
		if (src < end) {
			c = *src++;
			if (c == '\n')
				break;
			if ((c & 0x80) && !Isprint(c)) {
				c = '.';
			}
			if (c < ' ' || c == 0x7f) {
				if (c == '\t') {
					c = ' ';
					//      co %    8     !=     7
					while ((co % tabstop) != (tabstop - 1)) {
						dest[co++] = c;
					}
				} else {
					dest[co++] = '^';
					if (c == 0x7f)
						c = '?';
					else
						c += '@'; // Ctrl-X -> 'X'
				}
			}
		}
		dest[co++] = c;
		// discard scrolled-off-to-the-left portion,
		// in tabstop-sized pieces
		if (ofs >= tabstop && co >= tabstop) {
			memmove(dest, dest + tabstop, co);
			co -= tabstop;
			ofs -= tabstop;
		}
		if (src >= end)
			break;
	}
	// check "short line, gigantic offset" case
	if (co < ofs)
		ofs = co;
	// discard last scrolled off part
	co -= ofs;
	dest += ofs;
	// fill the rest with spaces
	if (co < columns)
		memset(&dest[co], ' ', columns - co);
	return dest;
}

//----- Refresh the changed screen lines -----------------------
// Copy the source line from text[] into the buffer and note
// if the current screenline is different from the new buffer.
// If they differ then that line needs redrawing on the terminal.
//
void refresh(void)
{
#define old_offset refresh__old_offset

	int li, changed;
	char *tp, *sp;		// pointer into text[] and screen[]

	// poll to see if there is input already waiting. if we are
	// not able to display output fast enough to keep up, skip
	// the display update until we catch up with input.
	if (chars_to_parse || awaitInput(0))
		return;

	sync_cursor(dot, &crow, &ccol);	// where cursor will be (on "dot")
	tp = screenbegin;	// index into text[] of top line

	// compare text[] to screen[] and mark screen[] lines that need updating
	for (li = 0; li < rows - 1 && !awaitInput(0); li++) {
		int cs, ce;				// column start & end
		char *out_buf;
		// format current text line
		out_buf = format_line(tp /*, li*/);

		// skip to the end of the current text[] line
		if (tp < end) {
			char *t = memchr(tp, '\n', end - tp);
			if (!t) t = end - 1;
			tp = t + 1;
		}

#ifdef VI_ROW_SUMS
		// Memory is small: the screen is kept as a sum per row, and a row
		// whose sum changed is written again, up to its last non-blank
		{
			unsigned sum, *rs;
			int n;
			sum = 0;
			for (n = 0; n < columns; n++)
				sum = sum * 31 + (unsigned char)out_buf[n];
			sum &= ~1;
			rs = (unsigned *)screen + li;
			if (sum != *rs || offset != old_offset) {
				*rs = sum;
				for (n = columns; n > 0 && out_buf[n - 1] == ' '; n--)
					;
				place_cursor(li, 0, TRUE);
				fwrite(out_buf, n, 1, stdout);
				if (n < columns)
					clear_to_eol();
			}
			continue;
		}
#endif
		// see if there are any changes between vitual screen and out_buf
		changed = FALSE;	// assume no change
		cs = 0;
		ce = columns - 1;
		sp = &screen[li * columns];	// start of screen line
		// compare newly formatted buffer with virtual screen
		// look forward for first difference between buf and screen
		for (; cs <= ce; cs++) {
			if (out_buf[cs] != sp[cs]) {
				changed = TRUE;	// mark for redraw
				break;
			}
		}

		// look backward for last difference between out_buf and screen
		for (; ce >= cs; ce--) {
			if (out_buf[ce] != sp[ce]) {
				changed = TRUE;	// mark for redraw
				break;
			}
		}
		// now, cs is index of first diff, and ce is index of last diff

		// if horz offset has changed, force a redraw
		if (offset != old_offset) {
			changed = TRUE;
		}

		// make a sanity check of columns indexes
		if (cs < 0) cs = 0;
		if (ce > columns - 1) ce = columns - 1;
		if (cs > ce) { cs = 0; ce = columns - 1; }
		// is there a change between virtual screen and out_buf
		if (changed) {
			// copy changed part of buffer to virtual screen
			memcpy(sp+cs, out_buf+cs, ce-cs+1);

			// move cursor to column of first change
			//if (offset != old_offset) {
			//	// place_cursor is still too stupid
			//	// to handle offsets correctly
			//	place_cursor(li, cs, FALSE);
			//} else {
				place_cursor(li, cs, TRUE);
			//}

			// write line out to terminal
			fwrite(&sp[cs], ce - cs + 1, 1, stdout);
		}
	}

	place_cursor(crow, ccol, TRUE);

	old_offset = offset;
#undef old_offset
	show_status_line();
}

//---------------------------------------------------------------------
//----- the Ascii Chart -----------------------------------------------
//
//  00 nul   01 soh   02 stx   03 etx   04 eot   05 enq   06 ack   07 bel
//  08 bs    09 ht    0a nl    0b vt    0c np    0d cr    0e so    0f si
//  10 dle   11 dc1   12 dc2   13 dc3   14 dc4   15 nak   16 syn   17 etb
//  18 can   19 em    1a sub   1b esc   1c fs    1d gs    1e rs    1f us
//  20 sp    21 !     22 "     23 #     24 $     25 %     26 &     27 '
//  28 (     29 )     2a *     2b +     2c ,     2d -     2e .     2f /
//  30 0     31 1     32 2     33 3     34 4     35 5     36 6     37 7
//  38 8     39 9     3a :     3b ;     3c <     3d =     3e >     3f ?
//  40 @     41 A     42 B     43 C     44 D     45 E     46 F     47 G
//  48 H     49 I     4a J     4b K     4c L     4d M     4e N     4f O
//  50 P     51 Q     52 R     53 S     54 T     55 U     56 V     57 W
//  58 X     59 Y     5a Z     5b [     5c \     5d ]     5e ^     5f _
//  60 `     61 a     62 b     63 c     64 d     65 e     66 f     67 g
//  68 h     69 i     6a j     6b k     6c l     6d m     6e n     6f o
//  70 p     71 q     72 r     73 s     74 t     75 u     76 v     77 w
//  78 x     79 y     7a z     7b {     7c |     7d }     7e ~     7f del
//---------------------------------------------------------------------
