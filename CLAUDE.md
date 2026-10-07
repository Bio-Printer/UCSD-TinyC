# Notes for Claude

## Testing

Test what is likely to show a problem with the change at hand; do not run the
whole suite unless asked (it takes a long time).

| Changed | Run |
|---|---|
| vi only (`ports/vi/`) | the Linux comparison (paged build with a tiny `VI_PAGECAP` against `gcc -DSTANDALONE`), then `tools/vitest.py` in Z80 mode |
| compiler code generation or library | `tools/runtests.py` and `tools/crosscheck.py` in both modes, `tools/selfcompile.py` |
| linker, volume layout, memory | add `tools/voltest.py` and `tools/vitest.py --build` |
| pexec / shell | `tools/pexectest.py` |

Modes: `PSYS_MODE=native` or `PSYS_MODE=z80`. Say in the report what was run and
what was not.
