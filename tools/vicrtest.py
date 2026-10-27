#!/usr/bin/env python3
"""vicrtest.py -- a file that is not a UCSD text file, but lines that end in CR (a compiler's listing:
UCSD-BASIC's NAME.LST, a data file with its last block filled out with NULs), in vi and the shell's
TYPE; and a binary file, which they refuse.  On the P-System, from the shell:

  TYPE #5:X.LST                     -> its lines (no NULs)
  TYPE #5:B.DAT, TYPE #5:VI.CODE    -> "not a text file", nothing of them shown
  vi #5:X.LST, G, o (a new last line), ZZ          -> 201 lines, each ending in CR (no LF), with VI.SWAP
  CD #9 (a tiny prefix volume: no room for VI.SWAP, the whole file in memory), vi #5:Y.LST (a small
  one: Z80 mode has little memory), dd, ZZ  -> line 1 gone, the rest as it was, CR line ends
  vi #5:B.DAT                       -> "not a file of lines (a binary file)", the file unchanged
Uses the mode of PSYS_MODE (native or z80) like the other tools."""
import os, sys
HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
sys.path.insert(0, HERE)
from psys import PSystem
from tcrun import compile_c, compile_modules
from mkvolume import VI_MODULES
import ucsdvol

N = 200
LINES = ['%5d  listing line %d of the file' % (i, i) for i in range(1, N + 1)]
SMALL = LINES[:30]


def crfile(lines):
    """lines ending in CR, the last block filled out with NULs (as a P-System program writes them)"""
    raw = ''.join(l + '\r' for l in lines).encode('latin1')
    return raw + bytes(-len(raw) % 512)


def main():
    ps = PSystem()
    code = compile_modules([os.path.join(os.environ.get('VIROOT', ROOT), m) for m in VI_MODULES], os.path.join(ps.dir, 'VI.CODE'), True)
    ps.put('VI.CODE', open(code, 'rb').read())
    base, sc = compile_c(os.path.join(ROOT, 'examples', 'shell.c'), ps.dir)
    ps.put('SHELL.CODE', open(sc, 'rb').read())
    v = ucsdvol.Volume(ps.src)
    v.write('X.LST', crfile(LINES), 5)
    v.write('Y.LST', crfile(SMALL), 5)
    binary = bytes(range(256)) * 4
    v.write('B.DAT', binary, 5)
    v.save()
    ucsdvol.main(['new', ps.spare, 'TINY', '12'])        # (#9: a prefix with no room for VI.SWAP)
    script = ['WAIT "Command:"', 'TYPE "X"', 'WAIT "Execute what file?"', 'TYPE "#5:SHELL\\r"', 'WAIT "shell> "',
              'TYPE "type #5:X.LST\\r"', 'WAIT "shell> "',
              'TYPE "type #5:B.DAT\\r"', 'WAIT "shell> "', 'TYPE "type #5:VI.CODE\\r"', 'WAIT "shell> "',
              'TYPE "#5:VI #5:X.LST\\r"', 'WAIT "X.LST"', 'TYPE "Goadded at the end\\x1bZZ"', 'WAIT "shell> "',
              'TYPE "cd #9\\r"', 'WAIT "shell> "',
              'TYPE "#5:VI #5:Y.LST\\r"', 'WAIT "Y.LST"', 'TYPE "ddZZ"', 'WAIT "shell> "',
              'TYPE "#5:VI #5:B.DAT\\r"', 'WAIT "shell> "',
              'TYPE "bye\\r"', 'WAIT "Command:"']
    ok, tr, info = ps.run_script('\n'.join(script) + '\n', 900)
    tr = tr.replace('\r', '\n')
    img = ucsdvol.Volume(os.path.join(ps.out, 'VERIFY_SOURCE.BLK'))
    x = img.read('X.LST')[0]
    body = x.rstrip(b'\0')
    lines = body.decode('latin1').split('\r')
    typed = tr[tr.find('type #5:X.LST'):tr.find('type #5:B.DAT')]
    y = img.read('Y.LST')[0].rstrip(b'\0')
    checks = [
        ('the script completed', ok),
        ('TYPE shows the lines of X.LST', all(l in typed for l in (LINES[0], LINES[99], LINES[-1])) and '\0' not in typed),
        ('TYPE refuses B.DAT and VI.CODE, showing nothing of them',
         tr.count('not a text file') == 2 and '\x01\x02\x03' not in tr),
        ('vi refuses B.DAT', 'B.DAT: not a file of lines (a binary file)' in tr and img.read('B.DAT')[0] == binary),
        ('X.LST: CR line ends, no LF, still a data file', b'\n' not in body and body.endswith(b'\r') and img.find('X.LST')[2] & 15 == 5),
        ('X.LST: a line added at the end (with VI.SWAP; autoindent: 2 blanks)',
         lines[:-1] == LINES + ['  added at the end'] and lines[-1] == ''),
        ('Y.LST: line 1 gone (no VI.SWAP), CR line ends', y == ''.join(l + '\r' for l in SMALL[1:]).encode('latin1')),
        ('the second vi had no VI.SWAP (the tiny prefix)', not ucsdvol.Volume(os.path.join(ps.out, 'VERIFY_SPARE.BLK')).find('VI.SWAP')
         if os.path.exists(os.path.join(ps.out, 'VERIFY_SPARE.BLK')) else True),
    ]
    bad = 0
    for what, good in checks:
        print('%-4s %s' % ('ok' if good else 'BAD', what))
        bad += not good
    if bad:
        print(tr[tr.find('cd #9'):][:2500])
        print('X.LST:', img.find('X.LST'), repr(body[:120]), repr(body[-200:]), len(lines))
    print('vi CR-file test (%s): %s   (work: %s)' % (os.environ.get('PSYS_MODE', 'native'), 'FAILED' if bad else 'PASSED', ps.dir))
    return 1 if bad else 0


if __name__ == '__main__':
    sys.exit(main())
