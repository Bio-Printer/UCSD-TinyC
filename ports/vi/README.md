# vi for the P-System (work in progress)

`vi.c` is the BusyBox-derived "tiny vi" (Sterling Huxley; revised by brent@mbari.org
2020 and Stefan Haubental 2024; GPL v2 or later, see the file's header), edited so
that it builds with Tiny-C and runs on UCSD Pascal II.0. The same file still builds
on Linux (`gcc -DSTANDALONE vi.c`). `viucsd.h` holds everything that is P-System
specific.

Build (host): `build/tc -z -I tinyc/include -L tinyc/include/tclibz.obj ports/vi/vi.c -o VI.CODE`
(`-z` and `tclibz.obj` so that it also runs in Z80 mode); `tools/mkvolume.py` puts VI.CODE
on TOOLS: and the sources on TOOLSRC:. On the P-System, `@TOOLS` on TOOLSRC: rebuilds it
(the same VI.CODE byte for byte) -- for now only in P-Code mode with the Harvard layout
(Options > Reclaim Z80 Interpreter and BIOS Memory, Options > Harvard Mode), where the
compiler's code takes no data memory: in the normal layout and in Z80 mode its compile pass
runs out of memory on vi.c (the declarations of ~200 functions plus the compiler's
segments).

Later: split vi.c into modules compiled separately (/C) and linked (/L), as the compiler
itself is, so that Z80 mode and the normal layout can build it too.

Run: from the shell, `VI NAME.C` (the name as typed: `.C`, `.H` and `.TEXT` files are
UCSD text files); from X(ecute) it asks for the file.

## What was changed in vi.c

* A `#ifdef __UCSD__` configuration ahead of the Linux one: no signals, no window
  resizing, no locale or 8-bit characters, no `:!` shell escape; the line buffers
  sized for the P-System screen (132 columns); `#include "viucsd.h"`.
* The CRASHME test code removed.
* Tiny-C has no variadic macros: the five `USE_FEATURE_VI_READONLY(, x)` uses are
  written out.
* `do_cmd()` and `colon()` were too big for one P-code procedure (at most 60 long
  jumps; Tiny-C's 3000-byte code buffer): the second half of `do_cmd()`'s switch is
  `do_cmd2()`, and `colon()`'s `:s` and `:set` are `colon_s()` and `colon_set()`.
  Plain C, so Linux uses them too; the same 60 commands give the same file on Linux,
  in P-Code mode and in Z80 mode.
* Ten routines have P-System versions in `viucsd.h` (the originals stay for Linux):
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

## The P-System side (viucsd.h)

* Keys: `getch()` (UNITREAD, raw); the P-System's cursor keys (CRTINFO: the emulator
  sends ^T ^R ^Q ^U for the arrows) become vi's arrow keys, so ^R and ^U are not
  available as vi commands (^L redraws, ^B/^F page).
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

## Big files: a window (vipage.h)

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

Tested by giving the Linux build a tiny window (`gcc -DSTANDALONE
-DENABLE_FEATURE_VI_PAGING=1`, `VI_PAGECAP=2600`): random sessions of 60 commands on a
1500-line file must save the same file as the build without paging; and on the
P-System by `tools/vitest.py` (a 1500-line file through a 4 KB window in Z80 mode).

## Status

Works: moving, inserting, deleting, yank/put, marks, searching, `.`, `J`, `<` `>`,
`:w` `:w NAME` `:q` `:wq` `ZZ` `:s` `:set` `:r NAME` `:e! NAME`, ... from the shell and
from X(ecute), in P-Code and in Z80 mode.

Files up to about 125 KB (128 chunks). The window: about 4 KB in Z80 mode, 12 KB in
P-Code mode, 30 KB with the Harvard layout (where the 34 KB of code takes no data
memory).

Not yet (stage 2): commands over more lines than fit in the window at once (`:%s`,
`:1,$d`, `>G`, a big `yy` ...) work a window at a time; for now they are refused.
`%` finds a matching bracket only within the window. `:r NAME` needs the whole file
to fit.
