# vi for the P-System (work in progress)

vi is the BusyBox-derived "tiny vi" (Sterling Huxley; revised by brent@mbari.org
2020 and Stefan Haubental 2024; GPL v2 or later, see `vi.h`'s header), edited so
that it builds with Tiny-C and runs on UCSD Pascal II.0. It was one file, `vi.c`
(123 KB, too big to edit with vi itself); it is now modules of about 20 KB:

| File | What |
|---|---|
| `vi.h` | what the modules share: the configuration, the global state (`struct globals G`), the functions one module calls in another |
| `vimain.c` | start, the main loop, keys, files, the terminal (Linux) |
| `viscreen.c` | the screen: drawing, the status line, the cursor |
| `vitext.c` | moving in and changing the text, searching, registers |
| `vicolon.c` | the `:` commands |
| `vicmd.c` | the vi commands (`do_cmd`) |
| `vipage.c`, `vipage.h` | a window into big files |
| `viucsd.c` | the P-System: keys, screen, files, the library functions Tiny-C lacks |

A function used in one module only is `static` there; the others are declared in
`vi.h`. The same files still build on Linux
(`gcc -DSTANDALONE vimain.c viscreen.c vitext.c vicolon.c vicmd.c vipage.c viucsd.c`;
`viucsd.c` is empty there).

Build (host): each module `build/tc -c -z -I tinyc/include ports/vi/NAME.c -o NAME.obj`,
then `build/tc -z -L tinyc/include/tclibz.obj vimain.obj ... viucsd.obj -o VI.CODE`
(`-z` and `tclibz.obj` so that it also runs in Z80 mode; `compile_modules` in
`tools/tcrun.py` does it); `tools/mkvolume.py` puts VI.CODE on TOOLS: and the sources
on TOOLSRC:. On the P-System, `@TOOLS` on TOOLSRC: compiles each module (`/Z /C`)
and links them (`/L VI=...`): the same VI.CODE byte for byte, in Z80 mode and the
normal layout too (`tools/vitest.py --build`). In one piece, vi.c needed P-Code mode
with the Harvard layout, the only one with the memory to compile it.

Run: from the shell, `VI NAME.C` (the name as typed: `.C`, `.H` and `.TEXT` files are
UCSD text files); from X(ecute) it asks for the file.

## What was changed in vi

* A `#ifdef __UCSD__` configuration ahead of the Linux one: no signals, no window
  resizing, no locale or 8-bit characters, no `:!` shell escape; the line buffers
  sized for the P-System screen (132 columns); `viucsd.c`.
* The CRASHME test code removed.
* Tiny-C has no variadic macros: the five `USE_FEATURE_VI_READONLY(, x)` uses are
  written out.
* `do_cmd()` and `colon()` were too big for one P-code procedure (at most 60 long
  jumps; Tiny-C's 3000-byte code buffer): the second half of `do_cmd()`'s switch is
  `do_cmd2()`, and `colon()`'s `:s` and `:set` are `colon_s()` and `colon_set()`.
  Plain C, so Linux uses them too; the same 60 commands give the same file on Linux,
  in P-Code mode and in Z80 mode.
* Ten routines have P-System versions in `viucsd.c` (the originals stay for Linux):
  `rawmode`, `cookmode`, `awaitInput`, `readit`, `file_size`, `file_insert`,
  `file_write`, `place_cursor`, `clear_to_eol`, `clear_to_eos`, and `show_help`
  (its one string is longer than the P-machine's 255-byte constants).
* Screen size from SYSCOM's CRTINFO; standout (bold status line) with the console's
  reverse/normal video codes 15 and 14 instead of ANSI escapes.
* The text buffer keeps 1 KB of free room instead of 10 KB (`TEXT_SLACK`).
* Fixes to upstream: the marks (`ma`, `''`) now move with the text when the buffer is
  reallocated (they were left pointing into the freed buffer); `:set` built its
  status line with `printf` instead of `sprintf`; `p`/`P` with an empty register
  left `.` recording, so the `.` typed next repeated itself for ever (vi hung).

## The P-System side (viucsd.c)

* Keys: `getch()` (UNITREAD, raw); the P-System's cursor keys (CRTINFO: the emulator
  sends ^T ^R ^Q ^U for the arrows) become vi's arrow keys, so ^R and ^U are not
  available as vi commands (^L redraws, ^B/^F page).
* Page Up, Page Down, Home, End, Insert, Delete: the emulator types the L2 editor's
  commands for them (`>P`, `JB`, ...). While vi runs it sets SYSCOM->expansion[1] to
  `PX_KEYS` (psys.h; cleared when it ends), and the emulator (from version 2.00;
  `emulator/KEYS.md`) then sends one code each, vi's own VI_K_HOME ...
  VI_K_PAGEDOWN (0x84..0x89): a screen down/up, start/end of the line, insert, x.
* The cursor (emulator 2.02): vi sets `PX_KEYS_BLOCK` rather than `PX_KEYS`, and the
  emulator draws the cursor as a block while vi runs; a line again when it ends.
* The mouse wheel (emulator 2.01): KEY_WHEELUP / KEY_WHEELDN (0x96, 0x97), one a
  notch, scroll 3 lines (as ^Y / ^E) in command mode; typing and : lines ignore them.
* Screen: FGOTOXY for the cursor, CRTCTRL's erase-to-end-of-line/screen.
* Files: UCSD text files through stdio. The memory `fopen` needs (1 KB + 80 bytes) is
  set aside at the start and given back only around opening a file, so a file whose
  text has taken all other memory can still be saved.
* The screen is kept as one checksum per row (90 bytes) instead of a copy (132 x 45 =
  6 KB, `VI_ROW_SUMS`): a row whose checksum changed is written again whole.
* Line numbers in addresses are parsed by hand: `sscanf` would bring the library's
  whole `scanf`.
* `strncasecmp`, `strchrnul`, `memrchr`, `snprintf`, `getopt`, ... that the library
  does not have.

## Big files: a window (vipage.c)

As the UCSD L2 editor does it, vi keeps only a window of the file in memory
(`ENABLE_FEATURE_VI_PAGING`, on for the P-System). The whole file goes into a
temporary file, `VI.SWAP` on the prefix volume, in 1 KB slots of whole lines; two
stacks of slot descriptors hold the lines *before* and *after* the window. Moving the
window writes the lines leaving it at one edge onto that side's stack and reads the top
chunk of the other side's stack in; slots are reused. Every line is in exactly one
place, so saving writes before + window + after (the file being edited is not kept
open, and saving over it is safe). `VI.SWAP` is deleted when vi ends.

* Before every command (and every repeat of a counted one) a screen of lines is kept
  in memory on both sides of the cursor, and 512 bytes free for typing: `j`, `k`, `^F`,
  `^B`, `}` ... move the window as they reach its edge. Inserting more than fits writes
  lines out below the cursor (above it only if it must).
* `/` `?` `n` `N` (and `{` `}`) search on through the stores, wrapping round the whole
  file; `G`, `gg`, `NG`, `:N`, `'a`, `''` move the window to the line, far moves by
  passing whole chunks between the stacks without reading them.
* Line numbers count from the file's start: the status line, `:=`, addresses.
  Marks and the `''` context are line numbers (vi's own to-do list asked for that).
* A range (`5dd`, `d10j`, `:100,200d`, `:%s/a/b/`) is first brought into memory whole;
  if it does not fit, the command is refused with a message, never cut short. While a
  range is held, the window is locked: a motion that would leave it refuses too.
* A yank too big for memory leaves the register empty with a message (vi no longer
  exits).
* The window is allocated once, as big as memory allows, and never reallocated.

Tested by giving the Linux build a small window (`gcc -DSTANDALONE
-DENABLE_FEATURE_VI_PAGING=1 vi*.c`, `VI_PAGECAP=6000`): `tools/vipagetest.py`, random
sessions of 60 commands on a 1500-line file, must save what the committed version
saves, with the file loaded lazily and not; and on the P-System by `tools/vitest.py`
(a 1500-line file through a 2.8 KB window in Z80 mode).

### Loading a file: the first screen at once

vi used to read the whole file before it drew anything, a character at a time
(and once more, to count its bytes first): in Z80 mode a 94 KB file took 20 s
to the first screen. Now `pg_load()` only opens the file, and the window is
filled from it as far as the window needs (`pg_more()`: a chunk of whole lines,
read a block at a time with the library's `fread`, written to VI.SWAP and put on
the bottom of the after stack); the first screen takes 0.4 s whatever the
file's size. The rest is read

* when the window needs lines below it (`pg_fill_bottom()`),
* while vi waits for a key (`pg_idle()` in `readit()`: a chunk, a look at the
  keyboard, a chunk ...: the whole 94 KB in about 5 s more, in Z80 mode),
* at once by what needs the whole file: `G`, a search, a `:` command, `:w`
  and `ZZ` (`pg_complete()`; `TOTLINES()` completes, `STATLINES()` does not).

Until it is all in, the status line's total is the lines loaded so far. A line
longer than 1023 characters, a file of more than 128 chunks, or VI.SWAP full:
at the start vi stops with a message as before; later (the screen is up, edits
may be made) the file stays as far as it was loaded (`pg_bad`) and `:w` refuses
to save it.

### Memory

vi is one resident code segment: every byte of code is a byte less for the
window (Z80 mode: 2.8 KB; below about 2.7 KB the paging misplaces the cursor
after some ^B). `#pragma segment MAIN` at the top of vi.h makes every call
within the segment a 2-byte CGP: the compiler puts a NOP after a call whose
callee's segment it does not know (3 bytes, for the linker to turn into a CXP if
the callee turns out to be in another segment), 1,000 of them in vi: 690 bytes
less. VI.SWAP's slot map is one bit for each of the 128 chunks (it was 2048).

## Speed of the screen

* `refresh()` compares all rows (format, checksum) only when something they
  depend on has changed: the text (`scr_gen`, +1 at every edit and at every
  screen erase), the top line, the horizontal offset, the tab stop, the size or
  the window's place in the file. A cursor move within the screen (h j k l,
  w, b ...) redraws nothing but the cursor and the status line: about 4 times
  faster.
* The cursor is placed by writing the OS's FGOTOXY codes (CHR(1), X+32, Y+32:
  MYGOTOXY.TEXT, built into BIGGY 1.11 and later) into the output buffer with
  the text, not by a flush and a call to GOTOXY for every row; the keyboard
  is polled every 8th row, not every row. A page of text is about 30% faster.
