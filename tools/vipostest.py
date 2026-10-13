#!/usr/bin/env python3
"""vipostest.py -- vi remembers where it left a text file: the line and column, in binary, in 6 bytes of the
file's 1 KB header page ('V' 'I' 1 column line-low line-high at byte 1000, in the part the Pascal Editor never
uses), and the next time vi opens the file that line is in the middle of the screen with the cursor on the
column.  On the P-System, from the shell:

  a file with no record opens at line 1
  150G, 6 columns right, ZZ (unchanged: the place is still written) -> the next open is at line 150, on the
  middle row of the screen, and x there deletes the character at column 6 (the column came back too)
  a record beyond the end of the file (line 9999) -> the last line; a record with a wrong magic -> line 1
  vi -c 5 FILE: the command wins over the record
  a change and ZZ (vi writes the file): the place is kept all the same (line 100)
  a bigger file and no key: the rest loads while vi waits, the status line then shows the total and no L
  a file left unchanged keeps every other byte of its header page (the Editor's)
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
    header0 = header('B.C', record(9999, 5))
    header('C.C', record(40, 3, False))
    header('D.C', record(77, 2))
    header('F.C')
    v.write('G.C', make_text(['line %d of a bigger file, to load while vi waits' % i for i in range(1, 1501)]), 3)
    v.save()
    # (command line, keys)
    runs = [('vi #5:A.C', '150G6lZZ'),               # no record -> line 1; leave at 150, column 6
            ('vi #5:A.C', 'xZZ'),                    # opens at 150: x deletes column 6 there
            ('vi #5:B.C', 'ZZ'),                     # beyond the end: the last line
            ('vi #5:C.C', 'ZZ'),                     # a bad check: line 1
            ('vi -c 5 #5:D.C', 'ZZ'),                # -c wins: line 5
            ('vi #5:D.C', 'ZZ'),                     # (ZZ left it at 5)
            ('vi #5:F.C', '100GxZZ'),                # a change, then ZZ (it writes the file)
            ('vi #5:F.C', 'ZZ'),                     # ... the place is kept all the same: line 100
            ('vi #5:G.C', 'WAIT')]                   # no key: the rest loads, the status line says so
    script = ['WAIT "Command:"', 'TYPE "X"', 'WAIT "Execute what file?"', 'TYPE "#5:SHELL\\r"', 'WAIT "shell> "']
    for cmd, keys in runs:
        if keys == 'WAIT':
            script += ['TYPE "%s\\r"' % cmd, 'WAIT "G.C"', 'WAIT "1500 0%"', 'TYPE "ZZ"', 'WAIT "shell> "']
            continue
        if keys is None:
            script += ['TYPE "%s\\r"' % cmd, 'WAIT "shell> "']
            continue
        script += ['TYPE "%s\\r"' % cmd, 'WAIT "%s"' % cmd.split(':')[-1], 'TYPE "%s"' % keys.replace('\r', '\\r'), 'WAIT "shell> "']
    script += ['TYPE "bye\\r"', 'WAIT "Command:"']
    scr = os.path.join(ps.dir, 'screens.txt')
    os.environ['VERIFY_SCREEN'] = scr
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
    centre = None            # the row of line 150 on the first screen that shows it as the cursor's line
    for blk in re.split(r'^== key ', open(scr, encoding='latin1').read(), flags=re.M)[1:]:
        rows = blk.split('\n')[1:]
        if any(r.startswith('COMMAND') and ' line 150/300' in r for r in rows):
            centre = [i for i, r in enumerate(rows) if 'line 150 of the file' in r]
            break
    checks = [
        ('the script completed', ok),
        ('the line it opens at is in the middle of the screen (row %s of 45)' % centre,
         bool(centre) and 18 <= centre[0] <= 26),
        ('no record: opens at line 1', where[0] == 1),
        ('after 150G6l ZZ: the next open is at line 150', where[1] == 150),
        ('the column came back: x deleted the 7th character of line 150',
         a[148 + 0] == LINES[148] and (LINES[149][:6] + LINES[149][7:]) in a),
        ('a record beyond the end: the last line', where[2] == N),
        ('a record with a wrong magic: line 1', where[3] == 1),
        ('vi -c 5: the command wins', where[4] == 5),
        ('... and ZZ there leaves the new place: the next open is at line 5', where[5] == 5),
        ('no key: the whole file loads, the status line then shows its 1500 lines and no L',
         ok and re.search(r'1500 0%[^\n]*?a\d+/\d+ *(?!L)', tr[tr.find('shell> vi #5:G.C'):]) is not None),
        ('a change, ZZ: the next open is at line 100 (%s)' % where[7], where[7] == 100),
        ('no crash', 'System re-initialized' not in tr and 'STK OFLOW' not in tr),
    ]
    hb = raw('B.C')[:1024]                  # (left unchanged: only vi's 6 bytes differ)
    hb0 = bytearray(header0)
    same = all(hb[i] == hb0[i] for i in list(range(0, HDR)) + list(range(HDR + 6, 1024)))
    checks.append(("a file left unchanged keeps the rest of its header page (DEFINED, COUNT, MARK1, the rest)", same))
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
