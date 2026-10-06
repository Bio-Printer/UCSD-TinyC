# vi for the P-System (work in progress)

`vi.c` is the BusyBox-derived "tiny vi" (Sterling Huxley; revised by brent@mbari.org
2020 and Stefan Haubental 2024; GPL v2 or later, see the file's header), edited so
that it builds with Tiny-C and runs on UCSD Pascal II.0. The same file still builds
on Linux (`gcc -DSTANDALONE vi.c`). `viucsd.h` holds everything that is P-System
specific.

Build (host): `build/tc -z -I tinyc/include -L tinyc/include/tclibz.obj ports/vi/vi.c -o VI.CODE`
(`-z` and `tclibz.obj` so that it also runs in Z80 mode).

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
  status line with `printf` instead of `sprintf`.

## The P-System side (viucsd.h)

* Keys: `getch()` (UNITREAD, raw); the P-System's cursor keys (CRTINFO: the emulator
  sends ^T ^R ^Q ^U for the arrows) become vi's arrow keys, so ^R and ^U are not
  available as vi commands (^L redraws, ^B/^F page).
* Screen: FGOTOXY for the cursor, CRTCTRL's erase-to-end-of-line/screen.
* Files: UCSD text files through stdio. The memory `fopen` needs (1 KB + 80 bytes) is
  set aside at the start and given back only around opening a file, so a file whose
  text has taken all other memory can still be saved. Files over 30000 characters are
  refused (an `int` holds no more) rather than loaded in part.
* `strncasecmp`, `strchrnul`, `memrchr`, `snprintf`, `getopt`, ... that the library
  does not have.

## Status

Works: moving, inserting, deleting, yank/put, marks, searching, `.`, `J`, `<` `>`,
`:w` `:w NAME` `:q` `:wq` `ZZ` `:s` `:set` `:r NAME` `:e! NAME`, ... from the shell and
from X(ecute), in P-Code and in Z80 mode.

Open: memory. The code is 28 KB (17.5 KB vi, 10.5 KB library) and the screen copy
6 KB (132 x 45), so the largest file is about 6.5 KB in Z80 mode and 14 KB in P-Code
mode (28 KB with the Harvard layout, where the code is not in data memory). Running
out of memory while editing still ends the program (`out of memory`, or a stack
overflow), losing the changes since the last write.