* With emulator 2.03 or later (SYSCOM->expansion[1] = 25607 is answered 25608:
  emulator/KEYS.md, PX_KEYS_SCROLL in psys.h) a scroll of the screen is done by
  the console: ESC [ 1 ; N r (the text rows as the scroll region), ESC [ n S or
  ESC [ n T, ESC [ r. The rows' checksums move with the rows, and only the rows
  that come in are formatted, compared and drawn: j or k past the screen's edge,
  ^E, ^Y, the mouse wheel and a short ^D / ^U draw 1 to 3 rows instead of 43.
  A page (^F, ^B, Page Down, Page Up) moves the top line by nearly the whole
  screen, so it still draws nearly every row. Without 2.03 (the answer does not
  come) vi stores PX_KEYS_BLOCK as before and redraws every row that changed.
  `tools/viscrolltest.py` runs vi on the P-System with and without the codes and
  compares the screen after every key.
* Every byte of code is a byte less for the window in Z80 mode (vi is one
  resident 35 KB segment): the scroll code and the shorter status buffers
  (148 bytes) took the window from 3072 to 2802 bytes there; the 1500-line
  session of `tools/vitest.py` needs about 2.8 KB.
* The row checksum stays a multiply-add (`sum * 31 + c`): the P-machine has a
  multiply but no shift, rotate or xor, so a rotate-and-xor sum costs more
  (a right shift or ^ is a library call with a loop; measured slower).
* `tools/viscreentest.py` compares the screen after every key with an earlier
  version's on Linux.

## Status

Works: moving, inserting, deleting, yank/put, marks, searching, `.`, `J`, `<` `>`,
`:w` `:w NAME` `:q` `:wq` `ZZ` `:s` `:set` `:r NAME` `:e! NAME`, ... from the shell and
from X(ecute), in P-Code and in Z80 mode.

Files up to about 125 KB (128 chunks). The window: about 2.8 KB in Z80 mode, 12 KB in
P-Code mode, 30 KB with the Harvard layout (where the 34 KB of code takes no data
memory).

Not yet (stage 2): commands over more lines than fit in the window at once (`:%s`,
`:1,$d`, `>G`, a big `yy` ...) work a window at a time; for now they are refused.
`%` finds a matching bracket only within the window. `:r NAME` needs the whole file
to fit.

vi does not use the library's printf family (2.8 KB of resident code):
`vi_vsnprintf` in viucsd.c does %s %d %i %u %c %% into a buffer of a given size
(`printf` is `vi_printf`, `vsprintf` and `sprintf` are bounded), and the 300-byte
buffer of the old `snprintf` is gone: 1.8 KB more window in Z80 mode (4.3 KB).

Scrolling up without the emulator's scroll codes (before 2.03, or any console): from the
last row, n line feeds move the whole screen up n lines (`scroll_rows`, `scroll_lf`); only the n
rows that come in are drawn, and the status line, which went up with the rest, again. Scrolling
down has no such code, so it redraws the rows. (`tools/viscreentest.py` runs it as `VI_LFSCROLL=1`.)
