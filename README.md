# Tiny-C for UCSD Pascal II.0

A C compiler (a practical subset of C) that runs **on** the UCSD Pascal
II.0 P-System and produces P-code `.CODE` files. It is written in the same
subset, so it compiles itself: the P-System-built compiler is byte-identical
to the host-built one.

Start here when picking the project up in a new session.

## Repository layout

| Path | What |
|---|---|
| `tinyc/*.c`, `tc.h`, `parse.h`, `gen.h` | the compiler: `main` (driver, `@FILE` batches), `util`, `types`, `pp` (preprocessor), `lex`, `psym`/`expr`/`decl`/`stmt`/`compile` (parser; `compile` is its pass, split from `stmt`), `ir` (intermediate file), `gen`/`genx` (P-code: emitter and object file / expressions; split so that each compiles in Z80 mode), `link` (linker, `/J` join) |
| `tinyc/lib/*.c`, `libint.h` | the C library (joined into `TCLIB.OBJ`) |
| `tinyc/include/*.h` | headers; `tcmsgs.txt` = the compiler's messages (TCMSGS.TEXT) |
| `tests/NAME.c` + `.expect` (+ `.keys`, `.wait`) | test/demo programs and their expected output |
| `examples/` | `shell.c` (a mini-shell built on `pexec()`: a program name without a volume is looked for on every disk on line, and when several have it the shell asks which; `CD #5` sets the prefix to unit 5's volume, `DIR [VOL:|#n:][PATTERN]` lists files with `*`/`=` and `?` wildcards, `TYPE` shows text files, `DELETE` deletes files (with wildcards it asks first), `WHEREIS` finds files on every disk, `VOLUMES` lists the disks, `COPY`/`MOVE`/`RENAME` (a copy keeps its kind and date); its command line keeps the last 10 commands in `#4:SYSTEM.CMDS` (Up/Down) and is edited with Left, Right, Home, End, Insert, Delete; on BIGGY as SYSTEM.SHELL, which `$` at the Command: prompt runs) `memfree.c` (a program's free memory), `memmark.c` (`MEMMARK FILL`, run a program, `MEMMARK SCAN`: an estimate of that program's least free memory, from `memfill()`/`memgap()` in `psys.h`) and `args.c` (does what its `main(argc, argv)` arguments say: run it from the shell, e.g. `ARGS ADD 2 3`); on TCEXTRA |
| `volumes/` | **TINY-C.zip** (what is needed to use Tiny-C: compiler, library, messages, headers; the compiler looks for them on `TINY-C:`) **TCSRC.zip** (the compiler's and the library's sources, BUILD/LIBS scripts), **TCEXTRA.zip** (demos, the shell and its examples, CMPCODE), **TCTESTS.zip** (the test programs; four volumes because a UCSD directory holds 77 files and `@BUILD`/`@LIBS`/`@DEMOS`/`@TESTS` leave a NAME.OBJ per module or program), **TOOLS.zip** (tools written in Tiny-C, ready to run: VI.CODE, GREP.CODE) and **TOOLSRC.zip** (their sources: vi's modules, GREP.C) and **BIGGY.zip** (boot disk `Big_Disk.BLK` with Tiny-C ready to use; doubles need the v1.88 emulator's CSP 100..137); `*.txt` = file listings |
| `volumes/*---8_byte_floats.BLK` | the user's own working copies of the volumes: **frozen** -- no tool writes them, and changes to the generated volumes are not copied into them any more (only on request) |
| `ports/vi/` | **vi** (the BusyBox-derived tiny vi) ported to the P-System: modules `vi.h`, `vimain.c`, `viscreen.c`, `vitext.c`, `vicolon.c`, `vicmd.c`, `vipage.c` (still build on Linux) + `viucsd.c` (the P-System side); work in progress, see its README |
| `ports/grep/` | **grep.c**: `GREP [-i] PATTERN FILES` from the shell -- a regular expression (`*` any characters as in file names, `. [] + ? ^ $`), case ignored unless `-i`, `VOL:NAME:LINE: text`; FILES with wildcards on every disk unless a volume is named (TOOLS:GREP.CODE) |
| `verify/` | Tiny-C Verify pack: `TCVERIFY.SCRIPT` + `TCVERIFY.zip`, `cmpcode.c`, `rmfiles.c`, README |
| `repro/` | engine bug repros (REAL compare, DEEPCXP: both fixed in the engine) |
| `tools/` | host tools (below) |
| `UCSD-Pascal---P-Machine_work-v1.88.zip` | the emulator (engine, Linux runner `verify/run_verify.cpp`), with the double CSPs (NativeDouble.inc) |
| `emulator/` | **the double CSPs on their own**: `NativeDouble.inc` + the 4-line `PSystemEngine.cpp` patch, and how to add them to a Windows build (README); **the PC keys for vi**: `PSystemEngine-keys.patch` and `KEYS.md` (Page Up/Down, Home, End, Insert, Delete as one code each while a program sets `SYSCOM->expansion[1]` to `PX_KEYS`) |
| `Usefull_System_Disk_Images.zip`, `pascal.bin` | boot disk images, the Z80 loader |

## Tools (Linux; `tools/setup.sh` unpacks the zips into `build/` and builds `build/run_verify`)

**Emulator version:** calls through function pointers compile to `CSP 138` (CALLI) by
default (see below), which the emulator implements from **version 1.91**
([UCSD-Pascal_Windows_Emulator](https://github.com/Bio-Printer/UCSD-Pascal_Windows_Emulator)).
The zip in this directory is v1.88 and has no CALLI: build the runner from a checkout of the
emulator instead with `ENGINE_DIR=/path/to/UCSD-Pascal_Windows_Emulator tools/setup.sh`
(keep `ENGINE_DIR` set for the tools), or use `TINYC_Z80CALLS=1` with the zip.

| Tool | Does |
|---|---|
| `runtests.py [name]` | compile each test with the host compiler, run it on the P-System, compare with `.expect` |
| `crosscheck.py` | compile each test **on the P-System** too; the code files must be identical |
| `selfcompile.py [module]` | compile the compiler's modules on the P-System, link CC2.CODE, compare with the host build |
| `buildtc.py` | host build of CC.CODE from the modules (`build/tcmod/`) |
| `mkvolume.py` | build `volumes/` (TINY-C, TCSRC, TCEXTRA, TCTESTS, TOOLS, TOOLSRC) with FILES.TEXT listings; a new tool is one line in its `TOOLS` table |
| `mkbiggy.py` | build `volumes/BIGGY.zip`: the boot volume BIGGY: with SYSTEM.SHELL (the Tiny-C shell); Tiny-C itself is only on TINY-C: (`X TINY-C:CC`; since BIGGY 1.15); only files that differ are written, so it is byte-identical to the reference `Big_Disk.BLK` of [UCSD-Pascal-Volumes](https://github.com/Bio-Printer/UCSD-Pascal-Volumes) (Filer and Editor that take NAME.C / NAME.H workfiles) |
| `greptest.py` | GREP from the shell: case, `-i`, regular expressions, wildcards on every disk or one volume, line numbers across pages |
| `alltest.py` | TOOLSRC:ALL.TEXT: one batch file running BUILD, LIBS and TOOLS; the compiler, library and tools identical to the shipped ones, the objects on TCSRC: |
| `shellalltest.py` | the way a user starts a total rebuild: `$` at the Command: prompt, then `cc @all` in the shell (TOOLSRC:ALL: BUILD, LIBS, TOOLS); the same checks as `alltest.py`, and the tracked least free memory with the lowest passes |
| `findtest.py` | which disk CC takes `#include` files and a batch file's sources from (copies on several disks; the including file's disk, the prefix, an error) |
| `pexectest.py` | `pexec()` with the mini-shell (needs BIGGY 1.11; pexec itself works from 1.10): a program started from the shell has exactly the free memory it has from X(ecute; exit statuses, errors; the shell's CD, DIR, TYPE, DELETE, WHEREIS, VOLUMES, COPY, MOVE and RENAME, its command history and line editing; `$` at the Command: prompt |
| `voltest.py` | on the four volumes: `@LIBS`, `@BUILD` (on TCSRC:), `@DEMOS`, `@TESTS`, CMPCODE checks; reports least free memory |
| `mkverify.py`, `tcverify.py [native\|z80]` | build / run the Tiny-C Verify pack (`TCV_MAX=seconds` for Z80 mode) |
| `vitest.py [--build]` | `ports/vi`: the same 60-command editing session on Linux and on the P-System (from the shell); the saved files must be identical. `--build`: `@TOOLS` on TOOLSRC: (P-Code mode, Harvard layout) must make TOOLS:VI.CODE byte for byte |
| `modes.py prog.c` | run a program in Z80 and P-Code mode and compare (engine bug hunting) |
| `pdis.py FILE.CODE` | P-code disassembler |
| `pcensus.py FILE.CODE\|VOL.BLK` | static P-code census: instructions, and the inline constants/tables (`LSA LPA LDC XJP`), `--check` validates every jump target, `--selfpatch` lists the old self-modifying indirect calls |
| `f12test.py` | test the double (8-byte floating point) CSPs 100..137 (P-Code mode; see `docs/DOUBLES.md`) |
| `pchunk.py [SIZE ...]` | the compiler's least free memory (emulator-tracked, every @BUILD and @LIBS command) against PCHUNK, the block size of its permanent pool (`util.c`) |
| `ucsdvol.py` | read/write UCSD volume images (`ls`, `get`, `put`, `rm`, `new`) |
| `psys.py`, `tcrun.py`, `selfhost.py` | library code used by the above |

Host compiler: `build/tc [-c] [-z] [-I dir] [-L lib.obj] [-o out] files` (built from
`tinyc/tc.c` by `tcrun.build_tc()`). Environment: `PSYS_MODE=z80|native`,
`VERIFY_RECLAIM=1` (P-Code mode with the Z80 interpreter's memory reclaimed),
`TINYC_MAP=1` (linker prints procedure addresses).

Before committing a compiler change, run: `runtests.py`, `crosscheck.py`,
`selfcompile.py`, `mkvolume.py` + `voltest.py`, `mkverify.py` + `tcverify.py`,
`pexectest.py`.

## On the P-System

`X(ecute TINY-C:CC` (the compiler is CC.CODE on TINY-C:), then at "Compile what file?":
`NAME` (compile NAME.C, or NAME.TEXT, and link), `/C NAME`, `/L OUT=A,B`,
`/J LIB=A,B` (join objects into a library), `@FILE` (commands from FILE.TEXT);
`/Z NAME` and `/Z /C NAME` are `NAME` and `/C NAME` with `-z` (below).
The same commands can be given as arguments instead, e.g. from the shell
`cc @build @libs` or `cc /z hanoi sieve`: options and the word after them make
one command, they run in turn without a prompt, and the first that fails stops CC
(exit status 1).
Sources are `NAME.C`, headers `NAME.H` (UCSD text format, text kind).
Which disk a file comes from: a source, `#include` file or `@FILE` named with a
volume (`TOOLSRC:VI.H`, `#5:X.C`) only from there; named without one, CC reads every
disk's directory: on one disk only, that one; on several, the one on the disk of the
file that names it (the including file, the `@FILE`), else the prefix volume's, else
an error that lists the volumes (`findfile` in `pp.c`, code segment FIND;
`tools/findtest.py` tests it).
With the prefix on TCSRC: (and TINY-C: mounted), `X(ecute TINY-C:CC`
`@BUILD` rebuilds the compiler (TINY-C:CC2.CODE), `@LIBS` the library (TINY-C:TCLIB2.OBJ); `@TOOLS` (on TOOLSRC:) links every tool onto TOOLS:; `@ALL` (on TOOLSRC:) runs `@TCSRC:BUILD`, `@TCSRC:LIBS` and `@TOOLS` (a batch file may name others; `/C` puts NAME.OBJ on its source's volume, `/L` and `/J` find objects the way sources are found);
`@DEMOS` (on TCEXTRA:) and `@TESTS` (on TCTESTS:) every program there.

## Calls through function pointers (`-z` / `/Z`)

A call through a function pointer (`f(x)` with `f` a variable; `qsort`, `bsearch` and
`printf`'s floating-point formatting do it) is compiled to `<arguments>; <function value>;
CSP 138` (CALLI): the engine pops the function value (`seg | proc << 8`) and does what
`CXP seg,proc` would. Nothing is written into the instruction stream, so the code can live in
a separate instruction space. CALLI is in the native P-Code engine from version 1.91; in Z80
mode the engine's coprocessor (version 1.93) does it too.

`-z` (host) or `/Z` (P-System) generates the earlier sequence instead, which stores the
function value into the operands of a following `CXP` at run time. That works everywhere,
Z80 mode included. Everything linked into a program must be built the same way, the library
too: the tools build it twice (`tclib.obj`, and `tclibz.obj` with `-z`), `tcrun.py` picks
`-z` and `tclibz.obj` when `PSYS_MODE=z80` or `TINYC_Z80CALLS=1`, and the shipped volumes
(TINY-C:TCLIB.OBJ, the TCEXTRA: programs) are built with `-z` so that they work in both
modes. A program built without `-z` that calls through a function pointer and runs in Z80 mode
on an engine before 1.93 does not stop: it gives wrong results. From 1.93 it runs correctly
(`TINYC_Z80CALLS=0` makes `tcrun.py` test exactly that in Z80 mode).

## setjmp and longjmp

`<setjmp.h>`: `setjmp(env)` saves its own call's mark stack control word (the caller's
frame, segment, return address and stack depth) in `jmp_buf env`; `longjmp(env, val)`
returns from that setjmp call again with `val` (1 for 0), out of any number of calls and
segments. It gives back the segments of the functions it leaves (their reference counts
in INTSEGT, and in the engine's Harvard layout their code), as their returns would have.
The function that called setjmp must not have returned. Works in Z80 and P-Code mode,
with and without reclaimed memory and the Harvard layout; `tests/setjmp.c`, and
docs/DESIGN.md for how.

## Status (September 2026)

* Self-hosting, byte-identical, in P-Code mode and in **Z80 mode** (Z80 mode
  has 3,915 words less memory, and is the tightest).  GEN.C outgrew it
  (the Compiling pass ran out of stack in Z80 mode, and in P-Code mode
  without reclaimed memory), so it is split into `gen.c` and `genx.c`;
  `PSYS_MODE=z80 voltest.py` runs `@BUILD @LIBS @DEMOS @TESTS` in Z80 mode.
* Least free memory (SP - NP at every P-code instruction, tracked by the
  emulator: Options > Track Least Free Memory, run_verify
  `VERIFY_LOWWATER`) in Z80 mode (the normal layout; the same in P-Code
  mode without reclaim).  **A total rebuild started the way a user does it
  -- the shell (`$` at the Command: prompt), then `cc @all`: the compiler,
  the library, vi and grep -- has 2,887 words free at the least** (was 589;
  `tools/shellalltest.py`): compiling vi's VITEXT.C, then VIPAGE.C 2,915 and
  VICOLON.C 2,954, linking CC2.CODE 2,974, preprocessing the vi modules
  3,322, the compiler's own LINK.C 3,494, EXPR.C and PP.C 3,545, the
  library's STDIO.C 3,836.  (From X(ecute, not the shell, 92 words more.)
  Before that work, every @BUILD and @LIBS command from X(ecute had 2,713
  words (STDIO.C), `cc @build @libs` from the shell 2,573; the tools were
  not counted, and they were the worst: VICMD.C 589.  The Filer, setting the
  prefix, has 1,993 -- which is why shellalltest.py does not set it.)
  With reclaimed memory, 3,915 words more.  CC's "(N
  words free)" after each pass is that pass's least (`memleast()` in
  psys.h, emulator 1.99; elsewhere the free memory at the pass's end), so
  the least of them is the status bar's figure; `voltest.py` prints both.
  What took the memory, and what was done in the last round (a code segment
  is in memory as long as one of its functions runs, so the segments of
  main(), of the parser and of the pass count in full at every point, and a
  segment loaded at the deepest point of an expression counts there):
  - `fmtfix` (printf/scanf formats) was called for every call of a
    variadic function, which loaded the 2.3 KB segment REALLIT just to
    find that the name was not printf: at the deepest point of an
    expression that was the least free memory of most vi modules.
    `fmtfam` says first (in PARSE); fmtfix is in the small segment REFSCAN.
  - the case labels of a switch were two tables in the parser's heap that
    doubled at 64 cases (vi's do_cmd has 100+).  They go to the intermediate
    file as they come (record C) and the code generator collects them for
    the S record; the parser keeps only the values, in chunks, for the
    duplicate check.
  - `statement()` is smaller: for, do, switch, case labels, goto and labels
    are functions of their own, a block that is the body of an `if`, loop
    or switch is parsed by `compound()` directly (it was `statement()` ->
    `compound()`), and `case X:` / `default:` / `label:` go round the
    parser's loop instead of calling `statement()`, so a run of 12 case
    labels is no longer 12 stack frames.
  - `struct Node` is 16 bytes (was 24), so the expression pool (nodes and
    strings until the end of the statement) is 1,600 bytes in the Compiling
    pass (was 2,000; peak over the compiler, library, tools, demos and
    tests 1,308) and 1,900 in the code generator (was 2,400; peak 1,522).
    `castexpr` is part of `unary`, and the rest of `?:` is its own function:
    two frames fewer for every level of parentheses.
  - the linker allocated the code buffer (the largest procedure) and the
    entry code before pass 2, which reads past the code it does not need,
    and opened the object files with them in memory: 1,807 -> 2,934 words
    for linking CC2.CODE.
  - cold code left the always-resident segments: `linkall` and `linkcmd`
    (the /L and /J commands) are in LINK, `message` (reads TCMSGS.TEXT) in
    CINIT, the double and long-constant conversions in REALLIT: 1.7 KB.
  - the preprocessor keeps a macro in one block (name and body after a
    6-byte record) and collapses blanks in bodies (vi.h pads its 70
    accessor macros); `findfile`/`fileunit` read a directory block by block
    (2 KB less stack at an #include); the symbol hash tables are 32 entries
    (were 128).
  - vi.h includes `vipage.h` and `limits.h` before its 150 macros: opening
    a file needs 1,000 words of the OS's stack, and the preprocessor's least
    free memory was there.
  Earlier rounds:
  - the Compiling pass's heap is mostly the declarations a file uses
    (symbols, types, fields), held in blocks of PCHUNK bytes (util.c):
    `tools/pchunk.py` measures the least against PCHUNK (448 now; rerun
    it when the sources change).  The 35 runtime helpers (`__divi`,
    `__lmul` ...) are declared when first needed, not in every module.
    A symbol is 14 bytes (was 18 and a link-name string for a static
    function, which is now made when written), a type 12 (was 22: what
    only one kind of type uses shares a place, the small fields are
    flags): about 667 words more in every Compiling pass.
  - the code generator's buffers are sized for 1.5 times the largest
    procedure in the compiler, library, demos and tests (tc.h: MAXCODE
    3000 bytes, MAXLABEL 200, MAXFIX 250, MAXREL 320; the switch case
    table is allocated per switch).
  - files CC opens between passes (does NAME.C exist, the @FILE batch)
    are opened inside a heap mark of their own; their buffer used to sit
    unused under every pass (about 300 words).
  - the linker gives back its reference lists (what each procedure uses:
    about 1,950 words for CC2.CODE) once it knows what to link, before it
    writes the code (`pmark`/`prelease`, util.c).
  - the end of the Compiling pass (`compileend`: the module's variables,
    closing the intermediate file) runs in segment CINIT after PARSE has
    been left: with the heap at its largest, PARSE's 19 KB of code was
    still resident only for it.
  - real constants are converted by the preprocessor (`realtoken`,
    segment REALLIT): it writes each as `` `HHHHHHHHSDIGITSeEXP` `` (the
    float image in hex, S or L, the text a double is made from), so the
    compiling pass no longer loads REALLIT for a constant deep in an
    expression.
  - the linker's procedure record is 12 bytes (was 16: the count of
    references is the first word of their list, the flags are bits), and
    a static procedure's name is kept without its MODULE' (the module
    names are kept once): about 840 words more when it links CC2.CODE.
  - the linker keeps its procedure and variable tables in blocks of 64
    pointers: a doubling table freed the old one, and when the compiler
    passed 512 procedures that cost about 1,500 words.
  - STMT.C's pass and set-up moved to `compile.c`; GEN.C is `gen.c` and
    `genx.c` (unsplit, it ran out at 347 words).
* The Z80 interpreter on the boot disk has no SIN/COS/EXP/ATAN/SQT/LOG/LN
  (assembled with NOFPT): before engine 1.93 `math.h` functions stopped there
  with "Unimplemented instruction"; from 1.93 the engine's Z80-mode
  coprocessor does them (and the doubles and CALLI), as in P-Code mode.
* Memory techniques in use: per-pass heap (MARK/RELEASE, free list set aside
  across a pass), pass-only tables allocated per pass, shared function types,
  parameter names kept apart (`pnames`), header declarations kept only when
  used (`scanrefs` Bloom filter), prototypes declared under `#pragma segment`
  so calls within a segment are 2-byte CGPs (linker checks: message 116).
* Limits: 10 segments per program (1 + 7..15), 77 files per UCSD directory.
* **8-byte doubles** (`double`, `long double`; P-Code mode, and Z80 mode
  from engine 1.93):
  IEEE binary64 via the engine's CSP 100..137 — see `docs/DOUBLES.md`.
  `float` stays the 4-byte REAL; unsuffixed constants are float, `1.5L` double.

## Ideas not done yet

* More code-size work in the code generator (e.g. 1-byte global operands).
* A "Verify Tiny-C" item in the emulator's menu (sketch in `verify/README.md`).
