#!/usr/bin/env python3
"""lctest.py -- LC (ports/lc/lc.c, TOOLS:LC.CODE) from the shell on the P-System: the names of a disk in
columns (-C when the output goes to a file; down the columns, -x across), one on a line (-1, and when not
-C), -F (CODE files get a *), -s (sizes), -r, -S, patterns (* = ? and a VOL:/#n: before them), several
patterns (a header line each), a pipe to WC, a pattern that matches nothing (status 1), a disk that is not
there (status 2), a bad option.  The expected output is made here from the directory of the volume
(the console is 132 wide).

Uses the mode of PSYS_MODE (native or z80) like the other tools."""
import os, sys, re
HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
sys.path.insert(0, HERE)
from psys import PSystem
import ucsdvol
from tcrun import compile_c
from difftest import make_text

WIDTH = 132
KINDS = {'TEXT': 3, 'CODE': 2, 'DATA': 5}
FILES = {}          # name -> (kind, bytes)
for i in range(34):
    ext = ['TEXT', 'CODE', 'DATA'][i % 3]
    stem = ['F%02d' % i, 'FILE%d' % i, 'FQ' + 'X' * (i % 9), 'F-%d' % (i * 7)][i % 4]
    name = '%s.%s' % (stem, ext)
    size = [100, 700, 1500, 3000, 513, 5000][(i * 5) % 6] + i
    if ext == 'TEXT':
        FILES[name] = (3, make_text(['line %d' % j for j in range(size // 20)]))
    else:
        FILES[name] = (KINDS[ext], bytes(size))


def columns(items, across):
    """items already ordered; the lines of the layout (the rules of lc.c)"""
    n = len(items)
    maxlen = max(len(s) for s in items)
    cols = max(1, min(n, WIDTH // (maxlen + 2)))
    rows = (n + cols - 1) // cols
    if not across:
        cols = (n + rows - 1) // rows
    cw = maxlen + 2
    out = []
    for r in range(rows):
        line = []
        for c in range(cols):
            i = r * cols + c if across else c * rows + r
            if i < n:
                line.append(items[i])
        out.append(''.join(s.ljust(cw) for s in line[:-1]) + line[-1])
    return out


def expect(flags, names, entries):
    """lines lc prints for the file names (all matching) with the flags"""
    sel = list(names)
    size = {n: entries[n] for n in sel}
    if 'S' in flags:
        sel.sort(key=lambda n: (-size[n], n))
    else:
        sel.sort()
    if 'r' in flags:
        sel.reverse()
    items = []
    for n in sel:
        s = n + ('*' if 'F' in flags and n.endswith('.CODE') else '')
        items.append(('%4d ' % size[n] if 's' in flags else '') + s)
    if not items:
        return []
    if '1' in flags or 'C' not in flags:
        return items
    return columns(items, 'x' in flags)


def glob(pat):
    r = '^' + ''.join('.*' if c in '*=' else '.' if c == '?' else re.escape(c.upper()) for c in pat) + '$'
    return re.compile(r)


def main():
    z = os.environ.get('PSYS_MODE') == 'z80'
    ps = PSystem()
    for src in ('examples/shell.c', 'ports/lc/lc.c', 'ports/wc/wc.c'):
        base, code = compile_c(os.path.join(ROOT, src), ps.dir, z)
        ps.put(base + '.CODE', open(code, 'rb').read())
    v = ucsdvol.Volume(ps.src)
    for n, (k, data) in FILES.items():
        v.write(n, data, k)
    v.save()
    v = ucsdvol.Volume(ps.src)
    entries = {e[3]: e[1] - e[0] for e in v.entries}
    allf = sorted(entries)
    cases = []          # (label, command, expected lines)
    for flags in ('', 'C', 'Cx', 'C1', 'CF', 'Cs', 'Cr', 'CS', 'CFs', 'CxS', 'CxFsr', '1', 'x', 'F', 's', 'Sr'):
        pat = 'F*'
        sel = [n for n in allf if glob(pat).match(n)]
        cases.append(('lc -%s F*' % flags if flags else 'lc F*', 'lc %s %s' % ('-' + flags if flags else '', pat),
                      expect(flags, sel, entries)))
    for pat in ('*.CODE', 'F??.*', 'FILE?.TEXT', 'FQ=', '#5:F1*', 'F-*'):
        sel = [n for n in allf if glob(pat.split(':')[-1]).match(n)]
        cases.append(('lc -C %s' % pat, 'lc -C ' + pat, expect('C', sel, entries)))
    checks = []
    script = ['WAIT "Command:"', 'TYPE "X"', 'WAIT "Execute what file?"', 'TYPE "#5:SHELL\\r"', 'WAIT "shell> "',
              'TYPE "cd #5\\r"', 'WAIT "shell> "']
    for i, (label, cmd, exp) in enumerate(cases):
        script += ['TYPE "%s >#5:O%d.TEXT\\r"' % (cmd, i), 'WAIT "shell> "']
    extra = ['lc -C F0* F1*', 'lc NOSUCH*', 'lc #14:', 'lc -q', 'lc -1 F* | wc -l']
    for c in extra:
        script += ['TYPE "%s\\r"' % c, 'WAIT "shell> "']
    script += ['TYPE "bye\\r"', 'WAIT "Command:"']
    ok, tr, info = ps.run_script('\n'.join(script) + '\n', 3000)
    tr = tr.replace('\r', '\n')
    checks.append(('the session completed', ok))
    img = ucsdvol.Volume(os.path.join(ps.out, 'VERIFY_SOURCE.BLK'))
    for i, (label, cmd, exp) in enumerate(cases):
        n = 'O%d.TEXT' % i
        got = ucsdvol.ucsd_to_text(img.read(n)[0]).split('\n')[:-1] if img.find(n) else None
        good = got == exp
        if not good:
            print('--- %s\n got:\n%s\n want:\n%s' % (label, '\n'.join(got or []), '\n'.join(exp)))
        checks.append((label, good))
    tail = tr[tr.find('shell> lc -C F0*'):]
    two = tail[:tail.find('shell> lc NOSUCH')]
    f0 = [n for n in allf if n.startswith('F0')]
    f1 = [n for n in allf if n.startswith('F1')]
    vol = ps.src and ucsdvol.Volume(ps.src).volname
    checks.append(('two patterns: a header and a layout each', two.count(vol + ':') == 2
                   and all(n in two for n in f0 + f1)))
    checks.append(('no match: said, status 1', 'NOSUCH*: no file matches' in tail and '[exit status 1]' in tail))
    checks.append(('no disk: said, status 2', 'no disk #14 on line' in tail and '[exit status 2]' in tail))
    checks.append(('bad option: said', "invalid option -- 'q'" in tail))
    m = re.search(r'shell> lc -1 F\* \| wc -l\n(.*?)\nshell>', tail, re.S)
    nF = len([n for n in allf if n.startswith('F')])
    checks.append(('lc | wc -l counts the names', bool(m) and str(nF) in m.group(1).split()))
    bad = [n for n, g in checks if not g]
    for n, g in checks:
        print('%-5s %s' % ('ok' if g else 'FAIL', n))
    print('lc test: %s' % ('FAILED' if bad else 'PASSED'))
    return 1 if bad else 0


if __name__ == '__main__':
    sys.exit(main())
