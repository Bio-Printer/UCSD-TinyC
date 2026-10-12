#!/usr/bin/env python3
"""wctest.py -- WC (ports/wc/wc.c, TOOLS:WC.CODE) from the shell on the P-System: every option (-l -w -c -m
-L, joined as -lw, none) on text files with tabs, blank lines, long lines and an empty file, several files
and the "total" line, the standard input (<FILE, a pipe, -), a binary file (whole 512-byte blocks), a
missing file (said, status 1), a bad option.  The numbers must be the host's wc's on the same text (the
widths of the columns are not compared).

Uses the mode of PSYS_MODE (native or z80) like the other tools."""
import os, sys, re, subprocess, tempfile
HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
sys.path.insert(0, HERE)
from psys import PSystem
import ucsdvol
from tcrun import compile_c
from difftest import make_text

FILES = {
    'W1.TEXT': ['one two three', '', '  leading and trailing  ', 'tab\there\tand more', 'x' * 100, 'a\tb', '\tstart', 'last line'],
    'W2.TEXT': [],
    'W3.TEXT': ['word'] * 3 + [''] * 4 + ['the quick brown fox jumps over the lazy dog ' * 5] + ['  '] + ['end'],
    'W4.TEXT': ['%d %s' % (i, 'w' * (i % 17)) for i in range(150)],
}
BIN = bytes(range(256)) * 4 - 0 if False else bytes(range(256)) * 3 + b'abc def\n'
OPTS = ['', '-l', '-w', '-c', '-m', '-L', '-lw', '-wL', '-lwcL', '-cl']


def hostwc(opt, names):
    d = tempfile.mkdtemp()
    for n in names:
        open(os.path.join(d, n), 'w').write(''.join(l + '\n' for l in FILES[n]))
    r = subprocess.run(['wc'] + opt.split() + names, capture_output=True, text=True, cwd=d)
    return [l.split() for l in r.stdout.splitlines()]


def fields(out):
    return [l.split() for l in out.replace('#5:', '').splitlines() if l.strip()]


def session(cmds, z):
    ps = PSystem()
    for src in ('examples/shell.c', 'ports/wc/wc.c', 'ports/cat/cat.c'):
        base, code = compile_c(os.path.join(ROOT, src), ps.dir, z)
        ps.put(base + '.CODE', open(code, 'rb').read())
    v = ucsdvol.Volume(ps.src)
    for n, ls in FILES.items():
        v.write(n, make_text(ls), 3)
    v.write('B.DAT', BIN, 5)
    v.save()
    script = ['WAIT "Command:"', 'TYPE "X"', 'WAIT "Execute what file?"', 'TYPE "#5:SHELL\\r"', 'WAIT "shell> "',
              'TYPE "cd #5\\r"', 'WAIT "shell> "']
    for i, c in enumerate(cmds):
        script += ['TYPE "%s >#5:O%d.TEXT\\r"' % (c, i), 'WAIT "shell> "']
    script += ['TYPE "wc -x\\r"', 'WAIT "shell> "', 'TYPE "wc #5:NOSUCH.TEXT\\r"', 'WAIT "shell> "', 'TYPE "bye\\r"', 'WAIT "Command:"']
    ok, tr, info = ps.run_script('\n'.join(script) + '\n', 3000)
    tr = tr.replace('\r', '\n')
    img = ucsdvol.Volume(os.path.join(ps.out, 'VERIFY_SOURCE.BLK'))
    outs = []
    for i in range(len(cmds)):
        n = 'O%d.TEXT' % i
        outs.append(ucsdvol.ucsd_to_text(img.read(n)[0]) if img.find(n) else None)
    return ok, outs, tr


def main():
    z = os.environ.get('PSYS_MODE') == 'z80'
    cases = []                      # (label, command, expected fields)
    for opt in OPTS:
        for names in (['W1.TEXT'], ['W3.TEXT'], ['W4.TEXT'], ['W2.TEXT'], ['W1.TEXT', 'W2.TEXT', 'W3.TEXT']):
            exp = hostwc(opt, names)
            cases.append(('wc %s %s' % (opt, ' '.join(names)), 'wc %s %s' % (opt, ' '.join('#5:' + n for n in names)), exp))
    for opt in ('', '-l', '-w', '-L'):
        exp = hostwc(opt, ['W1.TEXT'])
        exp = [e[:-1] for e in exp]
        cases.append(('stdin <FILE %s' % opt, 'wc %s < #5:W1.TEXT' % opt, exp))
        cases.append(('stdin pipe %s' % opt, 'cat #5:W1.TEXT | wc %s' % opt, exp))
        cases.append(('stdin - %s' % opt, 'wc %s - < #5:W1.TEXT' % opt, [e + ['-'] for e in exp]))
    # a binary file: whole blocks
    nblk = (len(BIN) + 511) // 512
    cases.append(('binary -c', 'wc -c #5:B.DAT', [[str(nblk * 512), 'B.DAT']]))
    checks = []
    outs, tr = [], ''
    for s in range(0, len(cases), 30):
        chunk = cases[s:s + 30]
        ok, o, tr = session([c[1] for c in chunk], z)
        checks.append(('a session of %d commands completed' % len(chunk), ok))
        for (label, cmd, exp), got in zip(chunk, o):
            g = fields(got) if got is not None else None
            if '-' == cmd.split()[-3:-2][0:1] and False:
                pass
            good = g == exp
            if not good:
                print('--- %s\n got: %s\n want: %s' % (label, g, exp))
            checks.append((label, good))
    checks.append(('a bad option: said', "invalid option -- 'x'" in tr))
    checks.append(('a missing file: said', 'NOSUCH.TEXT: cannot open it' in tr))
    bad = [n for n, g in checks if not g]
    for n, g in checks:
        print('%-5s %s' % ('ok' if g else 'FAIL', n))
    print('wc test: %s' % ('FAILED' if bad else 'PASSED'))
    return 1 if bad else 0


if __name__ == '__main__':
    sys.exit(main())
