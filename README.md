# Tiny-C for UCSD Pascal II.0

A C compiler (a practical subset of C) that runs **on** the UCSD Pascal
II.0 P-System and produces P-code `.CODE` files. It is written in the same
subset, so it compiles itself: the P-System-built compiler is byte-identical
to the host-built one.

Start here when picking the project up in a new session.

## Repository layout

| Path | What |
|---|---|
| `tinyc/*.c`, `tc.h`, `parse.h` | the compiler: `main` (driver, `@FILE` batches), `util`, `types`, `pp` (preprocessor), `lex`, `psym`/`expr`/`decl`/`stmt` (parser), `ir` (intermediate file), `gen` (P-code), `link` (linker, `/J` join) |
| `tinyc/lib/*.c`, `libint.h` | the C library (joined into `TCLIB.OBJ`) |
| `tinyc/include/*.h` | headers; `tcmsgs.txt` = the compiler's messages (TCMSGS.TEXT) |
| `tests/NAME.c` + `.expect` (+ `.keys`, `.wait`) | test/demo programs and their expected output |
| `examples/` | `shell.c` (a mini-shell built on `pexec()`; on BIGGY as SYSTEM.SHELL, which `$` at the Command: prompt runs) and `memfree.c` (a program's free memory); on TCEXTRA |
| `volumes/` | **TINY-C.zip** (compiler, library, headers, all sources, BUILD/LIBS scripts) **TCEXTRA.zip** (tests/demos, CMPCODE) and **BIGGY.zip** (boot disk `Big_Disk.BLK` with Tiny-C ready to use; doubles need the v1.88 emulator's CSP 100..137); `*.txt` = file listings |
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
| `selfcompile.py [module]` | compile the compiler's modules on the P-System, link TINYC2.CODE, compare with the host build |
| `buildtc.py` | host build of TINYC.CODE from the modules (`build/tcmod/`) |
| `mkvolume.py` | build `volumes/` (TINY-C, TCEXTRA) with FILES.TEXT listings |
| `mkbiggy.py` | build `volumes/BIGGY.zip`: the boot volume BIGGY: with TINYC.CODE, TCLIB.OBJ, TCMSGS.TEXT, the headers and SYSTEM.SHELL; only files that differ are written, so it is byte-identical to the reference `Big_Disk.BLK` of [UCSD-Pascal-Volumes](https://github.com/Bio-Printer/UCSD-Pascal-Volumes) (Filer and Editor that take NAME.C / NAME.H workfiles) |
| `pexectest.py` | `pexec()` with the mini-shell (needs BIGGY 1.11; pexec itself works from 1.10): a program started from the shell has exactly the free memory it has from X(ecute; exit statuses, errors; `$` at the Command: prompt |
| `voltest.py` | on the volumes: `@LIBS`, `@BUILD`, `@DEMOS`, CMPCODE checks; reports least free memory |
| `mkverify.py`, `tcverify.py [native\|z80]` | build / run the Tiny-C Verify pack (`TCV_MAX=seconds` for Z80 mode) |
| `modes.py prog.c` | run a program in Z80 and P-Code mode and compare (engine bug hunting) |
| `pdis.py FILE.CODE` | P-code disassembler |
| `pcensus.py FILE.CODE\|VOL.BLK` | static P-code census: instructions, and the inline constants/tables (`LSA LPA LDC XJP`), `--check` validates every jump target, `--selfpatch` lists the old self-modifying indirect calls |
| `f12test.py` | test the double (8-byte floating point) CSPs 100..137 (P-Code mode; see `docs/DOUBLES.md`) |
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

`X(ecute TINYC`, then at "Compile what file?":
`NAME` (compile NAME.C, or NAME.TEXT, and link), `/C NAME`, `/L OUT=A,B`,
`/J LIB=A,B` (join objects into a library), `@FILE` (commands from FILE.TEXT);
`/Z NAME` and `/Z /C NAME` are `NAME` and `/C NAME` with `-z` (below).
Sources are `NAME.C`, headers `NAME.H` (UCSD text format, text kind).
`@BUILD` rebuilds the compiler (TINYC2.CODE), `@LIBS` the library (TCLIB2.OBJ),
`@DEMOS` (on TCEXTRA:) every program there.

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

## Status (September 2026)

* Self-hosting, byte-identical, in P-Code mode and in **Z80 mode** (Z80 mode
  has 3,915 words less memory; tightest: code generation of STMT/GEN, ~165
  words to spare — the driver's 1 KB stack saving since then adds to that).
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
