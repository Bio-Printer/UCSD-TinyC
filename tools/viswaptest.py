#!/usr/bin/env python3
"""viswaptest.py -- vi saves a file on the volume that holds VI.SWAP (the prefix volume).

VI.SWAP is made the size of the file and 25 KB more, then closed (the rest of the volume is free again) and
opened again to read and write; before, it took the largest free area of the prefix volume and a :w or ZZ of
a file on that volume found no room ("I/O error").  On the P-System: the Filer's P(refix sets #5 (WORK:,
whose free space is one area at its end), then from the shell:

  vi A.C (vi's vicmd.c), x on line 101, ZZ        -> A.C written, that character gone
  vi B.C (300 short lines), 200G, dd, :w, ZZ      -> B.C written without line 200
  vi C.LIST (Big_Disk's SYSTEM.LIST: a compiler's listing, a data file of 146 KB with CR line ends),
  1Gdd, ZZ                                        -> written with CR line ends, without its first line
  vi C.LIST again, G                              -> its 2487 lines, no NULs from the filled-out last block
  vi C.LIST again, 687G, 60 times k, dd, ZZ       -> line 627 is the one gone: lines above the window keep coming in (the
                                                     lines going out join the chunk below when they fit,
                                                     so VI.SWAP's slots do not run out)
  vi C.LIST again, 687G, 5 times ^B (Page Up), H, dd, ZZ
                                                  -> a line about 5 screens up is the one gone (the window
                                                     can hold not much more than a screen in Z80 mode: the
                                                     lines leaving the bottom of the screen make room)
  no VI.SWAP left on the volume afterwards
Uses the mode of PSYS_MODE (native or z80) like the other tools."""
import os, sys
HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
sys.path.insert(0, HERE)
from psys import PSystem
from tcrun import compile_c, compile_modules
from mkvolume import VI_MODULES
from difftest import make_text
import ucsdvol


def main():
    z = os.environ.get('PSYS_MODE') == 'z80'
    ps = PSystem()
    code = compile_modules([os.path.join(ROOT, m) for m in VI_MODULES], os.path.join(ps.dir, 'VI.CODE'), True)
    ps.put('VI.CODE', open(code, 'rb').read())
    base, sc = compile_c(os.path.join(ROOT, 'examples', 'shell.c'), ps.dir)
    ps.put('SHELL.CODE', open(sc, 'rb').read())
    a = open(os.path.join(ROOT, 'ports', 'vi', 'vicmd.c')).read().expandtabs(8).split('\n')[:-1]
    b = ['line %d of the file' % i for i in range(1, 301)]
    v = ucsdvol.Volume(ps.src)
    v.write('A.C', ucsdvol.text_to_ucsd('\n'.join(a) + '\n'), 3)    # (tabs as spaces: as the P-System keeps them)
    v.write('B.C', make_text(b), 3)
    c = ucsdvol.Volume(os.path.join(ROOT, 'build', 'BIGGY.BLK')).read('SYSTEM.LIST')[0]
    v.write('C.LIST', c, 5)
    v.save()
    script = ['WAIT "Command:"', 'TYPE "F"', 'WAIT "Filer:"', 'TYPE "P"', 'WAIT "?"', 'TYPE "#5\\r"',
              'WAIT "Filer:"', 'TYPE "Q"', 'WAIT "Command:"',
              'TYPE "X"', 'WAIT "Execute what file?"', 'TYPE "SHELL\\r"', 'WAIT "shell> "',
              'TYPE "vi A.C\\r"', 'WAIT "A.C"', 'TYPE "101Gx"', 'TYPE "ZZ"', 'WAIT "shell> "',
              'TYPE "vi B.C\\r"', 'WAIT "B.C"', 'TYPE "200Gdd:w\\r"', 'WAIT "299L"', 'TYPE "ZZ"', 'WAIT "shell> "',
              'TYPE "vi C.LIST\\r"', 'WAIT "C.LIST"', 'TYPE "1Gdd"', 'TYPE "ZZ"', 'WAIT "shell> "',
              'TYPE "vi C.LIST\\r"', 'WAIT "C.LIST"', 'TYPE "G"', 'WAIT "2487/2487"', 'TYPE ":q\\r"', 'WAIT "shell> "',
              'TYPE "vi C.LIST\\r"', 'WAIT "C.LIST"', 'WAIT "2487 0%"', 'TYPE "687G"', 'TYPE "%s"' % ('k' * 60),
              'TYPE "ddZZ"', 'WAIT "shell> "',
              'TYPE "vi C.LIST\\r"', 'WAIT "C.LIST"', 'WAIT "2486 0%"', 'TYPE "687G"', 'TYPE "%s"' % ('\x02' * 5),
              'TYPE "HddZZ"', 'WAIT "shell> "',
              'TYPE "bye\\r"', 'WAIT "Command:"']
    ok, tr, info = ps.run_script('\n'.join(script) + '\n', 1800 if z else 300)
    tr = tr.replace('\r', '\n')
    img = ucsdvol.Volume(os.path.join(ps.out, 'VERIFY_SOURCE.BLK'))
    text = lambda n: ucsdvol.ucsd_to_text(img.read(n)[0]).split('\n')[:-1]
    a2, b2 = [text(n) if img.find(n) else [] for n in ('A.C', 'B.C')]
    i = 100
    j = len(a[i]) - len(a[i].lstrip())                 # (G: the first character that is not blank)
    ea = a[:i] + [a[i][:j] + a[i][j + 1:]] + a[i + 1:]
    eb = b[:199] + b[200:]
    checks = [('the script ran to its end', ok),
              ('no I/O error on the screen', 'I/O error' not in tr and 'Write error: I/O' not in tr),
              ('A.C written with the x on line 101', a2 == ea),
              ('B.C written without line 200', b2 == eb)]
    c2 = img.read('C.LIST')[0] if img.find('C.LIST') else b''
    ec = c[c.index(b'\r') + 1:].split(b'\r')          # (the first dd)
    ec = ec[:626] + ec[627:]                           # (the dd after 687G and 60 k)
    got = c2.rstrip(b'\0').split(b'\r')
    d = next((n for n in range(len(got)) if got[n] != ec[n]), len(got))   # the line the dd after ^B took
    checks.append(('C.LIST (CR line ends) written: only the lines vi deleted are gone, no NULs but the last block\'s',
                   got == ec[:d] + ec[d + 1:] and c2.rstrip(b'\0').endswith(b'\r')))
    i = tr.find('shell> vi C.LIST', tr.find('shell> vi C.LIST') + 1)
    j = tr.find('shell> vi C.LIST', i + 1)
    checks.append(('C.LIST again: 2487 lines, no ^@', i >= 0 and '2487/2487' in tr[i:j] and '^@' not in tr[i:j]))
    checks.append(('C.LIST: 687G and 60 k reach line 627 (the dd there)', j >= 0 and got == ec[:d] + ec[d + 1:]))
    checks.append(('C.LIST: 687G and 5 ^B reach about 5 screens up (the dd at line %d)' % (d + 1), 440 <= d <= 520))   # (Z80: 687G is not centred, the screen starts lower)
    checks.append(('no VI.SWAP left', img.find('VI.SWAP') is None))
    bad = 0
    for what, good in checks:
        print('%-4s  %s' % ('ok' if good else 'BAD', what))
        bad += not good
    if bad:
        print(tr[-3000:])
        print(info[-1000:])
    print('vi swap test (%s): %s   (work: %s)' % ('z80' if z else 'native', 'FAILED' if bad else 'PASSED', ps.dir))
    return 1 if bad else 0


if __name__ == '__main__':
    sys.exit(main())
