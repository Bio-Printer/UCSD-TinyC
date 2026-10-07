/* vicmd.c -- vi: the vi commands */
#include "vi.h"

static int do_cmd2(char *cp);

//----- Execute a Vi Command -----------------------------------
// the second half of do_cmd()'s commands: returns 0 when done, 1 to
// repeat (cmdcnt), 2 to do *cp again, 4 when do_cmd must return at once
static int do_cmd2(char *cp)
{
#define c (*cp)
	const char *msg;
	char c1, *p, *q, *save_dot;
	char buf[12];
	int dir, cnt, i, j;

	switch (c) {
	default:			// unrecognised command
		buf[0] = c;
		buf[1] = '\0';
		if (c < ' ') {
			buf[0] = '^';
			buf[1] = c + '@';
			buf[2] = '\0';
		}
		not_implemented(buf);
		end_cmd_q();	// stop adding to q
		break;
	case '0':			// 0- goto begining of line
	case '1':			// 1-
	case '2':			// 2-
	case '3':			// 3-
	case '4':			// 4-
	case '5':			// 5-
	case '6':			// 6-
	case '7':			// 7-
	case '8':			// 8-
	case '9':			// 9-
		if (c == '0' && cmdcnt < 1) {
			dot_begin();	// this was a standalone zero
		} else {
			if (cmdcnt >= INT_MAX/10) {
				cmdcnt = 0;
				status_line_bold("Repeat Count OVERFLOW");
				return 4;
			}
			cmdcnt = cmdcnt * 10 + (c - '0');	// this 0 is part of a number
			if (cmdcnt!=1)
			  format_edit_status(EDIT_STATUS " col %d {%d times}");
		}
		break;
	case ':':			// :- the colon mode commands
		p = get_input_line(":");	// get input line- use "status line"
#if ENABLE_FEATURE_VI_COLON
		colon(p);		// execute the command
#else
		if (*p == ':')
			p++;				// move past the ':'
		cnt = strlen(p);
		if (cnt <= 0)
			break;
		if (strncasecmp(p, "quit", cnt) == 0
		 || strncasecmp(p, "q!", cnt) == 0   // delete lines
		) {
			if (file_modified && p[1] != '!') {
				status_line_bold("No write since last change (:quit! overrides)");
			} else {
				editing = 0;
			}
		} else if (strncasecmp(p, "write", cnt) == 0
		        || strncasecmp(p, "wq", cnt) == 0
		        || strncasecmp(p, "wn", cnt) == 0
		        || strncasecmp(p, "x", cnt) == 0
		) {
			cnt = file_write(current_filename, text, end - 1);
			if (cnt < 0) {
				if (cnt == -1)
					status_line_bold("Write error: %s", strerror(errno));
			} else {
				file_modified = 0;
				last_file_modified = -1;
				status_line("\"%s\" %dL, %dC", current_filename, count_lines(text, end - 1), cnt);
				if (p[0] == 'x' || p[1] == 'q' || p[1] == 'n'
				 || p[0] == 'X' || p[1] == 'Q' || p[1] == 'N'
				) {
					editing = 0;
				}
			}
		} else if (strncasecmp(p, "file", cnt))
		  if (sscanf(p, "%d", &j) > 0) {
			dot = find_line(j);		// go to line # j
			dot_skip_over_ws();
		} else {		// unrecognised cmd
			not_implemented(p);
		}
#endif /* !FEATURE_VI_COLON */
		break;
	case '<':			// <- Left  shift something
	case '>':			// >- Right shift something
		cnt = ABSLINE(dot);	// remember what line we are on
		c1 = get_one_char();	// get the type of thing to delete
		find_range(&p, &q, c1);
#if ENABLE_FEATURE_VI_PAGING
		if (pg_over) {
			status_line_bold("Too many lines for memory");
			end_cmd_q();
			break;
		}
#endif
		yank_delete(p, q, 1, YANKONLY);	// save copy before change
		p = begin_line(p);
		q = end_line(q);
		i = count_lines(p, q);	// # of lines we are shifting
		for ( ; i > 0; i--, p = next_line(p)) {
			if (c == '<') {
				// shift left- remove tab or 8 spaces
				if (*p == '\t') {
					// shrink buffer 1 char
					text_hole_delete(p, p);
				} else if (*p == ' ') {
					// we should be calculating columns, not just SPACE
					for (j = 0; *p == ' ' && j < tabstop; j++) {
						text_hole_delete(p, p);
					}
				}
			} else if (c == '>') {
				// shift right -- add tab or 8 spaces
				char_insert(p, '\t');
			}
		}
		dot = find_line(cnt);	// what line were we on
		dot_skip_over_ws();
		end_cmd_q();	// stop adding to q
		break;
	case 'A':			// A- append at e-o-l
		dot_end();		// go to e-o-l
		//**** fall through to ... 'a'
	case 'a':			// a- append after current char
		if (*dot != '\n')
			dot++;
		goto dc_i;
		break;
	case 'B':			// B- back a blank-delimited Word
	case 'E':			// E- end of a blank-delimited word
	case 'W':			// W- forward a blank-delimited word
		dir = FORWARD;
		if (c == 'B')
			dir = BACK;
		if (c == 'W' || isspace(dot[dir])) {
			dot = skip_thing(dot, 1, dir, S_TO_WS);
			dot = skip_thing(dot, 2, dir, S_OVER_WS);
		}
		if (c != 'W')
			dot = skip_thing(dot, 1, dir, S_BEFORE_WS);
		return 1;
	case 'C':			// C- Change to e-o-l
	case 'D':			// D- delete to e-o-l
		save_dot = dot;
		dot = dollar_line(dot);	// move to before NL
		// copy text into a register and delete
		dot = yank_delete(save_dot, dot, 0, YANKDEL);	// delete to e-o-l
		if (c == 'C')
			goto dc_i;	// start inserting
#if ENABLE_FEATURE_VI_DOT_CMD
		if (c == 'D')
			end_cmd_q();	// stop adding to q
#endif
		break;
	case 'g':                       // 'gg' goto a line number (from vim)
					// (default to first line in file)
		c1 = get_one_char();
		if (c1 != 'g') {
			buf[0] = 'g';
			buf[1] = c1;
			buf[2] = '\0';
			not_implemented(buf);
			break;
		}
		if (cmdcnt == 0)
			cmdcnt = 1;
		/* fall through */
	case 'G':		// G- goto to a line number (default= E-O-F)
#if ENABLE_FEATURE_VI_PAGING
		if (pg_f)
			pg_goto(TOTLINES());	// the window to the file's end
#endif
		dot = end - 1;				// assume E-O-F
		if (cmdcnt > 0) {
			dot = find_line(cmdcnt);	// what line is #cmdcnt
		}
		dot_skip_over_ws();
		break;
	case 'H':			// H- goto top line on screen
		dot = screenbegin;
		if (cmdcnt > (rows - 1)) {
			cmdcnt = (rows - 1);
		}
		if (cmdcnt-- > 1) {
			do_cmd('+');
		}				// repeat cnt
		dot_skip_over_ws();
		break;
	case 'I':			// I- insert before first non-blank
		dot_begin();	// 0
		dot_skip_over_ws();
		//**** fall through to ... 'i'
	case 'i':			// i- insert before current char
	case VI_K_INSERT:	// Cursor Key Insert
dc_i:
		cmd_mode = CMODE_INSERT;	// start insrting
		break;
	case 'J':			// J- join current and next lines together
		dot_end();		// move to NL
		if (dot < end - 1) {	// make sure not last char in text[]
			*dot++ = ' ';	// replace NL with space
			file_modified++;
			while (isblank(*dot)) {	// delete leading WS
				dot_delete();
			}
		}
		end_cmd_q();	// stop adding to q
		if (cmdcnt-- > 2)
			return 2;
		break;
	case 'L':			// L- goto bottom line on screen
		dot = end_screen();
		if (cmdcnt > (rows - 1)) {
			cmdcnt = (rows - 1);
		}
		if (cmdcnt-- > 1) {
			do_cmd('-');
		}				// repeat cnt
		dot_begin();
		dot_skip_over_ws();
		break;
	case 'M':			// M- goto middle line on screen
		dot = screenbegin;
		for (cnt = 0; cnt < (rows-1) / 2; cnt++)
			dot = next_line(dot);
		break;
	case 'O':			// O- open a empty line above
		//    0i\n ESC -i
		p = begin_line(dot);
		if (p[-1] == '\n') {
			dot_prev();
	case 'o':			// o- open a empty line below; Yes, I know it is in the middle of the "if (..."
			dot_end();
			dot = char_insert(dot, '\n');
		} else {
			dot_begin();	// 0
			dot = char_insert(dot, '\n');	// i\n ESC
			dot_prev();	// -
		}
		goto dc_i;
		break;
	case 'R':			// R- continuous Replace char
dc5:
		cmd_mode = CMODE_REPLACE;
		break;
	case VI_K_DELETE:
		c = 'x';
		// fall through
	case 'X':			// X- delete char before dot
	case 'x':			// x- delete the current char
	case 's':			// s- substitute the current char
		dir = 0;
		if (c == 'X')
			dir = -1;
		if (dot[dir] != '\n') {
			if (c == 'X')
				dot--;	// delete prev char
			dot = yank_delete(dot, dot, 0, YANKDEL);	// delete char
		}
		if (c == 's')
			goto dc_i;	// start insrting
		end_cmd_q();	// stop adding to q
		return 1;
	case 'Z':			// Z- if modified, {write}; exit
		// ZZ means to save file (if necessary), then exit
		c1 = get_one_char();
		if (c1 != 'Z') {
			indicate_error(c);
			break;
		}
		if (file_modified) {
			if (ENABLE_FEATURE_VI_READONLY && readonly_mode) {
				status_line_bold("\"%s\" File is read only", current_filename);
				break;
			}
			cnt = file_write(current_filename, text, end - 1);
			if (cnt < 0) {
				if (cnt == -1)
					status_line_bold("Write error: %s", strerror(errno));
			} else if (cnt == (end - 1 - text + 1)) {
				editing = 0;
			}
		} else {
			editing = 0;
		}
		break;
	case '^':			// ^- move to first non-blank on line
		dot_begin();
		dot_skip_over_ws();
		break;
	case 'b':			// b- back a word
	case 'e':			// e- end of word
		dir = FORWARD;
		if (c == 'b')
			dir = BACK;
		if ((dot + dir) < text || (dot + dir) > end - 1)
			break;
		dot += dir;
		if (isspace(*dot)) {
			dot = skip_thing(dot, (c == 'e') ? 2 : 1, dir, S_OVER_WS);
		}
		if (isalnum(*dot) || *dot == '_') {
			dot = skip_thing(dot, 1, dir, S_END_ALNUM);
		} else if (ispunct(*dot)) {
			dot = skip_thing(dot, 1, dir, S_END_PUNCT);
		}
		return 1;
	case 'c':			// c- change something
	case 'd':			// d- delete something
#if ENABLE_FEATURE_VI_YANKMARK
	case 'y':			// y- yank   something
	case 'Y':			// Y- Yank a line
#endif
		{
		int yf, ml, whole = 0;
		yf = YANKDEL;	// assume either "c" or "d"
#if ENABLE_FEATURE_VI_YANKMARK
		if (c == 'y' || c == 'Y')
			yf = YANKONLY;
#endif
		c1 = 'y';
		if (c != 'Y')
			c1 = get_one_char();	// get the type of thing to delete
		// determine range, and whether it spans lines
		ml = find_range(&p, &q, c1);
#if ENABLE_FEATURE_VI_PAGING
		if (pg_over) {
			status_line_bold("Too many lines for memory");
			end_cmd_q();
			break;
		}
#endif
		if (c1 == 27) {	// ESC- user changed mind and wants out
			c = c1 = 27;	// Escape- do nothing
		} else if (strchr("wW", c1)) {
			if (c == 'c') {
				// don't include trailing WS as part of word
				while (isblank(*q)) {
					if (q <= text || q[-1] == '\n')
						break;
					q--;
				}
			}
			dot = yank_delete(p, q, ml, yf);	// delete word
		} else if (strchr("^0bBeEft%$ lh\b\177", c1)) {
			// partial line copy text into a register and delete
			dot = yank_delete(p, q, ml, yf);	// delete word
		} else if (strchr("cdykjHL+-{}\r\n", c1)) {
			// whole line copy text into a register and delete
			dot = yank_delete(p, q, ml, yf);	// delete lines
			whole = 1;
		} else {
			// could not recognize object
			c = c1 = 27;	// error-
			ml = 0;
			indicate_error(c);
		}
		if (ml && whole) {
			if (c == 'c') {
				dot = char_insert(dot, '\n');
				// on the last line of file don't move to prev line
				if (whole && dot != (end-1)) {
					dot_prev();
				}
			} else if (c == 'd') {
				dot_begin();
				dot_skip_over_ws();
			}
		}
		if (c1 != 27) {
			// if CHANGING, not deleting, start inserting after the delete
			if (c == 'c') {
				strcpy(buf, "Change");
				goto dc_i;	// start inserting
			}
			if (c == 'd') {
				strcpy(buf, "Delete");
			}
#if ENABLE_FEATURE_VI_YANKMARK
			if (c == 'y' || c == 'Y') {
				strcpy(buf, "Yank");
			}
			p = reg[YDreg] ? reg[YDreg] : "";
			q = p + strlen(p);
			for (cnt = 0; p <= q; p++) {
				if (*p == '\n')
					cnt++;
			}
			status_line("%s %d lines (%d chars) using [%c]",
				buf, cnt, reg[YDreg] ? (int)strlen(reg[YDreg]) : 0, what_reg());
#endif
			end_cmd_q();	// stop adding to q
		}
		}
		break;
	case 'k':			// k- goto prev line, same col
	case VI_K_UP:		// cursor key Up
		dot_prev();
		dot = move_to_col(dot, ccol + offset);	// try stay in same col
		return 1;
	case 'r':			// r- replace the current char with user input
		c1 = get_one_char();	// get the replacement char
		if (*dot != '\n') {
			*dot = c1;
			file_modified++;
		}
		end_cmd_q();	// stop adding to q
		break;
	case 't':			// t- move to char prior to next x
		last_forward_char = get_one_char();
		do_cmd(';');
		if (*dot == last_forward_char)
			dot_left();
		last_forward_char= 0;
		break;
	case 'w':			// w- forward a word
		if (isalnum(*dot) || *dot == '_') {	// we are on ALNUM
			dot = skip_thing(dot, 1, FORWARD, S_END_ALNUM);
		} else if (ispunct(*dot)) {	// we are on PUNCT
			dot = skip_thing(dot, 1, FORWARD, S_END_PUNCT);
		}
		if (dot < end - 1)
			dot++;		// move over word
		if (isspace(*dot)) {
			dot = skip_thing(dot, 2, FORWARD, S_OVER_WS);
		}
		return 1;
	case 'z':			// z-
		c1 = get_one_char();	// get the replacement char
		cnt = 0;
		if (c1 == '.')
			cnt = (rows - 2) / 2;	// put dot at center
		if (c1 == '-')
			cnt = rows - 2;	// put dot at bottom
		screenbegin = begin_line(dot);	// start dot at top
		dot_scroll(cnt, -1);
		break;
	case '|':			// |- move to column "cmdcnt"
		dot = move_to_col(dot, cmdcnt - 1);	// try to move to column
		break;
	case '~':			// ~- flip the case of letters   a-z -> A-Z
		if (islower(*dot)) {
			*dot = toupper(*dot);
			file_modified++;
		} else if (isupper(*dot)) {
			*dot = tolower(*dot);
			file_modified++;
		}
		dot_right();
		end_cmd_q();	// stop adding to q
		return 1;
		//----- The Cursor and Function Keys -----------------------------
	case VI_K_HOME:	// Cursor Key Home
		dot_begin();
		break;
		// The Fn keys could point to do_macro which could translate them
	case VI_K_FUN1:	// Function Key F1
	case VI_K_FUN2:	// Function Key F2
	case VI_K_FUN3:	// Function Key F3
	case VI_K_FUN4:	// Function Key F4
	case VI_K_FUN5:	// Function Key F5
	case VI_K_FUN6:	// Function Key F6
	case VI_K_FUN7:	// Function Key F7
	case VI_K_FUN8:	// Function Key F8
	case VI_K_FUN9:	// Function Key F9
	case VI_K_FUN10:	// Function Key F10
	case VI_K_FUN11:	// Function Key F11
	case VI_K_FUN12:	// Function Key F12
		break;
	}
	return 0;
#undef c
}

