#!/usr/bin/env python3
"""subvoltest.py -- sub-volumes (tinyc/lib/dirs.c pathdir, __svopen, __svclose, __svremove; the
shell's MKDIR, CD PATH, DIR, TYPE, DELETE on paths), on the P-System.

  MKDIR #5:A 200, MKDIR #5:/A/B 60     a sub-volume, and one inside it
  DIR #5:A/                            its directory: no files yet
  CD #5:/A                             the path; CD alone tells it again after a program ran
  DIR >LIST.TEXT                       a shell command's output into the sub-volume
  WC LIST.TEXT, CAT LIST.TEXT >B/COPY.TEXT, WC #5:A/B/COPY.TEXT
                                       programs read and write paths (the path goes with pexec)
  CD B, DIR, CD .., CD ..              down and up again: the prefix volume
  DELETE #5:/A/LIST.TEXT               a file in a sub-volume goes
  CD #5:/NONE, COPY on a path          refused
then on the host: A and B are data files whose blocks 2..5 hold their directories, and B's COPY.TEXT
is the text of LIST.TEXT.
Uses the mode of PSYS_MODE (native or z80) like the other tools."""
import os, re, sys, tempfile
HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
sys.path.insert(0, HERE)
from psys import PSystem
from tcrun import compile_c
import ucsdvol


def inner(path, name):
    """the volume held by file name of the volume image at path: (its image file, Volume)"""
    v = ucsdvol.Volume(path)
    e = [e for e in v.entries if e[3] == name][0]
    data = open(path, 'rb').read()[e[0] * 512:e[1] * 512]
    fd, p = tempfile.mkstemp(suffix='.BLK')
    os.write(fd, data)
    os.close(fd)
    return p, ucsdvol.Volume(p), e


def wcs(text):
    """the three counts of a WC line"""
    m = re.search(r'^\s*(\d+)\s+(\d+)\s+(\d+)\s', text, re.M)
    return m and m.groups()


def main():
    z = os.environ.get('PSYS_MODE') == 'z80'
    ps = PSystem()
    for src in ('examples/shell.c', 'ports/wc/wc.c', 'ports/cat/cat.c', 'ports/grep/grep.c'):
        b, code = compile_c(os.path.join(ROOT, src), ps.dir)
        ps.put(os.path.basename(code), open(code, 'rb').read())
    s = ['WAIT "Command:"', 'TYPE "X"', 'WAIT "Execute what file?"', 'TYPE "#5:SHELL\\r"', 'WAIT "shell> "']
    for cmd, wait in (('mkdir #5:A 200', 'a sub-volume'), ('mkdir #5:/A/B 60', 'a sub-volume'),
                      ('mkdir #5:A 10', 'there already'),
                      ('dir #5:A/', 'blocks used'), ('cd #5:/A', 'path is'),
                      ('dir >LIST.TEXT', None), ('#5:wc LIST.TEXT', 'LIST.TEXT'), ('cd', 'path is'),
                      ('#5:cat LIST.TEXT >B/COPY.TEXT', None), ('#5:wc #5:A/B/COPY.TEXT', 'COPY.TEXT'),
                      ('type b/copy.text', 'blocks used'), ('dir', 'blocks used'),
                      ('cd B', 'path is'), ('dir', 'blocks used'), ('cd ..', 'path is'), ('cd ..', 'prefix is'),
                      ('delete #5:/A/LIST.TEXT', 'deleted'), ('dir #5:A/', 'blocks used'),
                      ('cd #5:/NONE', 'no sub-volume'), ('copy #5:A/B/COPY.TEXT #9:', 'not yet')):
        s.append('TYPE "%s\\r"' % cmd)
        if wait:
            s.append('WAIT "%s"' % wait)
        s.append('WAIT "shell> "')
    s += ['TYPE "bye\\r"', 'WAIT "Command:"']
    ok, tr, info = ps.run_script('\n'.join(s) + '\n', 2400 if z else 400)
    tr = tr.replace('\r', '\n')
    img = os.path.join(ps.out, 'VERIFY_SOURCE.BLK')
    checks = [('the script ran to its end', ok)]
    try:
        pa, va, ea = inner(img, 'A')
        pb, vb, eb = inner(pa, 'B')
        names_a = sorted(e[3] for e in va.entries)
        copy = ucsdvol.ucsd_to_text(vb.read('COPY.TEXT')[0])
        kinds = (ea[2] & 15, eb[2] & 15)
        lens = (ea[1] - ea[0], eb[1] - eb[0], va.eov, vb.eov)
    except (Exception, SystemExit) as x:
        names_a, copy, kinds, lens = ['(%s)' % x], '', (), ()
    seg = lambda a, b: tr[tr.find(a):tr.find(b, tr.find(a) + len(a))]
    listing = seg('shell> dir >LIST.TEXT', 'shell> ')
    checks += [('MKDIR: A (200 blocks) and B in it (60), data files, the inner volumes their size (%s %s)' % (kinds, lens),
                kinds == (5, 5) and lens == (200, 60, 200, 60)),
               ('MKDIR on a name that is there: refused', 'there already' in tr),
               ('DIR #5:A/ after MKDIRs: WORK:/A/ with B', 'WORK:/A/ (#5)' in tr and '1 of 1 files' in tr),
               ('CD #5:/A: the path', 'path is WORK:/A (#5)' in tr),
               ('WC of LIST.TEXT on the path (a program after pexec)', 'LIST.TEXT' in seg('shell> #5:wc', 'shell> ')
                and 'cannot' not in seg('shell> #5:wc', 'shell> ')),
               ('CD alone after programs ran: the path still', seg('shell> cd\n', 'shell> ').count('path is WORK:/A') == 1),
               ('CAT >B/COPY.TEXT: the text of LIST.TEXT', copy.strip() != '' and 'blocks used' in copy),
               ('WC #5:A/B/COPY.TEXT: the same counts as LIST.TEXT', wcs(seg('shell> #5:wc #5', 'shell> ')) == wcs(seg('shell> #5:wc LIST', 'shell> ')) != None),
               ('TYPE b/copy.text', 'blocks used' in seg('shell> type b/copy.text', 'shell> ')),
               ('CD B, CD .., CD ..: down, up, the volume again', 'path is WORK:/A/B (#5)' in tr and 'prefix is WORK: (#5)' in tr),
               ('DELETE #5:/A/LIST.TEXT: gone (A holds %s)' % ' '.join(names_a), 'WORK:/A/LIST.TEXT deleted' in tr and names_a == ['B']),
               ('CD to no sub-volume, COPY on a path: refused', 'cd: no sub-volume #5:/NONE' in tr and 'copy: not yet' in tr)]
    bad = 0
    for what, good in checks:
        print('%-4s  %s' % ('ok' if good else 'BAD', what))
        bad += not good
    if bad:
        print(tr[-3000:])
        print(info[-800:])
    print('sub-volume test (%s): %s   (work: %s)' % ('z80' if z else 'native', 'FAILED' if bad else 'PASSED', ps.dir))
    return 1 if bad else 0


if __name__ == '__main__':
    sys.exit(main())
