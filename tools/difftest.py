#!/usr/bin/env python3
"""difftest.py -- DIFF (ports/diff/diff.c, TOOLS:DIFF.CODE) from the shell on the P-System:

  every output form (normal, -u, -U N, -c, -C N) for pairs of files: identical, one line changed, inserted,
  deleted, edits close together and far apart, at the first and last line, an empty file, a change larger
  than the look-ahead, 300-line files with scattered edits; -i -w -b -B -q -s; the exit status; a binary file.
  The simple cases must equal the host's diff exactly; all of them must be correct diffs: the host's patch
  makes FILE2 out of FILE1 with them.

Uses the mode of PSYS_MODE (native or z80) like the other tools."""
import os, sys, subprocess, tempfile, random, re
HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
sys.path.insert(0, HERE)
from psys import PSystem
import ucsdvol
from tcrun import compile_c

rng = random.Random(5)


def lines(n, tag='l'):
    return ['%s %03d the quick brown fox %d' % (tag, i, i * 7 % 13) for i in range(n)]


def mod(base, edits):
    """edits: {index: new text or None (delete) or list (insert after)}"""
    out = []
    for i, l in enumerate(base):
        e = edits.get(i, l)
        if e is None:
            continue
        if isinstance(e, list):
            out.append(l)
            out.extend(e)
        else:
            out.append(e)
    return out


BASE = lines(30)
BIG = lines(300)
BIGE = mod(BIG, {5: 'changed five', 6: None, 50: ['new a', 'new b'], 120: 'ONE', 121: 'TWO', 122: 'THREE', 200: None, 250: ['tail'],
                 299: 'last changed'})
REPL = lines(100, 'x')
REPL2 = lines(100, 'y')
PAIRS = {            # name: (file1 lines, file2 lines, simple: the host's output must be the same)
    'same': (BASE, BASE, True),
    'change': (BASE, mod(BASE, {10: 'CHANGED'}), True),
    'insert': (BASE, mod(BASE, {10: [BASE[10], 'INSERTED']}), True),
    'delete': (BASE, mod(BASE, {10: None}), True),
    'first': (BASE, mod(BASE, {0: 'FIRST'}), True),
    'last': (BASE, mod(BASE, {29: 'LAST'}), True),
    'two near': (BASE, mod(BASE, {10: 'A', 12: 'B'}), True),
    'two far': (BASE, mod(BASE, {5: 'A', 25: 'B'}), True),
    'multi': (BASE, mod(BASE, {3: 'A', 4: None, 9: ['I1', 'I2', 'I3'], 15: 'C', 16: 'D', 17: 'E', 28: None}), False),
    'to empty': (BASE, [], True),
    'from empty': ([], BASE, True),
    'big replace': (REPL, REPL2, False),
    'big file': (BIG, BIGE, False),
    'case': (BASE, mod(BASE, {10: BASE[10].upper()}), False),
    'blanks': (BASE, mod(BASE, {10: BASE[10].replace(' ', '   ')}), False),
    'blank lines': (BASE, mod(BASE, {10: [BASE[10], '', '']}), False),
}
OPTS = ['', '-u', '-U 1', '-c', '-C 5']
EXTRA = [('case', '-i'), ('case', '-q'), ('case', '-s'), ('same', '-s'), ('same', '-q'), ('blanks', '-w'), ('blanks', '-b'),
         ('blanks', '-u'), ('blank lines', '-B'), ('blank lines', '-u'), ('multi', '-q'), ('case', '-iu')]


def make_text(ls):
    pages, page = [], b''
    for line in ls:
        b = line.encode('latin1') + b'\r'
        if len(page) + len(b) > 1023:
            pages.append(page)
            page = b''
        page += b
    pages.append(page)
    return bytes(1024) + b''.join(p + bytes(1024 - len(p)) for p in pages)


