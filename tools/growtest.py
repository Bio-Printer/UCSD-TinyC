#!/usr/bin/env python3
"""growtest.py -- GROW and SHRINK (ports/grow, ports/shrink: the library's svresize, subvol.c),
KRUNCH of a sub-volume, and GREP, LC and WC wildcards in sub-volumes, on the P-System:

  MKDIR #5:A 40, #5:Z 20 (right after A); COPY #5:T*.TEXT #5:A
  GREP zebra #5:A/*.TEXT, LC #5:A/, LC #5:a, WC #5:A/*.TEXT; CD #5:/A: GREP and WC *.TEXT there
  GROW #5:A +30        no room after it (Z): it moves to a free area, whole
  GROW #5:A 100        room after it now: in place
  COPY BIG.DAT into A, DELETE A/T1.TEXT (a gap at its start)
  SHRINK #5:A -20      its files end before the new end: no krunch
  SHRINK #5:A 42       they do not: krunched first, then 42
  SHRINK #5:A 30       its files take 40 blocks: refused
  GROW +10, COPY T1, DELETE T2; KRUNCH -y #5:/A    the gap squeezed out of the sub-volume
  SHRINK #5:A          no size: no free blocks left in it (its directory and files only)
  MKDIR #5:P 60, P/Q 20, P/R 20, RMDIR P/Q: P's free blocks in two areas (20, 14)
  GROW #5:P/R +16      no area holds 36: P krunched first (R moves down), then in place
  GROW #5:P/R +100     no room: refused; GROW with a bad size, on a file: refused
then on the host: A, R and the files in A, byte for byte.
Uses the mode of PSYS_MODE (native or z80) like the other tools."""
import os, re, sys, tempfile
HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
sys.path.insert(0, HERE)
from psys import PSystem
from tcrun import compile_c
import ucsdvol

T1 = 'apple\nzebra one\n'
T2 = 'zebra two\nbanana\n'
BIG = bytes((k * 7 + 3) & 255 for k in range(30 * 512))


def inner(path, name):
    """the volume held by file name of the volume image at path: (its image file, Volume, entry)"""
    v = ucsdvol.Volume(path)
    e = [e for e in v.entries if e[3] == name][0]
    data = open(path, 'rb').read()[e[0] * 512:e[1] * 512]
    fd, p = tempfile.mkstemp(suffix='.BLK')
    os.write(fd, data)
    os.close(fd)
    return p, ucsdvol.Volume(p), e