void do_cmd(char c)
{
	const char *msg;
	char c1, *p, *q, *save_dot;
	char buf[12];
	int dir, cnt, i, j;

again:
#if ENABLE_FEATURE_VI_PAGING
	pg_fix();
#endif
	/* if this is a cursor key, skip these checks */
	switch (c) {
		case VI_K_UP:
		case VI_K_DOWN:
		case VI_K_LEFT:
		case VI_K_RIGHT:
		case VI_K_HOME:
		case VI_K_END:
		case VI_K_PAGEUP:
		case VI_K_PAGEDOWN:
			goto key_cmd_mode;
	}

	if (cmd_mode == CMODE_REPLACE) {
		//  flip-flop Insert/Replace mode
		if (c == VI_K_INSERT)
		{
			cmd_mode = CMODE_INSERT;
			goto dc1;
		}
		// we are 'R'eplacing the current *dot with new char
		if (*dot == '\n') {
			// don't Replace past E-o-l
			cmd_mode = CMODE_INSERT;	// convert to insert
		} else {
			if (1 <= c || Isprint(c)) {
				if (c != 27)
					dot = yank_delete(dot, dot, 0, YANKDEL);	// delete char
				dot = char_insert(dot, c);	// insert new char
			}
			goto dc1;
		}
	}
	if (cmd_mode == CMODE_INSERT) {
		//  hitting "Insert" twice means "R" replace mode
		if (c == VI_K_INSERT) {
			cmd_mode = CMODE_REPLACE;
			goto dc1;
		}
		// insert the char c at "dot"
		if (1 <= c || Isprint(c)) {
			dot = char_insert(dot, c);
		}
		goto dc1;
	}

 key_cmd_mode:
	switch (c) {
	default:			// the rest: do_cmd2()
		switch (do_cmd2(&c)) {
		case 1:
			goto repeat;
		case 2:
			goto again;
		case 4:
			return;
		}
		break;
		//case 0x01:	// soh
		//case 0x09:	// ht
		//case 0x0b:	// vt
		//case 0x0e:	// so
		//case 0x0f:	// si
		//case 0x10:	// dle
		//case 0x11:	// dc1
		//case 0x13:	// dc3
		//case 0x16:	// syn
		//case 0x17:	// etb
		//case 0x18:	// can
		//case 0x1c:	// fs
		//case 0x1d:	// gs
		//case 0x1e:	// rs
		//case 0x1f:	// us
		//case '!':	// !-
		//case '#':	// #-
		//case '&':	// &-
		//case '(':	// (-
		//case ')':	// )-
		//case '*':	// *-
		//case '=':	// =-
		//case '@':	// @-
		//case 'F':	// F-
		//case 'K':	// K-
		//case 'Q':	// Q-
		//case 'S':	// S-
		//case 'T':	// T-
		//case 'V':	// V-
		//case '[':	// [-
		//case '\\':	// \-
		//case ']':	// ]-
		//case '_':	// _-
		//case '`':	// `-
		//case 'u':	// u- FIXME- there is no undo
		//case 'v':	// v-
	case 0x00:			// nul- ignore
		break;
	case 2:			// ctrl-B  scroll up   full screen
	case VI_K_PAGEUP:	// Cursor Key Page Up
		dot_scroll(rows - 2, -1);
		break;
	case 4:			// ctrl-D  scroll down half screen
		dot_scroll((rows - 2) / 2, 1);
		break;
	case 5:			// ctrl-E  scroll down one line
		dot_scroll(1, 1);
		break;
	case 6:			// ctrl-F  scroll down full screen
	case VI_K_PAGEDOWN:	// Cursor Key Page Down
		dot_scroll(rows - 2, 1);
		break;
	case 7:			// ctrl-G  show current status
		format_edit_status(EDIT_STATUS " col %d");
		break;
	case 'h':			// h- move left
	case VI_K_LEFT:	// cursor key Left
	case 8:		// ctrl-H- move left    (This may be ERASE char)
	case 0x7f:	// DEL- move left   (This may be ERASE char)
		dot_left();
repeat:
		if (cmdcnt-- > 1)
			goto again;
		break;
	case 10:			// Newline ^J
	case 'j':			// j- goto next line, same col
	case VI_K_DOWN:	// cursor key Down
		dot_next();		// go to next B-o-l
		dot = move_to_col(dot, ccol + offset);	// try stay in same col
		goto repeat;
	case 12:			// ctrl-L  force redraw whole screen
	case 18:			// ctrl-R  force redraw
		createScreen();
		redraw();
		break;
	case 13:			// Carriage Return ^M
	case '+':			// +- goto next line
		dot_next();
		dot_skip_over_ws();
		goto repeat;
	case 21:			// ctrl-U  scroll up   half screen
		dot_scroll((rows - 2) / 2, -1);
		break;
	case 25:			// ctrl-Y  scroll up one line
		dot_scroll(1, -1);
		break;
	case 27:			// esc
		if (cmd_mode == CMODE_COMMAND)
			indicate_error(c);
		cmd_mode = CMODE_COMMAND;	// stop insrting
		end_cmd_q();
		break;
	case ' ':			// move right
	case 'l':			// move right
	case VI_K_RIGHT:	// Cursor Key Right
		dot_right();
		goto repeat;
#if ENABLE_FEATURE_VI_YANKMARK
	case '"':			// "- name a register to use for Delete/Yank
		c1 = get_one_char();
		c1 = tolower(c1);
		if (islower(c1)) {
			YDreg = c1 - 'a';
		} else {
			indicate_error(c);
		}
		break;
	case '\'':			// '- goto a specific mark
		c1 = get_one_char();
		c1 = tolower(c1);
		if (islower(c1)) {
			c1 = c1 - 'a';
			// get the b-o-l
#if ENABLE_FEATURE_VI_PAGING
			if (markl[(unsigned char) c1]) {
				dot = find_line(markl[(unsigned char) c1]);
				dot_skip_over_ws();
			}
#else
			q = mark[(unsigned char) c1];
			if (text <= q && q < end) {
				dot = q;
				dot_begin();	// go to B-o-l
				dot_skip_over_ws();
			}
#endif
		} else if (c1 == '\'') {	// goto previous context
			dot = swap_context(dot);	// swap current and previous context
			dot_begin();	// go to B-o-l
			dot_skip_over_ws();
		} else {
			indicate_error(c);
		}
		break;
	case 'm':			// m- Mark a line
		// this is really stupid.  If there are any inserts or deletes
		// between text[0] and dot then this mark will not point to the
		// correct location! It could be off by many lines!
		// Well..., at least its quick and dirty.
		c1 = get_one_char();
		c1 = tolower(c1);
		if (islower(c1)) {
			c1 = c1 - 'a';
			// remember the line
			mark[(int) c1] = dot;
#if ENABLE_FEATURE_VI_PAGING
			markl[(int) c1] = ABSLINE(dot);
#endif
		} else {
			indicate_error(c);
		}
		break;
	case 'P':			// P- Put register before
	case 'p':			// p- put register after
		p = reg[YDreg];
		if (p == 0) {
			status_line_bold("Nothing in register %c", what_reg());
			end_cmd_q();	// not a command to repeat
			break;
		}
		// are we putting whole lines or strings
		if (strchr(p, '\n') != NULL) {
			if (c == 'P') {
				dot_begin();	// putting lines- Put above
			}
			if (c == 'p') {
				// are we putting after very last line?
				if (end_line(dot) == (end - 1)) {
					dot = end;	// force dot to end of text[]
				} else {
					dot_next();	// next line, then put before
				}
			}
		} else {
			if (c == 'p')
				dot_right();	// move to right, can move to NL
		}
		dot = string_insert(dot, p);	// insert the string
		end_cmd_q();	// stop adding to q
		break;
	case 'U':			// U- Undo; replace current line with original version
		if (reg[Ureg] != 0) {
			p = begin_line(dot);
			q = end_line(dot);
			p = text_hole_delete(p, q);	// delete cur line
			p = string_insert(p, reg[Ureg]);	// insert orig line
			dot = p;
			dot_skip_over_ws();
		}
		break;
#endif /* FEATURE_VI_YANKMARK */
	case '$':			// $- goto end of line
	case VI_K_END:		// Cursor Key End
		dot = end_line(dot);
		goto repeat;
	case '%':			// %- find matching char of pair () [] {}
		for (q = dot; q < end && *q != '\n'; q++) {
			if (strchr("()[]{}", *q) != NULL) {
				// we found half of a pair
				p = find_pair(q, *q);
				if (p == NULL) {
					indicate_error(c);
				} else {
					dot = p;
				}
				break;
			}
		}
		if (*q == '\n')
			indicate_error(c);
		break;
	case 'f':			// f- forward to a user specified char
		last_forward_char = get_one_char();	// get the search char
		//
		// dont separate these two commands. 'f' depends on ';'
		//
		//**** fall through to ... ';'
	case ';':			// ;- look at rest of line for last forward char
		if (last_forward_char == 0)
			break;
		q = dot + 1;
		while (q < end - 1 && *q != '\n' && *q != last_forward_char) {
			q++;
		}
		if (*q == last_forward_char)
			dot = q;
		c = ';'; goto repeat;
	case ',':           // repeat latest 'f' in opposite direction
		if (last_forward_char == 0)
			break;
		q = dot - 1;
		while (q >= text && *q != '\n' && *q != last_forward_char) {
			q--;
		}
		if (q >= text && *q == last_forward_char)
			dot = q;
		c = ','; goto repeat;

	case '-':			// -- goto prev line
		dot_prev();
		dot_skip_over_ws();
		goto repeat;
#if ENABLE_FEATURE_VI_DOT_CMD
	case '.':			// .- repeat the last modifying command
		// Stuff the last_modifying_cmd back into stdin
		// and let it be re-executed.
		if (adding2q) {
			// a command that failed is still being recorded, and
			// now holds this '.': repeating it would repeat for ever
			end_cmd_q();
			lmc_len = 0;
			indicate_error(c);
			break;
		}
		if (lmc_len > 0) {
			last_modifying_cmd[lmc_len] = 0;
			ioq = ioq_start = xstrdup(last_modifying_cmd);
		}
		break;
#endif
#if ENABLE_FEATURE_VI_SEARCH
	case '?':			// /- search for a pattern
	case '/':			// /- search for a pattern
		buf[0] = c;
		buf[1] = '\0';
		q = get_input_line(buf);	// get input line- use "status line"
		if (!*q)
			break;	// bail out if user erased the entire pattern
		if (q[1]) { // strlen(q) > 1: new pat- save it and find
			free(last_search_pattern);
			last_search_pattern = xstrdup(q);
		    goto findNormal;	// new pattern determines search direction
		}  //Reuse pattern. If c=='?', search in direction opposite pattern's
		if (c == '/')
			goto findNormal;
		c = 'N';
	case 'N':		 // N- repeat search in opposite direction of last pattern
		dir = BACK;  //BACK here means "opposite" pattern's spec'd direction
		goto findPattern;

findNormal:
		c = 'n';
	case 'n':		// n- repeat search for last pattern in "normal" direction
		// search rest of text[] starting at next char
		// if search fails return orignal "p" not the "p+1" address
		dir = FORWARD; //in pattern's spec'd direction
findPattern:
		if (!last_search_pattern) {
			msg = "No previous regular expression";
			goto dc2;
		}  //derive absolute search direction from that relative to pattern's
		if (*last_search_pattern == '?')
			dir = -dir;  //relies on FORWARD/BACK being 1/-1, respectively
		p = dot + dir;
		q = char_search(p, last_search_pattern + 1, dir, FULL);
#if ENABLE_FEATURE_VI_PAGING
		if (q == NULL)		// on beyond the window
			q = pg_search(last_search_pattern + 1, dir, 0);
#endif
		if (q != NULL) {
			dot = q;	// good search, update "dot"
			goto repeat;
		}
		// no pattern found between "dot" and "end"- continue at top
#if ENABLE_FEATURE_VI_PAGING
		q = pg_search(last_search_pattern + 1, dir, 1);
		if (q == NULL) {	// the stores on the other side, then the window
#endif
		p = text;
		if (dir == BACK) {
			p = end - 1;
		}
		q = char_search(p, last_search_pattern + 1, dir, FULL);
#if ENABLE_FEATURE_VI_PAGING
		}
#endif
		if (q != NULL) {	// found something
			dot = q;	// found new pattern- goto it
			msg = "search hit BOTTOM, continuing at TOP";
			if (dir == BACK)
				msg = "search hit TOP, continuing at BOTTOM";
		} else
			msg = "Pattern not found";
dc2:
		status_line_bold("%s", msg);
		break;
	case '{':			// {- move backward paragraph
		q = char_search(dot, "\n\n", BACK, FULL);
#if ENABLE_FEATURE_VI_PAGING
		if (q == NULL)
			q = pg_search("\n\n", BACK, 0);
#endif
		if (q != NULL) {	// found blank line
			dot = next_line(q);	// move to next blank line
		}
		break;
	case '}':			// }- move forward paragraph
		q = char_search(dot, "\n\n", FORWARD, FULL);
#if ENABLE_FEATURE_VI_PAGING
		if (q == NULL)
			q = pg_search("\n\n", FORWARD, 0);
#endif
		if (q != NULL) {	// found blank line
			dot = next_line(q);	// move to next blank line
		}
		break;
#endif /* FEATURE_VI_SEARCH */
	}

dc1:
	// if text[] just became empty, add back an empty line
	if (end == text) {
		char_insert(text, '\n');	// start empty buf with dummy line
		dot = text;
	}
	// it is OK for dot to exactly equal to end, otherwise check dot validity
	if (dot != end) {
		dot = bound_dot(dot);	// make sure "dot" is valid
	}
#if ENABLE_FEATURE_VI_YANKMARK
	check_context(c);	// update the current context
#endif

	if (!isdigit(c))
		cmdcnt = 0;		// cmd was not a number, reset cmdcnt
	cnt = dot - begin_line(dot);
	// Try to stay off of the Newline
	if (*dot == '\n' && cnt > 0 && cmd_mode == CMODE_COMMAND)
		dot--;
}
