#!/usr/bin/env python3
"""objcopytest.py -- OBJCOPY (ports/objcopy/objcopy.c, TOOLS:OBJCOPY.CODE) from the shell on the P-System,
turning the output of HEXDUMP (both formats) back into files, which are read back from the disk image:

  hexdump F | objcopy - R   and   hexdump -i F >H, objcopy H R   round trips of a binary file, one over
                            64 KB (extended linear address records) and a text file whose last line is
                            short and ends in something that looks like hex bytes
  a hand-written dump with a gap in its offsets (zero filled), Intel Hex with a bad checksum, a bad -I

Uses the mode of PSYS_MODE (native or z80) like the other tools."""
import os, sys, random
HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
sys.path.insert(0, HERE)
from psys import PSystem
import ucsdvol
from tcrun import compile_c

rng = random.Random(11)
SMALL = bytes(rng.randrange(256) for _ in range(1000))
SMALLP = SMALL + bytes(1024 - len(SMALL))
BIG = bytes(rng.randrange(256) for _ in range(66000))
BIGP = BIG + bytes(512 * ((len(BIG) + 511) // 512) - len(BIG))
TEXT = 'Hello, world!\nsecond line\n\n  indented\nab cd ef 01 23\n'
TEXTB = TEXT.encode('latin1')
HAND = ('00000010  01 02 03 04 05 06 07 08   09 0A 0B 0C 0D 0E 0F 10  ................\n'
        '00000030  AA BB CC                                          ...\n')
HANDB = bytes(range(1, 17)) + bytes(16) + bytes([0xAA, 0xBB, 0xCC])
HANDP = HANDB + bytes(512 - len(HANDB))
BADHEX = ':0300000001020300\n:00000001FF\n'               # the checksum is 0xFA


def make_text(text):
    pages, page = [], b''
    for line in text.split('\n')[:-1]:
        b = line.encode('latin1') + b'\r'
        if len(page) + len(b) > 1023:
            pages.append(page)
            page = b''
        page += b
    pages.append(page)
    return bytes(1024) + b''.join(p + bytes(1024 - len(p)) for p in pages)


# (command, the file it writes and its bytes) -- or the console text that must show
CASES = [
    ('hexdump #5:B.DAT | objcopy - #5:R1.DAT', 'R1.DAT', SMALLP),
    ('hexdump -i #5:B.DAT >#5:H2.TEXT', None, None),
    ('objcopy #5:H2.TEXT #5:R2.DAT', 'R2.DAT', SMALLP),
    ('hexdump -i #5:BIG.DAT >#5:H3.TEXT', None, None),
    ('objcopy -I ihex -O binary #5:H3.TEXT #5:R3.DAT', 'R3.DAT', BIGP),
    ('hexdump #5:BIG.DAT >#5:H4.TEXT', None, None),
    ('objcopy -I hexdump #5:H4.TEXT #5:R4.DAT', 'R4.DAT', BIGP),
    ('hexdump #5:P.TEXT >#5:H5.TEXT', None, None),
    ('objcopy #5:H5.TEXT #5:R5.TEXT', 'R5.TEXT', TEXTB),
    ('hexdump -i #5:P.TEXT | objcopy -I ihex - #5:R6.TEXT', 'R6.TEXT', TEXTB),
    ('objcopy #5:HAND.TEXT #5:R7.DAT', 'R7.DAT', HANDP),
    ('objcopy #5:BAD.TEXT #5:R8.DAT', None, 'bad checksum'),
    ('objcopy -I foo #5:H5.TEXT', None, 'unknown input format foo'),
    ('objcopy #5:NOSUCH.TEXT', None, 'NOSUCH.TEXT: cannot open it'),
]


def main():
    ps = PSystem()
    z = os.environ.get('PSYS_MODE') == 'z80'
    for src in ('examples/shell.c', 'ports/hexdump/hexdump.c', 'ports/objcopy/objcopy.c'):
        base, code = compile_c(os.path.join(ROOT, src), ps.dir, z)
        ps.put(base + '.CODE', open(code, 'rb').read())
    v = ucsdvol.Volume(ps.src)
    v.write('B.DAT', SMALLP, 5)
    v.write('BIG.DAT', BIGP, 5)
    v.write('P.TEXT', make_text(TEXT), 3)
    v.write('HAND.TEXT', make_text(HAND), 3)
    v.write('BAD.TEXT', make_text(BADHEX), 3)
    v.save()
    script = ['WAIT "Command:"', 'TYPE "X"', 'WAIT "Execute what file?"', 'TYPE "#5:SHELL\\r"', 'WAIT "shell> "',
              'TYPE "cd #5\\r"', 'WAIT "shell> "']
    for c, _, _ in CASES:
        script += ['TYPE "%s\\r"' % c, 'WAIT "shell> "']
    script += ['TYPE "bye\\r"', 'WAIT "Command:"']
    ok, tr, info = ps.run_script('\n'.join(script) + '\n', 3000)
    tr = tr.replace('\r', '\n')
    img = ucsdvol.Volume(os.path.join(ps.out, 'VERIFY_SOURCE.BLK'))

    def console(cmd):
        i = tr.find('shell> ' + cmd + '\n')
        if i < 0:
            return ''
        i += 7 + len(cmd) + 1
        return tr[i:tr.find('shell> ', i)]
    checks = [('the script completed', ok)]
    for c, name, want in CASES:
        if name:
            e = img.find(name)
            if not e:
                good = False
            else:
                got = img.read(name)[0]
                if name.endswith('.TEXT'):
                    got = ucsdvol.ucsd_to_text(got).encode('latin1')
                good = got == want or (name.endswith('.TEXT') and got.rstrip(b'\0') == want)
                if not good:
                    n = next((k for k in range(min(len(got), len(want))) if got[k] != want[k]), min(len(got), len(want)))
                    print('--- %s: %d bytes, wanted %d, first difference at %d' % (c, len(got), len(want), n))
        elif want:
            s = console(c)
            good = want in s and '[exit status 1]' in s
            if not good:
                print('console of %s:\n%s' % (c, s[-300:]))
        else:
            good = '[exit status 0]' in console(c)
        checks.append((c, good))
    bad = [n for n, good in checks if not good]
    for n, good in checks:
        print('%-5s %s' % ('ok' if good else 'FAIL', n))
    print('objcopy test: %s   (work: %s)' % ('FAILED' if bad else 'PASSED', ps.dir))
    return 1 if bad else 0


if __name__ == '__main__':
    sys.exit(main())
