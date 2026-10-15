#!/usr/bin/env python3
"""visubvoltest.py -- vi on a text file in a sub-volume inside a sub-volume (the shell's CD), on the
P-System: MKDIR #5:TMP 500, MKDIR #5:/TMP/SRC, COPY ALL.TEXT into it, CD #5:/TMP/SRC, VI ALL.TEXT,
3j dd dd ZZ; then VI ALL.TEXT again (it opens where it was left: the header page, written by the
file's place, struct svat), ZZ.  The file (600 lines, 12 KB) is bigger than vi's window in Z80 mode:
it is loaded lazily from the sub-volume's blocks and saved by its place (the library's SVAT segment,
not DIRS).  On the host: the file without the two lines, the header page's place, no VI.SWAP left
(it is on the prefix volume, :VI.SWAP, not in the sub-volume).  Uses the mode of PSYS_MODE (native
or z80) like the other tools."""
import os, re, sys
HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
sys.path.insert(0, HERE)
from psys import PSystem
from tcrun import compile_c, compile_modules
from mkvolume import VI_MODULES
import ucsdvol


def inner(path, name):
    v = ucsdvol.Volume(path)
    e = [e for e in v.entries if e[3] == name][0]
    p = path + '.' + name
    open(p, 'wb').write(open(path, 'rb').read()[e[0] * 512:e[1] * 512])
    return p


def main():
    z = os.environ.get('PSYS_MODE') == 'z80'
    ps = PSystem()
    b, code = compile_c(os.path.join(ROOT, 'examples/shell.c'), ps.dir)
    ps.put('SHELL.CODE', open(code, 'rb').read())
    vi = compile_modules([os.path.join(ROOT, m) for m in VI_MODULES], os.path.join(ps.dir, 'VI.CODE'), z)
    ps.put('VI.CODE', open(vi, 'rb').read())
    lines = []
    for i in range(300):
        lines += ['; comment line %d' % i, '/Z /C MOD%d' % i]
    ps.put('ALL.TEXT', '\n'.join(lines) + '\n')
    s = ['WAIT "Command:"', 'TYPE "X"', 'WAIT "Execute what file?"', 'TYPE "#5:SHELL\\r"', 'WAIT "shell> "']
    for c, w in (('mkdir #5:tmp 500', 'sub-volume'), ('mkdir #5:/tmp/src', 'sub-volume'),
                 ('copy #5:all.text #5:/tmp/src', 'ALL.TEXT'), ('cd #5:/tmp/src', 'path is')):
        s += ['TYPE "%s\\r"' % c, 'WAIT "%s"' % w, 'WAIT "shell> "']
    s += ['TYPE "#5:vi all.text\\r"', 'WAIT "comment line 1"', 'TYPE "3j"', 'TYPE "dd"', 'TYPE "dd"', 'TYPE "ZZ"',
          'WAIT "exit status 0"', 'WAIT "shell> "',
          'TYPE "#5:vi all.text\\r"', 'WAIT "all.text"', 'TYPE "ZZ"', 'WAIT "exit status 0"', 'WAIT "shell> "']
    s += ['TYPE "bye\\r"', 'WAIT "Command:"']
    ok, tr, info = ps.run_script('\n'.join(s) + '\n', 1800 if z else 300)
    tr = tr.replace('\r', '\n')
    img = os.path.join(ps.out, 'VERIFY_SOURCE.BLK')
    try:
        work = sorted(e[3] for e in ucsdvol.Volume(img).entries)
        v = ucsdvol.Volume(inner(inner(img, 'TMP'), 'SRC'))
        src = sorted(e[3] for e in v.entries)
        raw = v.read('ALL.TEXT')[0]
        got = [l for l in ucsdvol.ucsd_to_text(raw).split('\n') if l]
        hdr = raw[1000:1004]
    except (Exception, SystemExit) as x:
        work, src, got, hdr = ['(%s)' % x], [], [], b''
    want = lines[:3] + lines[5:]
    second = tr[tr.rfind('vi all.text'):]
    checks = [('the script ran to its end: vi started, ZZ saved and ended (status 0), twice', ok),
              ('ALL.TEXT in /TMP/SRC without the two lines (%d lines)' % len(got), got == want),
              ('its header page keeps the place: VI, line 4', hdr[:3] == b'VI\x01'),
              ('the second vi opens at line 4', re.search(r'line 4/598', second) is not None),
              ('no VI.SWAP left, in SRC or on WORK: (%s; %s)' % (' '.join(work), ' '.join(src)),
               'VI.SWAP' not in work and src == ['ALL.TEXT'])]
    bad = 0
    for what, good in checks:
        print('%-4s  %s' % ('ok' if good else 'BAD', what))
        bad += not good
    if bad:
        print(re.sub(r'\n+', '\n', tr)[-2500:])
        print(info[-600:])
    print('vi sub-volume test (%s): %s   (work: %s)' % ('z80' if z else 'native', 'FAILED' if bad else 'PASSED', ps.dir))
    return 1 if bad else 0


if __name__ == '__main__':
    sys.exit(main())
