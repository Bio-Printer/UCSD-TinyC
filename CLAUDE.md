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

## New tools on TOOLS:

A program added to the `TOOLS` list in `tools/mkvolume.py` needs a page in `ports/man/MAN.TEXT` (a
`%% NAME` heading, then NAME / SYNOPSIS / OPTIONS ...; the header of that file says how): MAN shows it and
`mkvolume.py` stops when it is missing.  Change an option of a tool, change its page.

## Emulator versions

New code need not work with older emulators: use the current emulator's features
(handshake words, scroll codes, key counts) directly, no fallbacks for older ones.
Say in the report which emulator version a change needs.

## Testing

Test what is likely to show a problem with the change at hand; do not run the
whole suite unless asked (it takes a long time).

| Changed | Run |
|---|---|
| vi only (`ports/vi/`) | the Linux comparison (paged build with a tiny `VI_PAGECAP` against `gcc -DSTANDALONE vi*.c`), `tools/viscreentest.py` (screens vs the committed version), `tools/vipagetest.py` (the paged build, lazy loading, vs the committed version), `tools/viscrolltest.py` (with and without the emulator's scroll codes; needs `build/run_verify_new` from UCSD-Pascal_Windows_Emulator), then `tools/vitest.py` in Z80 mode |
| compiler code generation or library | `tools/runtests.py` and `tools/crosscheck.py` in both modes, `tools/selfcompile.py`; for stdio (`tinyc/lib/stdio.c`) also `tools/greptest.py`, `tools/pexectest.py` and `tools/vitest.py` |
| linker, volume layout, memory | add `tools/voltest.py` and `tools/vitest.py --build` |
| pexec / shell | `tools/pexectest.py` |
| memory (segments, pools, parser or linker stack): the least free memory of `cc @all` from the shell | `tools/shellalltest.py` (add `PSYS_MODE=z80` before a commit: about 25 minutes), `tools/voltest.py` |
| where CC finds files (`findfile`, pp.c / main.c) | `tools/findtest.py`, `tools/selfcompile.py` |
| CC's batch files (`@`, main.c), where objects go | `tools/alltest.py` (TOOLSRC:ALL: BUILD, LIBS, TOOLS in one run), `tools/findtest.py` |
| grep (`ports/grep/`) | `tools/greptest.py` |
| cat (`ports/cat/`) | `tools/cattest.py` |
| hexdump (`ports/hexdump/`) | `tools/hexdumptest.py` |
| objcopy (`ports/objcopy/`) | `tools/objcopytest.py` (needs hexdump) |
| less, more (`ports/less/`, `ports/more/`) | `tools/pagertest.py` |
| man, MAN.TEXT (`ports/man/`) | `tools/mantest.py` (needs cat) |
| sed (`ports/sed/`) | `tools/sedtest.py` (against the host's sed) |
| diff (`ports/diff/`) | `tools/difftest.py` (against the host's diff and patch) |
| wc (`ports/wc/`) | `tools/wctest.py` (against the host's wc; needs cat) |
| lc (`ports/lc/`) | `tools/lctest.py` (needs wc) |
| patch (`ports/patch/`) | `tools/patchtest.py` (against the host's diff) |
| shell redirection / pipes (`examples/shell.c`, `tinyc/lib/redirect.c`) | `tools/redirtest.py`, `tools/pexectest.py`, `tools/greptest.py` |
| emulator keys / break / ^S ^Q (`tools/mkbiggy.py` MISCINFO, psys.h handshakes) | `tools/typeaheadtest.py`, `tools/breaktest.py`, `tools/flowtest.py` |

Modes: `PSYS_MODE=native` or `PSYS_MODE=z80`. Say in the report what was run and
what was not.