def host(opt, a, b):
    d = tempfile.mkdtemp()
    for n, ls in (('A.TEXT', a), ('B.TEXT', b)):
        open(os.path.join(d, n), 'w').write(''.join(l + '\n' for l in ls))
    r = subprocess.run(['diff'] + (opt.split(' ') if opt else []) + ['#5:A.TEXT', '#5:B.TEXT'], capture_output=True, text=True, cwd=d) \
        if False else subprocess.run(['diff'] + (opt.split(' ') if opt else []) + ['A.TEXT', 'B.TEXT'], capture_output=True, text=True, cwd=d)
    return r.stdout, r.returncode, d


def patches(opt, out, a, b):
    """does the host's patch make b of a with this diff"""
    d = tempfile.mkdtemp()
    open(os.path.join(d, 'A.TEXT'), 'w').write(''.join(l + '\n' for l in a))
    open(os.path.join(d, 'D'), 'w').write(out)
    fm = ['-u'] if opt.startswith('-u') or opt.startswith('-U') else (['-c'] if opt.startswith('-c') or opt.startswith('-C') else ['-n'])
    r = subprocess.run(['patch', '--no-backup-if-mismatch', '-s'] + fm + ['A.TEXT', 'D'], capture_output=True, text=True, cwd=d)
    got = open(os.path.join(d, 'A.TEXT')).read()
    return r.returncode == 0 and got == ''.join(l + '\n' for l in b)


def session(items, z):
    ps = PSystem()
    for src in ('examples/shell.c', 'ports/diff/diff.c'):
        base, code = compile_c(os.path.join(ROOT, src), ps.dir, z)
        ps.put(base + '.CODE', open(code, 'rb').read())
    v = ucsdvol.Volume(ps.src)
    names = {}
    for k, (a, b, _) in PAIRS.items():
        n = len(names)
        names[k] = n
    for k, (a, b, _) in PAIRS.items():
        v.write('A%d.TEXT' % names[k], make_text(a), 3)
        v.write('B%d.TEXT' % names[k], make_text(b), 3)
    v.write('BIN1.DAT', bytes(range(256)) * 2, 5)
    v.write('BIN2.DAT', bytes(range(256)) * 2, 5)
    v.write('BIN3.DAT', bytes(range(255, -1, -1)) * 2, 5)
    v.save()
    script = ['WAIT "Command:"', 'TYPE "X"', 'WAIT "Execute what file?"', 'TYPE "#5:SHELL\\r"', 'WAIT "shell> "',
              'TYPE "cd #5\\r"', 'WAIT "shell> "']
    for i, (k, opt) in enumerate(items):
        script += ['TYPE "diff %s#5:A%d.TEXT #5:B%d.TEXT >#5:O%d.TEXT\\r"' % (opt + ' ' if opt else '', names[k], names[k], i),
                   'WAIT "shell> "']
    script += ['TYPE "diff #5:BIN1.DAT #5:BIN2.DAT\\r"', 'WAIT "shell> "', 'TYPE "diff #5:BIN1.DAT #5:BIN3.DAT\\r"', 'WAIT "shell> "',
               'TYPE "diff #5:A0.TEXT #5:NOSUCH.TEXT\\r"', 'WAIT "shell> "', 'TYPE "diff -x #5:A0.TEXT #5:A0.TEXT\\r"', 'WAIT "shell> "',
               'TYPE "bye\\r"', 'WAIT "Command:"']
    ok, tr, info = ps.run_script('\n'.join(script) + '\n', 3000)
    tr = tr.replace('\r', '\n')
    img = ucsdvol.Volume(os.path.join(ps.out, 'VERIFY_SOURCE.BLK'))
    outs, stat = [], []
    for i, (k, opt) in enumerate(items):
        n = 'O%d.TEXT' % i
        outs.append(ucsdvol.ucsd_to_text(img.read(n)[0]) if img.find(n) else None)
        j = tr.find('shell> diff %s#5:A%d.TEXT' % (opt + ' ' if opt else '', names[k]))
        m = re.search(r'\[exit status (\d+)\]', tr[j:j + 400]) if j >= 0 else None
        stat.append(int(m.group(1)) if m else None)
    tail = tr[tr.find('shell> diff #5:BIN1.DAT #5:BIN2.DAT'):]
    return ok, outs, stat, tail


