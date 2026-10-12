#!/usr/bin/env python3
"""hexdumptest.py -- HEXDUMP (ports/hexdump/hexdump.c, TOOLS:HEXDUMP.CODE) from the shell on the
P-System, its output redirected into text files (>FILE) that are read back from the disk image:

  hexdump FILE          offset, 16 bytes in hex (two more blanks after the 8th), the characters
  hexdump -i FILE       Intel Hex: data records of 16 bytes, an extended linear address record
                        at every 64 KB, the end record; checked by reading it back with the
                        host's objcopy (-I ihex) when there is one
  a binary file (whole blocks), a text file, one over 64 KB, several files (the offsets go on),
  the standard input (<FILE, a pipe), a missing file, a bad option.

Uses the mode of PSYS_MODE (native or z80) like the other tools."""
import os, sys, re, random, shutil, subprocess, tempfile
HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
sys.path.insert(0, HERE)
from psys import PSystem
import ucsdvol
from tcrun import compile_c

rng = random.Random(7)
SMALL = bytes(rng.randrange(256) for _ in range(1000))
SMALLP = SMALL + bytes(1024 - len(SMALL))               # a data file is whole 512-byte blocks
BIG = bytes(rng.randrange(256) for _ in range(66000))
BIGP = BIG + bytes(512 * ((len(BIG) + 511) // 512) - len(BIG))
TEXT = 'Hello, world!\nsecond line\n\n  indented, ~ and DEL-free\n'
TEXTB = TEXT.encode('latin1')


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


def dump(data, start=0):
    out = []
    for i in range(0, len(data), 16):
        r = data[i:i + 16]
        h = ''
        for k in range(16):
            h += ('%02X' % r[k] if k < len(r) else '  ') + ' ' + ('  ' if k == 7 else '')
        out.append('%08X  %s %s' % (start + i, h, ''.join(chr(c) if 32 <= c < 127 else '.' for c in r)))
    return out


def ihex(data):
    out = []

    def rec(t, a, d):
        s = len(d) + (a >> 8) + (a & 255) + t + sum(d)
        out.append(':%02X%04X%02X%s%02X' % (len(d), a, t, ''.join('%02X' % b for b in d), (256 - s) & 255))
    for i in range(0, len(data), 16):
        if i and i % 65536 == 0:
            rec(4, 0, bytes([i >> 24 & 255, i >> 16 & 255]))
        rec(0, i & 65535, data[i:i + 16])
    out.append(':00000001FF')
    return out


# (command, output file or None, the lines wanted or None)
CASES = [
    ('hexdump #5:B.DAT >#5:O1.TEXT', 'O1.TEXT', dump(SMALLP)),
    ('hexdump -i #5:B.DAT >#5:O2.TEXT', 'O2.TEXT', ihex(SMALLP)),
    ('hexdump #5:P.TEXT >#5:O3.TEXT', 'O3.TEXT', dump(TEXTB)),
    ('hexdump -i #5:P.TEXT >#5:O4.TEXT', 'O4.TEXT', ihex(TEXTB)),
    ('hexdump -i #5:BIG.DAT >#5:O5.TEXT', 'O5.TEXT', ihex(BIGP)),
    ('hexdump <#5:P.TEXT >#5:O6.TEXT', 'O6.TEXT', dump(TEXTB)),
    ('cat #5:P.TEXT | hexdump -i >#5:O7.TEXT', 'O7.TEXT', ihex(TEXTB)),
    ('hexdump #5:P.TEXT #5:P.TEXT >#5:O8.TEXT', 'O8.TEXT', dump(TEXTB + TEXTB)),
    ('hexdump #5:NOSUCH.DAT', None, None),
    ('hexdump -x #5:B.DAT', None, None),
]


def main():
    ps = PSystem()
    z = os.environ.get('PSYS_MODE') == 'z80'
    srcs = ['examples/shell.c', 'ports/hexdump/hexdump.c', 'ports/cat/cat.c']
    for src in srcs:
        base, code = compile_c(os.path.join(ROOT, src), ps.dir, z)
        ps.put(base + '.CODE', open(code, 'rb').read())
    v = ucsdvol.Volume(ps.src)
    v.write('B.DAT', SMALLP, 5)
    v.write('BIG.DAT', BIGP, 5)
    v.write('P.TEXT', make_text(TEXT), 3)
    v.save()
    script = ['WAIT "Command:"', 'TYPE "X"', 'WAIT "Execute what file?"', 'TYPE "#5:SHELL\\r"', 'WAIT "shell> "',
              'TYPE "cd #5\\r"', 'WAIT "shell> "']
    for c, _, _ in CASES:
        script += ['TYPE "%s\\r"' % c, 'WAIT "shell> "']
    script += ['TYPE "bye\\r"', 'WAIT "Command:"']
    ok, tr, info = ps.run_script('\n'.join(script) + '\n', 3000)
    tr = tr.replace('\r', '\n')
    if not ok:
        print(info[-600:]); print(tr[-800:])
    img = ucsdvol.Volume(os.path.join(ps.out, 'VERIFY_SOURCE.BLK'))     # WORK: after the run

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
            got = None
            if e:
                got = ucsdvol.ucsd_to_text(img.read(name)[0]).split('\n') if hasattr(ucsdvol, 'ucsd_to_text') else None
            good = got is not None and [l.rstrip() for l in got if l != ''] == [l.rstrip() for l in want]
            if not good and got is not None:
                for k, (g, w) in enumerate(zip(got, want)):
                    if g.rstrip() != w.rstrip():
                        print('--- %s line %d\n got:  %r\n want: %r' % (c, k + 1, g, w))
                        break
                else:
                    print('--- %s: %d lines, wanted %d' % (c, len(got), len(want)))
        elif 'NOSUCH' in c:
            s = console(c)
            good = 'hexdump: #5:NOSUCH.DAT: cannot open it' in s and '[exit status 1]' in s
        else:
            s = console(c)
            good = "invalid option -- 'x'" in s and 'use: HEXDUMP' in s and '[exit status 1]' in s
        if not good:
            print('console of %s:\n%s' % (c, console(c)[-400:]))
        checks.append((c, good))
    if shutil.which('objcopy') and img.find('O5.TEXT'):         # the Intel Hex of the big file, read back by the host
        t = ucsdvol.ucsd_to_text(img.read('O5.TEXT')[0])
        d = tempfile.mkdtemp()
        open(os.path.join(d, 'x.hex'), 'w').write(t)
        r = subprocess.run(['objcopy', '-I', 'ihex', '-O', 'binary', os.path.join(d, 'x.hex'), os.path.join(d, 'x.bin')],
                           capture_output=True)
        good = r.returncode == 0 and open(os.path.join(d, 'x.bin'), 'rb').read() == BIGP
        checks.append(('objcopy -I ihex reads the big file back, byte for byte', good))
    bad = [n for n, good in checks if not good]
    for n, good in checks:
        print('%-5s %s' % ('ok' if good else 'FAIL', n))
    print('hexdump test: %s   (work: %s)' % ('FAILED' if bad else 'PASSED', ps.dir))
    return 1 if bad else 0


if __name__ == '__main__':
    sys.exit(main())
