#!/usr/bin/env python3
"""krunchtest.py -- KRUNCH (ports/krunch/krunch.c) and the shell's DIR -E, on the P-System.

  WORK: (#5) with files and gaps between them (files taken out of the directory):
    DIR -E #5:          the unused areas listed where they are, each file's first block
    KRUNCH #5 + N       asks, the answer is not Y: nothing moves (status 1)
    KRUNCH -y #5        the files moved down: every file's bytes the same, the files one after
                        another from the directory's end, the free blocks one area at the end
    DIR -E #5:          no unused area but the last one
    DIR -S #5:*.DAT     the largest first (equal sizes in the order of their blocks); DIR #5:*.DAT -N
                        by name (A.DAT, the last file, first); DIR -X: no such option
  the boot disk (#4), from the shell started with S (SYSTEM.SHELL, on #4 itself):
    KRUNCH -y #4        moves SYSTEM.PASCAL and SYSTEM.SHELL while both run; then the shell comes
                        back (pexec's return followed it), runs a program and its own DIR, BYE
                        goes back to the OS, the Filer lists #4 and S starts the shell again
Uses the mode of PSYS_MODE (native or z80) like the other tools."""
import os, re, sys
HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
sys.path.insert(0, HERE)
from psys import PSystem
from tcrun import compile_c
import ucsdvol


def main():
    z = os.environ.get('PSYS_MODE') == 'z80'
    ps = PSystem()
    for src in ('examples/shell.c', 'ports/krunch/krunch.c'):
        b, code = compile_c(os.path.join(ROOT, src), ps.dir)
        ps.put(os.path.basename(code), open(code, 'rb').read())
    files = {}
    v = ucsdvol.Volume(ps.src)
    for i, size in enumerate((3000, 700, 9000, 1500, 20000, 512, 4100, 2600)):
        name = 'F%d.DAT' % i
        data = bytes((i * 37 + k * 7) & 255 for k in range(size))
        v.write(name, data, 5)
        files[name] = data
    files['A.DAT'] = bytes(range(256)) * 4
    v.write('A.DAT', files['A.DAT'], 5)              # (the last file: DIR -N puts it first)
    for name in ('F1.DAT', 'F3.DAT', 'F5.DAT'):      # gaps: before F2, F4, F6
        v.remove(name)
        del files[name]
    v.save()
    v = ucsdvol.Volume(ps.src)
    before = {e[3]: e[0] for e in v.entries}
    s = ['WAIT "Command:"', 'TYPE "X"', 'WAIT "Execute what file?"', 'TYPE "#5:SHELL\\r"', 'WAIT "shell> "',
         'TYPE "dir -e #5:\\r"', 'WAIT "blocks used"', 'WAIT "shell> "',
         'TYPE "#5:krunch #5\\r"', 'WAIT "(Y/N)"', 'TYPE "N"', 'WAIT "exit status 1"', 'WAIT "shell> "',
         'TYPE "#5:krunch -y #5\\r"', 'WAIT "krunched"', 'WAIT "shell> "',
         'TYPE "dir #5: -E\\r"', 'WAIT "blocks used"', 'WAIT "shell> "',
         'TYPE "dir -s #5:*.dat\\r"', 'WAIT "blocks used"', 'WAIT "shell> "',
         'TYPE "dir #5:*.dat -n\\r"', 'WAIT "blocks used"', 'WAIT "shell> "',
         'TYPE "dir -x #5:\\r"', 'WAIT "no option"', 'WAIT "shell> "',
         'TYPE "bye\\r"', 'WAIT "Command:"',
         'TYPE "S"', 'WAIT "shell> "',
         'TYPE "#5:krunch -y #4\\r"', 'WAIT "krunched"', 'WAIT "shell> "',
         'TYPE "#5:krunch -y #4\\r"', 'WAIT "nothing to move"', 'WAIT "shell> "',
         'TYPE "dir #4:system.*\\r"', 'WAIT "blocks used"', 'WAIT "shell> "',
         'TYPE "bye\\r"', 'WAIT "Command:"',
         'TYPE "F"', 'WAIT "Filer:"', 'TYPE "L"', 'WAIT "?"', 'TYPE "#4\\r"', 'WAIT "Filer:"', 'TYPE "Q"',
         'WAIT "Command:"', 'TYPE "S"', 'WAIT "shell> "', 'TYPE "bye\\r"', 'WAIT "Command:"']
    ok, tr, info = ps.run_script('\n'.join(s) + '\n', 2400 if z else 400)
    tr = tr.replace('\r', '\n')
    img = ucsdvol.Volume(os.path.join(ps.out, 'VERIFY_SOURCE.BLK'))
    ents = sorted(img.entries)
    nxt = img.dirlast
    contiguous = True
    for e in ents:
        contiguous = contiguous and e[0] == nxt
        nxt = e[1]
    moved = [e[3] for e in ents if e[3] in before and before[e[3]] != e[0]]
    same = all(img.read(n)[0][:len(d)] == d for n, d in files.items())
    first = tr[tr.find('dir -e #5:'):tr.find('krunch #5')]
    second = tr[tr.find('dir #5: -E'):tr.find('dir -s #5:')]
    order = lambda part: re.findall(r'^([A-Z0-9]+\.DAT)', part, re.M)
    bysize = order(tr[tr.find('dir -s #5:'):tr.find('dir #5:*.dat -n')])
    byname = order(tr[tr.find('dir #5:*.dat -n'):tr.find('dir -x')])
    boot = tr[tr.find('krunch -y #4'):]
    checks = [('the script ran to its end (the shell came back after #4 was krunched, the Filer, S again)', ok),
              ('DIR -E: the unused areas listed (3 between files, and the end), the first blocks',
               first.count('< unused >') == 4 and 'Start' in first),
              ('KRUNCH + N: status 1', 'exit status 1' in tr),
              ('KRUNCH -y #5: the files after a gap moved (%s)' % ' '.join(moved), set(moved) == {'F2.DAT', 'F4.DAT', 'F6.DAT', 'F7.DAT', 'A.DAT'}),
              ('every file the same bytes', same),
              ('the files one after another from the directory, the rest one free area', contiguous),
              ('DIR -E after: only the area at the end', second.count('< unused >') == 1),
              ('DIR -S: the largest first (%s)' % ' '.join(bysize),
               bysize == ['F4.DAT', 'F2.DAT', 'F6.DAT', 'F0.DAT', 'F7.DAT', 'A.DAT']),
              ('DIR -N: by name (%s)' % ' '.join(byname), byname == ['A.DAT', 'F0.DAT', 'F2.DAT', 'F4.DAT', 'F6.DAT', 'F7.DAT']),
              ('DIR -X: no such option', 'no option -x' in tr or 'no option -X' in tr),
              ('KRUNCH -y #4: SYSTEM.PASCAL and SYSTEM.SHELL moved', 'SYSTEM.PASCAL' in boot and 'SYSTEM.SHELL' in boot),
              ('a second KRUNCH of #4: nothing to move', 'nothing to move' in boot)]
    bad = 0
    for what, good in checks:
        print('%-4s  %s' % ('ok' if good else 'BAD', what))
        bad += not good
    if bad:
        print(tr[-2500:])
        print(info[-800:])
    print('krunch test (%s): %s   (work: %s)' % ('z80' if z else 'native', 'FAILED' if bad else 'PASSED', ps.dir))
    return 1 if bad else 0


if __name__ == '__main__':
    sys.exit(main())
