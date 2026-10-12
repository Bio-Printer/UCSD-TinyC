#!/usr/bin/env python3
"""patchtest.py -- PATCH (ports/patch/patch.c, TOOLS:PATCH.CODE) from the shell on the P-System:

  patches made by the host's diff in the unified, context and normal forms (-u, -U 1, -c, -C 5, normal) for
  the pairs of files that difftest uses: the result must equal the second file; a patch whose hunks are
  displaced (lines added in front), one with fuzz (changed context), -R, -b, -o, -C (check only), the file
  names from the headers, a hunk that fails (reject file, status 1), a missing file.

Uses the mode of PSYS_MODE (native or z80) like the other tools."""
import os, sys, subprocess, tempfile, re
HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
sys.path.insert(0, HERE)
from psys import PSystem
import ucsdvol
from tcrun import compile_c
from difftest import PAIRS, make_text, lines, mod

FORMS = ['', '-u', '-U 1', '-c', '-C 5']
CASES_PER_SESSION = 24


def hostdiff(opt, a, b):
    d = tempfile.mkdtemp()
    for n, ls in (('A.TEXT', a), ('B.TEXT', b)):
        open(os.path.join(d, n), 'w').write(''.join(l + '\n' for l in ls))
    r = subprocess.run(['diff'] + (opt.split(' ') if opt else []) + ['A.TEXT', 'B.TEXT'], capture_output=True, text=True, cwd=d)
    return r.stdout


def totext(ls):
    return make_text(ls)


def getlines(img, name):
    if not img.find(name):
        return None
    t = ucsdvol.ucsd_to_text(img.read(name)[0])
    r = t.split('\n')[:-1]
    return [] if r == [''] else r      # ucsd_to_text gives one empty line for an empty file


def run(cases, z):
    """cases: list of dicts: cmd (the shell line, with {n} for case number), files {name: lines}, patch text"""
    ps = PSystem()
    for src in ('examples/shell.c', 'ports/patch/patch.c'):
        base, code = compile_c(os.path.join(ROOT, src), ps.dir, z)
        ps.put(base + '.CODE', open(code, 'rb').read())
    v = ucsdvol.Volume(ps.src)
    for c in cases:
        for n, ls in c['files'].items():
            v.write(n, totext(ls), 3)
        v.write(c['pname'], totext(c['patch'].split('\n')[:-1] if c['patch'].endswith('\n') else c['patch'].split('\n')), 3)
    v.save()
    script = ['WAIT "Command:"', 'TYPE "X"', 'WAIT "Execute what file?"', 'TYPE "#5:SHELL\\r"', 'WAIT "shell> "',
              'TYPE "cd #5\\r"', 'WAIT "shell> "']
    for c in cases:
        script += ['TYPE "%s\\r"' % c['cmd'], 'WAIT "shell> "']
    script += ['TYPE "bye\\r"', 'WAIT "Command:"']
    ok, tr, info = ps.run_script('\n'.join(script) + '\n', 3000)
    tr = tr.replace('\r', '\n')
    img = ucsdvol.Volume(os.path.join(ps.out, 'VERIFY_SOURCE.BLK'))
    res = []
    for c in cases:
        j = tr.find('shell> ' + c['cmd'])
        seg = tr[j:tr.find('shell> ', j + 8)] if j >= 0 else ''
        m = re.search(r'\[exit status (\d+)\]', seg)
        res.append((int(m.group(1)) if m else 0, seg, img))
    return ok, res


