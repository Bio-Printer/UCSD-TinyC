#!/usr/bin/env python3
"""vipostest.py -- vi remembers where it left a text file: the place (line and column) is kept in 6 bytes of the
file's 1 KB header page (the part the Pascal Editor never uses: after its markers, environment and dates), and the next
time vi opens the file the cursor goes there.  On the P-System, from the shell:

  a file with no record opens at line 1
  150G, 6 columns right, ZZ (unchanged: the place is still written) -> the next open is at line 150
  that open: x ZZ deletes the character at column 6 of line 150 (the column came back too)
  a record beyond the end of the file (line 9999) -> the last line; a record with a wrong check -> line 1
  vi -c 5 FILE: the command wins over the record
  :w after a change keeps the header: the Editor's markers (DEFINED, COUNT, a marker name) are as they were,
  and every header byte except vi's 6 is unchanged
Uses the mode of PSYS_MODE (native or z80) like the other tools."""
import os, re, sys
HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
sys.path.insert(0, HERE)
from psys import PSystem
from tcrun import compile_c, compile_modules
from mkvolume import VI_MODULES
from difftest import make_text
import ucsdvol

N = 300
LINES = ['line %d of the file' % i for i in range(1, N + 1)]
HDR = 1000


def record(line, col, good=True):
    return bytes([86 if good else 87, 73, 1, col, line & 255, line >> 8])


def main():
    z = os.environ.get('PSYS_MODE') == 'z80'
    ps = PSystem()
    code = compile_modules([os.path.join(ROOT, m) for m in VI_MODULES], os.path.join(ps.dir, 'VI.CODE'), True)
    ps.put('VI.CODE', open(code, 'rb').read())
    base, sc = compile_c(os.path.join(ROOT, 'examples', 'shell.c'), ps.dir)
    ps.put('SHELL.CODE', open(sc, 'rb').read())
    v = ucsdvol.Volume(ps.src)

    def header(name, rec=None):
        raw = bytearray(make_text(LINES))
        raw[0:4] = bytes([1, 0, 2, 0])                  # the Editor's DEFINED and COUNT
        raw[4:12] = b'MARK1   '
        raw[132:140] = b'EDITOR!!'                      # (first bytes of what the Editor leaves alone)
        if rec:
            raw[HDR:HDR + 6] = rec
        v.write(name, bytes(raw), 3)
        return bytes(raw[:1024])
    h0 = header('A.C')
    header('B.C', record(9999, 5))
    header('C.C', record(40, 3, False))
    header('D.C', record(77, 2))
    header('E.C')
    v.save()
    # (command line, keys)
    runs = [('vi #5:A.C', '150G6lZZ'),               # no record -> line 1; leave at 150, column 6
            ('vi #5:A.C', 'xZZ'),                    # opens at 150: x deletes column 6 there
            ('vi #5:B.C', 'ZZ'),                     # beyond the end: the last line
            ('vi #5:C.C', 'ZZ'),                     # a bad check: line 1
            ('vi -c 5 #5:D.C', 'ZZ'),                # -c wins: line 5
            ('vi #5:D.C', 'ZZ'),                     # (ZZ left it at 5)
            ('vi #5:A.C', 'ddjjdd:w\rZZ'),           # a change and :w: the header is kept
            ('cd #5', None),                         # the prefix volume is the file's: VI.SWAP must leave room
            ('vi E.C', 'x:w\rZZ')]                   # ... for the new copy :w writes
    script = ['WAIT "Command:"', 'TYPE "X"', 'WAIT "Execute what file?"', 'TYPE "#5:SHELL\\r"', 'WAIT "shell> "']
    for cmd, keys in runs:
        if keys is None:
            script += ['TYPE "%s\\r"' % cmd, 'WAIT "shell> "']
            continue
        script += ['TYPE "%s\\r"' % cmd, 'WAIT "%s"' % cmd.split(':')[-1], 'TYPE "%s"' % keys.replace('\r', '\\r'), 'WAIT "shell> "']
    script += ['TYPE "bye\\r"', 'WAIT "Command:"']
    ok, tr, info = ps.run_script('\n'.join(script) + '\n', 900)
    tr = tr.replace('\r', '\n')
    start = []
    for (cmd, keys) in [r for r in runs if r[1] is not None]:
        i = tr.find('shell> ' + cmd + '\n', start[-1] + 1 if start else 0)
        start.append(i)
    where = []
    for i in start:
        m = re.search(r'COMMAND: [^ ]+ line (\d+)/(\d+)', tr[i:]) if i >= 0 else None
        where.append(int(m.group(1)) if m else None)
    img = ucsdvol.Volume(os.path.join(ps.out, 'VERIFY_SOURCE.BLK'))
    raw = lambda n: img.read(n)[0]
    textlines = lambda n: ucsdvol.ucsd_to_text(raw(n)).split('\n')[:-1]
    a = textlines('A.C')
    checks = [
        ('the script completed', ok),
        ("the prefix volume is the file's: :w works (VI.SWAP leaves room), the x was saved",
         'I/O error' not in tr and textlines('E.C')[0] == LINES[0][1:]),
        ('no record: opens at line 1', where[0] == 1),
        ('after 150G6l ZZ: the next open is at line 150', where[1] == 150),
        ('the column came back: x deleted the 7th character of line 150',
         a[148 + 0] == LINES[148] and (LINES[149][:6] + LINES[149][7:]) in a),
        ('a record beyond the end: the last line', where[2] == N),
        ('a record with a wrong magic: line 1', where[3] == 1),
        ('vi -c 5: the command wins', where[4] == 5),
        ('... and ZZ there leaves the new place: the next open is at line 5', where[5] == 5),
        ('no crash', 'System re-initialized' not in tr and 'STK OFLOW' not in tr),
    ]
    hb = raw('A.C')[:1024]
    same = all(hb[i] == h0[i] for i in list(range(0, HDR)) + list(range(HDR + 6, 1024)))
    checks.append(("a change and :w: the Editor's part of the header is as it was (DEFINED, COUNT, MARK1, the rest)", same))
    checks.append(("... and vi's record is there (line %s)" % where[1], hb[HDR:HDR + 3] == b'VI\x01'))
    bad = [n for n, g in checks if not g]
    for n, g in checks:
        print('%-5s %s' % ('ok' if g else 'FAIL', n))
    if bad:
        print(info[-600:])
        print('opened at', where, 'record', hb[HDR:HDR + 6].hex())
    print('vi position test (%s): %s   (work: %s)' % ('z80' if z else 'native', 'FAILED' if bad else 'PASSED', ps.dir))
    return 1 if bad else 0


if __name__ == '__main__':
    sys.exit(main())