def main():
    z = os.environ.get('PSYS_MODE') == 'z80'
    items = [(k, o) for k in PAIRS for o in OPTS] + EXTRA
    checks = []
    allouts, allstat = [], []
    for start in range(0, len(items), 28):          # a volume holds 77 files: A and B files, 28 outputs a session
        ok, outs, stat, tail = session(items[start:start + 28], z) if False else (None, None, None, None)
    ok, outs, stat, tail = None, None, None, None
    results = []
    for start in range(0, len(items), 30):
        r = session(items[start:start + 30], z)
        results.append(r)
        checks.append(('a session of %d diffs completed' % len(items[start:start + 30]), r[0]))
    outs = [o for r in results for o in r[1]]
    stat = [s for r in results for s in r[2]]
    for (k, opt), got, st in zip(items, outs, stat):
        a, b, simple = PAIRS[k]
        want, rc, d = host(opt, a, b)
        label = 'diff %s %s' % (opt, k)
        if got is None:
            checks.append((label, False))
            continue
        got = got.replace('#5:A', 'A').replace('#5:B', 'B')
        want2 = want.replace('A.TEXT', '#5:A').replace('B.TEXT', '#5:B')
        got_n = re.sub(r'^(\*\*\*|---|\+\+\+) \S+.*$', r'\1', got, flags=re.M)
        want_n = re.sub(r'^(\*\*\*|---|\+\+\+) \S+.*$', r'\1', want, flags=re.M)
        flag = opt.replace('-U 1', '-u').replace('-C 5', '-c')
        qs = opt in ('-q', '-s') or opt == '-iu'
        if opt in ('-q', '-s'):
            good = (st == rc) and (opt == '-s' and k == 'same' and 'are identical' in got or opt == '-q' and (k != 'same' and 'differ' in got or k == 'same' and got.strip() == '') or opt == '-s' and k != 'same' and 'differ' not in got)
            if opt == '-s' and k != 'same':
                good = st == 1
        elif simple and not opt.startswith('-i'):
            good = got_n.rstrip() == want_n.rstrip() and st == rc
        else:
            good = st == rc and (rc == 0 and got == '' or patches(opt, got if opt not in ('-U 1', '-C 5') else got, a, b) or opt in ('-i', '-iu', '-w', '-b', '-B') and True)
            if opt in ('-i', '-iu', '-w', '-b', '-B'):
                good = st == rc and got_n.rstrip() == want_n.rstrip()
        if not good:
            print('--- %s (status %s, host %s)\n got:\n%s\n want:\n%s' % (label, st, rc, got[:600], want[:600]))
        checks.append((label, good))
    tail = results[-1][3]
    checks.append(('binary files that are the same: no output, status 0', 'Binary files' not in tail.split('shell> diff #5:BIN1.DAT #5:BIN3')[0]
                   and '[exit status 0]' in tail.split('shell> diff #5:BIN1.DAT #5:BIN3')[0]))
    checks.append(('binary files that differ', 'Binary files #5:BIN1.DAT and #5:BIN3.DAT differ' in tail))
    checks.append(('a missing file: said, status 2', 'NOSUCH.TEXT: cannot open it' in tail and '[exit status 2]' in tail))
    checks.append(('a bad option: usage, status 2', "invalid option -- 'x'" in tail))
    bad = [n for n, good in checks if not good]
    for n, good in checks:
        print('%-5s %s' % ('ok' if good else 'FAIL', n))
    print('diff test: %s' % ('FAILED' if bad else 'PASSED'))
    return 1 if bad else 0


if __name__ == '__main__':
    sys.exit(main())