def main():
    z = os.environ.get('PSYS_MODE') == 'z80'
    cases = []
    k = 0

    def add(label, cmd, files, patch, check):
        nonlocal k
        k += 1
        cases.append(dict(label=label, cmd=cmd, files=files, patch=patch, pname='P%d.TEXT' % k, check=check, n=k))

    def got(img, name):
        return getlines(img, name)

    PAIRS['large hunk'] = (lines(85, 'x'), lines(85, 'y'), False)
    del PAIRS['big replace']
    for name, (a, b, _) in PAIRS.items():
        for opt in FORMS:
            n = len(cases) + 1
            p = hostdiff(opt, a, b)
            if not p:
                continue
            An = 'A%d.TEXT' % n
            add('%s %s' % (opt or 'normal', name), 'patch -i #5:P%d.TEXT #5:%s' % (n, An), {An: a}, p,
                lambda r, img, An=An, b=b: r[0] == 0 and got(img, An) == b)
    # headers give the names (unified and context)
    for opt in ('-u', '-c'):
        a, b = PAIRS['multi'][0], PAIRS['multi'][1]
        n = len(cases) + 1
        An = 'H%d.TEXT' % n
        p = re.sub(r'A\.TEXT', An, hostdiff(opt, a, b))
        p = re.sub(r'B\.TEXT', An, p)
        add('names from the %s headers' % opt, 'patch -i #5:P%d.TEXT' % n, {An: a}, p,
            lambda r, img, An=An, b=b: r[0] == 0 and got(img, An) == b)
    # displaced hunks: lines added in front
    a, b = PAIRS['two far'][0], PAIRS['two far'][1]
    n = len(cases) + 1
    An = 'D%d.TEXT' % n
    add('displaced hunks (offset)', 'patch -i #5:P%d.TEXT #5:%s' % (n, An), {An: ['new1', 'new2', 'new3'] + a}, hostdiff('-u', a, b),
        lambda r, img, An=An, b=b: r[0] == 0 and got(img, An) == ['new1', 'new2', 'new3'] + b and 'offset' in r[1])
    # fuzz: a context line changed
    n = len(cases) + 1
    An = 'F%d.TEXT' % n
    a, b = PAIRS['change'][0], PAIRS['change'][1]
    a2 = list(a)
    a2[7] = 'something else'
    b2 = list(b)
    b2[7] = 'something else'
    add('fuzz (a context line differs)', 'patch -i #5:P%d.TEXT #5:%s' % (n, An), {An: a2}, hostdiff('-u', a, b),
        lambda r, img, An=An, b2=b2: r[0] == 0 and got(img, An) == b2 and 'fuzz' in r[1])
    # reversed
    n = len(cases) + 1
    An = 'R%d.TEXT' % n
    a, b = PAIRS['multi'][0], PAIRS['multi'][1]
    add('-R', 'patch -R -i #5:P%d.TEXT #5:%s' % (n, An), {An: b}, hostdiff('-u', a, b),
        lambda r, img, An=An, a=a: r[0] == 0 and got(img, An) == a)
    # -b
    n = len(cases) + 1
    An = 'K%d.TEXT' % n
    add('-b keeps the original', 'patch -b -i #5:P%d.TEXT #5:%s' % (n, An), {An: a}, hostdiff('-u', a, b),
        lambda r, img, An=An, a=a, b=b: r[0] == 0 and got(img, An) == b and got(img, An.replace('.TEXT', '.ORIG.TEXT')) == a)
    # -o
    n = len(cases) + 1
    An = 'L%d.TEXT' % n
    add('-o writes elsewhere', 'patch -o #5:OUT%d.TEXT -i #5:P%d.TEXT #5:%s' % (n, n, An), {An: a}, hostdiff('-u', a, b),
        lambda r, img, An=An, a=a, b=b, n=n: r[0] == 0 and got(img, An) == a and got(img, 'OUT%d.TEXT' % n) == b)
    # -C
    n = len(cases) + 1
    An = 'M%d.TEXT' % n
    add('-C changes nothing', 'patch -C -i #5:P%d.TEXT #5:%s' % (n, An), {An: a}, hostdiff('-u', a, b),
        lambda r, img, An=An, a=a: r[0] == 0 and got(img, An) == a)
    # failure
    n = len(cases) + 1
    An = 'N%d.TEXT' % n
    bad = ['nothing like it %d' % i for i in range(30)]
    add('a hunk that fails: reject file, status 1', 'patch -i #5:P%d.TEXT #5:%s' % (n, An), {An: bad}, hostdiff('-u', a, b),
        lambda r, img, An=An, bad=bad: r[0] == 1 and 'FAILED' in r[1] and got(img, An) == bad
        and got(img, An.replace('.TEXT', '.REJ.TEXT')) is not None)
    # the patch from the standard input
    n = len(cases) + 1
    An = 'S%d.TEXT' % n
    add('the patch from the standard input', 'patch #5:%s < #5:P%d.TEXT' % (An, n), {An: a}, hostdiff('-u', a, b),
        lambda r, img, An=An, b=b: r[0] == 0 and got(img, An) == b)
    # two files in one patch, -p1
    n = len(cases) + 1
    A1, A2 = 'T%da.TEXT' % n, 'T%db.TEXT' % n
    a1, b1 = PAIRS['change'][0], PAIRS['change'][1]
    a2_, b2_ = PAIRS['insert'][0], PAIRS['insert'][1]
    pt = hostdiff('-u', a1, b1).replace('A.TEXT', 'x/' + A1).replace('B.TEXT', 'x/' + A1) + \
        hostdiff('-u', a2_, b2_).replace('A.TEXT', 'x/' + A2).replace('B.TEXT', 'x/' + A2)
    add('two files, -p1', 'patch -p1 -i #5:P%d.TEXT' % n, {A1: a1, A2: a2_}, pt,
        lambda r, img, A1=A1, A2=A2, b1=b1, b2_=b2_: r[0] == 0 and got(img, A1) == b1 and got(img, A2) == b2_)
    # missing file
    n = len(cases) + 1
    add('a missing patch file: status 2', 'patch -i #5:NOSUCH.TEXT #5:Z.TEXT', {}, 'x\n',
        lambda r, img: r[0] == 2 and 'cannot open' in r[1])

    checks = []
    for s in range(0, len(cases), CASES_PER_SESSION):
        chunk = cases[s:s + CASES_PER_SESSION]
        ok, res = run(chunk, z)
        checks.append(('a session of %d patches completed' % len(chunk), ok))
        for c, r in zip(chunk, res):
            good = False
            try:
                good = bool(c['check'](r, r[2]))
            except Exception as e:
                print('exception', c['label'], e)
            if not good:
                print('--- FAIL %s\n%s' % (c['label'], r[1][:500]))
                for nm in c['files']:
                    print('   %s now: %r' % (nm, (getlines(r[2], nm) or [])[:6]))
            checks.append((c['label'], good))
    bad = [n for n, g in checks if not g]
    for n, g in checks:
        print('%-5s %s' % ('ok' if g else 'FAIL', n))
    print('patch test: %s' % ('FAILED' if bad else 'PASSED'))
    return 1 if bad else 0


if __name__ == '__main__':
    sys.exit(main())
