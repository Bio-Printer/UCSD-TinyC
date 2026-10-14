#!/usr/bin/env python3
"""xsubvoltest.py -- X(ecute at the Command: prompt with a path into sub-volumes (the OS of
U134.4_OS_SOURCE_v1.13, BIGGY 1.26: GETCMD's SUBEXEC), on the P-System.

WORK: (#5) holds the sub-volume A (made on the host) with HELLO.CODE and T.TEXT, and B inside A
with HELLO2.CODE; HELLO.CODE is on WORK: itself too (it says so: not the one in A).

  X #5:A/HELLO          the code file in A runs where it is
  X #5:/A/B/HELLO2      a / after the colon, a nested sub-volume
  X #5:\\a\\b\\hello2      backslashes and lower case
  F(iler P(refix #5, X A/HELLO, X /A/HELLO   on the prefix volume
  X #5:A/HELLO2.CODE.   an exact name (the trailing .)
  X #5:HELLO            no path: as before (the one on WORK:)
  X #5:A/NOPE, X #5:NOPE/X, X #5:A/T.TEXT.   No file ..., ... not code
Uses the mode of PSYS_MODE (native or z80) like the other tools."""
import os, sys, re, tempfile
HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
sys.path.insert(0, HERE)
from psys import PSystem
from tcrun import compile_c
import ucsdvol


def program(d, name, text):
    src = os.path.join(d, name.lower() + '.c')
    open(src, 'w').write('#include <stdio.h>\nint main(void) { printf("%s\\n"); return 0; }\n' % text)
    b, code = compile_c(src, d)
    return open(code, 'rb').read()


def main():
    z = os.environ.get('PSYS_MODE') == 'z80'
    ps = PSystem()
    d = tempfile.mkdtemp(prefix='xsubvol_')
    hello_a = program(d, 'HELLO', 'hello from A')
    hello_w = program(os.path.join(d, ''), 'HELLOW', 'hello from WORK')
    hello2 = program(d, 'HELLO2', 'hello2 from A/B')
    b = os.path.join(d, 'B.BLK')
    ucsdvol.main(['new', b, 'B', '60'])
    v = ucsdvol.Volume(b)
    v.write('HELLO2.CODE', hello2, 2)
    v.save()
    a = os.path.join(d, 'A.BLK')
    ucsdvol.main(['new', a, 'A', '200'])
    v = ucsdvol.Volume(a)
    v.write('HELLO.CODE', hello_a, 2)
    v.write('T.TEXT', ucsdvol.text_to_ucsd('text\n'), 3)
    v.write('B', open(b, 'rb').read()[:60 * 512], 5)
    v.save()
    v = ucsdvol.Volume(ps.src)
    v.write('HELLO.CODE', hello_w, 2)
    v.write('A', open(a, 'rb').read()[:200 * 512], 5)
    v.save()
    runs = ['#5:A/HELLO', '#5:/A/B/HELLO2', '#5:\\\\a\\\\b\\\\hello2', None, 'A/HELLO', '/A/HELLO',
            '#5:A/HELLO2.CODE.', '#5:A/B/HELLO2.CODE.', '#5:HELLO', '#5:A/NOPE', '#5:NOPE/X', '#5:A/T.TEXT.']
    s = ['WAIT "Command:"']
    for r in runs:
        if r is None:                   # the prefix: #5
            s += ['TYPE "F"', 'WAIT "Filer:"', 'TYPE "P"', 'WAIT "Prefix"', 'TYPE "#5\\r"', 'WAIT "Filer:"',
                  'TYPE "Q"', 'WAIT "Command:"']
            continue
        s += ['TYPE "X"', 'WAIT "Execute what file?"', 'TYPE "%s\\r"' % r, 'WAIT "Command:"']
    ok, tr, info = ps.run_script('\n'.join(s) + '\n', 1200 if z else 200)
    tr = re.sub(r'\n+', '\n', tr.replace('\r', '\n'))

    def part(r):
        r = r.replace('\\\\', '\\')
        i = tr.find('Execute what file? ' + r + '\n')
        return tr[i:tr.find('Command:', i)] if i >= 0 else ''

    print('\n'.join(l for l in tr.split('\n') if l.startswith('Execute') or 'hello' in l or 'No file' in l or 'not code' in l))
    checks = [('the script ran to its end', ok),
              ('X #5:A/HELLO: the one in A', 'hello from A' in part('#5:A/HELLO')),
              ('X #5:/A/B/HELLO2: nested, / after the colon', 'hello2 from A/B' in part('#5:/A/B/HELLO2')),
              ('X #5:\\a\\b\\hello2: backslashes, lower case', 'hello2 from A/B' in part('#5:\\\\a\\\\b\\\\hello2')),
              ('prefix #5: X A/HELLO and X /A/HELLO', 'hello from A' in part('A/HELLO') and 'hello from A' in part('/A/HELLO')),
              ('X #5:A/B/HELLO2.CODE. (an exact name)', 'hello2 from A/B' in part('#5:A/B/HELLO2.CODE.')),
              ('X #5:A/HELLO2.CODE.: not in A itself: No file', 'No file #5:A/HELLO2.CODE' in part('#5:A/HELLO2.CODE.')),
              ('X #5:HELLO (no path): the one on WORK:, as before', 'hello from WORK' in part('#5:HELLO')),
              ('X #5:A/NOPE, #5:NOPE/X: No file', 'No file #5:A/NOPE' in part('#5:A/NOPE') and 'No file #5:NOPE/X' in part('#5:NOPE/X')),
              ('X #5:A/T.TEXT.: not code', 'not code' in part('#5:A/T.TEXT.'))]
    bad = 0
    for what, good in checks:
        print('%-4s  %s' % ('ok' if good else 'BAD', what))
        bad += not good
    if bad:
        print(tr[-2000:])
        print(info[-600:])
    print('X(ecute sub-volume test (%s): %s   (work: %s)' % ('z80' if z else 'native', 'FAILED' if bad else 'PASSED', ps.dir))
    return 1 if bad else 0


if __name__ == '__main__':
    sys.exit(main())
