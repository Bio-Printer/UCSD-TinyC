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
| `examples/` | `shell.c` (a mini-shell built on `pexec()`: a program name without a volume is looked for on every disk on line, and when several have it the shell asks which; `CD #5` sets the prefix to unit 5's volume, `DIR [VOL:|#n:][PATTERN]` lists files with `*`/`=` and `?` wildcards, `TYPE` shows text files, `DELETE` deletes files (with wildcards it asks first); on BIGGY as SYSTEM.SHELL, which `$` at the Command: prompt runs) `memfree.c` (a program's free memory), `memmark.c` (`MEMMARK FILL`, run a program, `MEMMARK SCAN`: an estimate of that program's least free memory, from `memfill()`/`memgap()` in `psys.h`) and `args.c` (does what its `main(argc, argv)` arguments say: run it from the shell, e.g. `ARGS ADD 2 3`); on TCEXTRA |
| `volumes/` | **TINY-C.zip** (what is needed to use Tiny-C: compiler, library, messages, headers; the compiler looks for them on `TINY-C:`) **TCSRC.zip** (the compiler's and the library's sources, BUILD/LIBS scripts), **TCEXTRA.zip** (demos, the shell and its examples, CMPCODE), **TCTESTS.zip** (the test programs; four volumes because a UCSD directory holds 77 files and `@BUILD`/`@LIBS`/`@DEMOS`/`@TESTS` leave a NAME.OBJ per module or program), **TOOLS.zip** (tools written in Tiny-C, ready to run: VI.CODE) and **TOOLSRC.zip** (their sources: VI.C, VIUCSD.H) and **BIGGY.zip** (boot disk `Big_Disk.BLK` with Tiny-C ready to use; doubles need the v1.88 emulator's CSP 100..137); `*.txt` = file listings |
| `volumes/*---8_byte_floats.BLK` | the user's own working copies of the volumes: **frozen** -- no tool writes them, and changes to the generated volumes are not copied into them any more (only on request) |
| `ports/vi/` | **vi** (the BusyBox-derived tiny vi) ported to the P-System: modules `vi.h`, `vimain.c`, `viscreen.c`, `vitext.c`, `vicolon.c`, `vicmd.c`, `vipage.c` (still build on Linux) + `viucsd.c` (the P-System side); work in progress, see its README |
| `verify/` | Tiny-C Verify pack: `TCVERIFY.SCRIPT` + `TCVERIFY.zip`, `cmpcode.c`, `rmfiles.c`, README |
| `repro/` | engine bug repros (REAL compare, DEEPCXP: both fixed in the engine) |
| `tools/` | host tools (below) |
| `UCSD-Pascal---P-Machine_work-v1.88.zip` | the emulator (engine, Linux runner `verify/run_verify.cpp`), with the double CSPs (NativeDouble.inc) |
| `emulator/` | **the double CSPs on their own**: `NativeDouble.inc` + the 4-line `PSystemEngine.cpp` patch, and how to add them to a Windows build (README) |
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
| `pexectest.py` | `pexec()` with the mini-shell (needs BIGGY 1.11; pexec itself works from 1.10): a program started from the shell has exactly the free memory it has from X(ecute; exit statuses, errors; the shell's CD, DIR, TYPE and DELETE; `$` at the Command: prompt |
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
With the prefix on TCSRC: (and TINY-C: mounted), `X(ecute TINY-C:CC`
`@BUILD` rebuilds the compiler (CC2.CODE), `@LIBS` the library (TCLIB2.OBJ);
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
  mode without reclaim): every @BUILD and @LIBS command from X(ecute
  2,713 words (the library's STDIO.C, Compiling pass; EXPR.C and LINK.C
  about 2,740, linking CC2.CODE 2,813); `cc @build @libs` from the shell
  2,573 (STDIO.C).  (The Filer, setting the prefix, has less: 1,993.)  With reclaimed memory, 3,915 words more.  CC's "(N
  words free)" after each pass is that pass's least (`memleast()` in
  psys.h, emulator 1.99; elsewhere the free memory at the pass's end), so
  the least of them is the status bar's figure; `voltest.py` prints both.
  What took the memory, and what was done:
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
