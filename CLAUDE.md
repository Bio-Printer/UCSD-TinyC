# Notes for Claude

## Repositories

When work changes what a repository holds, commit and push that repository
too -- not only a patch or a copy somewhere else:
* UCSD-TinyC (this one): `main` and the session's branch.
* UCSD-Pascal-Volumes: `main` -- the changed BLK_format disks, the extracted
  files (`tools/extract_all.py BLK_format .`), the manifest, and a README.txt
  entry.
* UCSD-Pascal_Windows_Emulator: `main` -- an engine or window change is a new
  version (Version.h, UCSDPascal.rc, README).

## Testing

Test what is likely to show a problem with the change at hand; do not run the
whole suite unless asked (it takes a long time).

| Changed | Run |
|---|---|
| vi only (`ports/vi/`) | the Linux comparison (paged build with a tiny `VI_PAGECAP` against `gcc -DSTANDALONE vi*.c`), then `tools/vitest.py` in Z80 mode |
| compiler code generation or library | `tools/runtests.py` and `tools/crosscheck.py` in both modes, `tools/selfcompile.py` |
| linker, volume layout, memory | add `tools/voltest.py` and `tools/vitest.py --build` |
| pexec / shell | `tools/pexectest.py` |
| memory (segments, pools, parser or linker stack): the least free memory of `cc @all` from the shell | `tools/shellalltest.py` (add `PSYS_MODE=z80` before a commit: about 25 minutes), `tools/voltest.py` |
| where CC finds files (`findfile`, pp.c / main.c) | `tools/findtest.py`, `tools/selfcompile.py` |
| CC's batch files (`@`, main.c), where objects go | `tools/alltest.py` (TOOLSRC:ALL: BUILD, LIBS, TOOLS in one run), `tools/findtest.py` |
| grep (`ports/grep/`) | `tools/greptest.py` |

Modes: `PSYS_MODE=native` or `PSYS_MODE=z80`. Say in the report what was run and
what was not.