def main():
    z = os.environ.get('PSYS_MODE') == 'z80'
    ps = PSystem()
    for src in ('examples/shell.c', 'ports/grow/grow.c', 'ports/shrink/shrink.c', 'ports/krunch/krunch.c',
                'ports/grep/grep.c', 'ports/lc/lc.c', 'ports/wc/wc.c'):
        b, code = compile_c(os.path.join(ROOT, src), ps.dir)
        ps.put(os.path.basename(code), open(code, 'rb').read())
    ps.put('T1.TEXT', T1)
    ps.put('T2.TEXT', T2)
    ps.put('BIG.DAT', BIG)
    steps = [('mkdir #5:A 40', 'a sub-volume'), ('mkdir #5:Z 20', 'a sub-volume'),
             ('copy #5:T*.TEXT #5:A', 'T2.TEXT'),
             ('#5:grep zebra #5:A/*.TEXT', 'exit status'), ('#5:lc #5:A/', 'exit status'),
             ('#5:lc -1 #5:a', 'exit status'), ('#5:wc #5:A/*.TEXT', 'exit status'),
             ('cd #5:/A', 'path is'), ('#5:grep -n zebra *.TEXT', 'exit status'), ('#5:wc -l *.TEXT', 'exit status'),
             ('cd #5:', 'prefix is'),
             ('#5:grow #5:A +30', 'exit status'), ('#5:grow #5:A 100', 'exit status'),
             ('copy #5:BIG.DAT #5:A', 'BIG.DAT'), ('delete #5:A/T1.TEXT', 'deleted'),
             ('#5:shrink #5:A -20', 'exit status'), ('#5:shrink #5:A 42', 'exit status'),
             ('#5:shrink #5:A 30', 'exit status'),
             ('#5:grow #5:A +10', 'exit status'), ('copy #5:T1.TEXT #5:A', 'T1.TEXT'), ('delete #5:A/T2.TEXT', 'deleted'),
             ('#5:krunch -y #5:/A', 'exit status'), ('#5:shrink #5:A', 'exit status'),
             ('mkdir #5:P 60', 'a sub-volume'), ('mkdir #5:P/Q 20', 'a sub-volume'), ('mkdir #5:P/R 20', 'a sub-volume'),
             ('rmdir #5:P/Q', 'removed'), ('#5:grow #5:P/R +16', 'exit status'), ('#5:grow #5:P/R +100', 'exit status'),
             ('#5:grow #5:A x10', 'exit status'), ('#5:shrink #5:T1.TEXT 10', 'exit status')]
    s = ['WAIT "Command:"', 'TYPE "X"', 'WAIT "Execute what file?"', 'TYPE "#5:SHELL\\r"', 'WAIT "shell> "']
    for cmd, wait in steps:
        s += ['TYPE "%s\\r"' % cmd, 'WAIT "%s"' % wait, 'WAIT "shell> "']
    s += ['TYPE "bye\\r"', 'WAIT "Command:"']
    ok, tr, info = ps.run_script('\n'.join(s) + '\n', 3600 if z else 400)
    tr = tr.replace('\r', '\n')

    def part(cmd):
        a = tr.find('shell> ' + cmd + '\n')
        return tr[a:tr.find('shell> ', a + 7)] if a >= 0 else ''

    img = os.path.join(ps.out, 'VERIFY_SOURCE.BLK')
    try:
        pa, va, ea = inner(img, 'A')
        pp, vp, ep = inner(img, 'P')
        pr, vr, er = inner(pp, 'R')
        a_names = sorted(e[3] for e in va.entries)
        a_files = va.read('BIG.DAT')[0][:len(BIG)] == BIG and ucsdvol.ucsd_to_text(va.read('T1.TEXT')[0]) == T1
        a_sizes = (ea[1] - ea[0], va.eov)
        a_tight = all(x[0] == y[1] for x, y in zip(sorted(va.entries)[1:], sorted(va.entries))) \
            and sorted(va.entries)[0][0] == 6
        a_used = va.used()
        r_sizes = (er[1] - er[0], vr.eov, er[0])
        p_names = sorted(e[3] for e in vp.entries)
    except (Exception, SystemExit) as x:
        a_names, a_files, a_sizes, a_tight, r_sizes, p_names, a_used = ['(%s)' % x], False, (), False, (), [], 0
    g = part('#5:grep zebra #5:A/*.TEXT')
    gp = part('#5:grep -n zebra *.TEXT')
    w = part('#5:wc #5:A/*.TEXT')
    checks = [('the script ran to its end', ok),
              ('GREP #5:A/*.TEXT: the lines, as WORK:/A/NAME:LINE:', 'WORK:/A/T1.TEXT:2: zebra one' in g
               and 'WORK:/A/T2.TEXT:1: zebra two' in g and '2 lines in 2 of 2 files' in g),
              ('LC #5:A/ and LC #5:a (a sub-volume\'s name): its files', 'T1.TEXT' in part('#5:lc #5:A/')
               and 'T2.TEXT' in part('#5:lc #5:A/') and re.search(r'T1\.TEXT\s+T2\.TEXT\s+\[exit status 0', part('#5:lc -1 #5:a')) is not None),
              ('WC #5:A/*.TEXT: each file and the total', '#5:A/T1.TEXT' in w and '#5:A/T2.TEXT' in w and 'total' in w),
              ('on the path (CD #5:/A): GREP -n and WC -l *.TEXT there', 'WORK:/A/T1.TEXT:1: apple' in gp
               and 'WORK:/A/T2.TEXT:2: banana' in gp and 'T1.TEXT' in part('#5:wc -l *.TEXT')),
              ('GROW +30 with Z after it: moved, 70 blocks', 'moved to block' in part('#5:grow #5:A +30')
               and '#5:A: 70 blocks (it was 40)' in tr),
              ('GROW 100: in place', 'moved' not in part('#5:grow #5:A 100') and '#5:A: 100 blocks (it was 70)' in tr),
              ('SHRINK -20: no krunch needed, none done', 'krunching' not in part('#5:shrink #5:A -20')
               and '#5:A: 80 blocks (it was 100)' in tr),
              ('SHRINK 42: krunched first (BIG.DAT moves down), then 42', 'krunching #5:A first' in part('#5:shrink #5:A 42')
               and 'BIG.DAT' in part('#5:shrink #5:A 42') and '#5:A: 42 blocks (it was 80)' in tr),
              ('SHRINK 30: its files take 40: refused', 'take 40 blocks: not 30' in part('#5:shrink #5:A 30')),
              ('KRUNCH -y #5:/A: the gap squeezed out', 'WORK:/A/ (#5) krunched' in part('#5:krunch -y #5:/A')),
              ('SHRINK #5:A (no size): from 52 blocks to its directory and files', '(it was 52)' in part('#5:shrink #5:A')
               and 'exit status 0' in part('#5:shrink #5:A')),
              ('GROW P/R +16: P krunched first, R 36 blocks', 'krunching the volume that holds #5:P/R first' in tr
               and '#5:P/R: 36 blocks (it was 20)' in tr),
              ('GROW P/R +100: no room', 'no room' in part('#5:grow #5:P/R +100')),
              ('a bad size, a file not a sub-volume: refused (status 1, 2)', 'the size is +N' in part('#5:grow #5:A x10')
               and 'exit status 1' in part('#5:grow #5:A x10') and 'is not a sub-volume' in part('#5:shrink #5:T1.TEXT 10')
               and 'exit status 2' in part('#5:shrink #5:T1.TEXT 10')),
              ('on the host: A no bigger than its directory and files (its file and its volume: %s), BIG.DAT and T1.TEXT '
               'the same bytes, one after another (%s)' % (a_sizes, ' '.join(a_names)),
               a_sizes == (a_used, a_used) and a_used < 52 and a_files and a_tight and a_names == ['BIG.DAT', 'T1.TEXT']),
              ('on the host: P holds R only, R 36 blocks from block 6 (%s)' % (r_sizes,), p_names == ['R'] and r_sizes == (36, 36, 6))]
    bad = 0
    for what, good in checks:
        print('%-4s  %s' % ('ok' if good else 'BAD', what))
        bad += not good
    if bad:
        print(tr[-3500:])
        print(info[-800:])
    print('grow test (%s): %s   (work: %s)' % ('z80' if z else 'native', 'FAILED' if bad else 'PASSED', ps.dir))
    return 1 if bad else 0


if __name__ == '__main__':
    sys.exit(main())
