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
  MKDIR #5:C, #5:C/D; DIR c, CD c, CD d (names on the prefix, on the path); RMDIR: refused for the
  path the shell is in, the one that holds it, and a file; RMDIR d, RMDIR c
  COPY, MOVE, RENAME in and out of sub-volumes; refused: into itself, RENAME elsewhere, DELETE of a
  sub-volume, MOVE of the path; a sub-volume moved whole to #9, RMDIR of it with its files (asks: N, then Y)
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
    v = ucsdvol.Volume(ps.src)
    v.write('\\TMP', b'x' * 512, 5)         # a file named \TMP (an older shell's COPY made such)
    v.save()
    s = ['WAIT "Command:"', 'TYPE "X"', 'WAIT "Execute what file?"', 'TYPE "#5:SHELL\\r"', 'WAIT "shell> "']
    for step in (('mkdir #5:A 200', 'a sub-volume'), ('mkdir #5:/A/B 60', 'a sub-volume'),
                      ('mkdir #5:A 10', 'there already'),
                      ('dir #5:A/', 'blocks used'), ('cd #5:/A', 'path is'),
                      ('dir >LIST.TEXT', None), ('#5:wc LIST.TEXT', 'LIST.TEXT'), ('cd', 'path is'),
                      ('#5:cat LIST.TEXT >B/COPY.TEXT', None), ('#5:wc #5:A/B/COPY.TEXT', 'COPY.TEXT'),
                      ('type b/copy.text', 'blocks used'), ('dir', 'blocks used'),
                      ('cd B', 'path is'), ('dir', 'blocks used'), ('cd ..', 'path is'), ('cd ..', 'prefix is'),
                      ('delete #5:/A/LIST.TEXT', 'deleted'), ('dir #5:A/', 'blocks used'),
                      ('cd #5:/NONE', 'no sub-volume'),
                      ('copy #5:A/B/COPY.TEXT #9:', 'SPARE:COPY.TEXT'), ('copy #9:COPY.TEXT #5:A/C2.TEXT', 'C2.TEXT'),
                      ('copy #5:WC.CODE #5:A', 'WC.CODE'), ('move #5:A/C2.TEXT #5:A/B/', 'C2.TEXT'),
                      ('rename #5:A/B/C2.TEXT C3.TEXT', 'C3.TEXT'), ('type #5:A/B/C3.TEXT', 'blocks used'),
                      ('copy #5:A #5:A/B/', 'not into itself'), ('rename #5:A/B/C3.TEXT #9:X.TEXT', 'MOVE'),
                      ('move #5:A/WC.CODE #5:', 'WC.CODE'), ('delete #5:A', 'RMDIR removes it'),
                      ('cd #5:/A/B', 'path is'), ('move #5:A #9:', 'CD .. first'), ('cd #5:', 'prefix is'),
                      ('mkdir #5:C 30', 'a sub-volume'), ('mkdir #5:C/D 10', 'a sub-volume'),
                      ('dir c', 'blocks used'), ('cd c', 'path is'), ('cd d', 'path is'),
                      ('rmdir #5:/C/D', 'CD .. first'), ('cd ..', 'path is'), ('rmdir #5:C', 'CD .. first'),
                      ('rmdir d', 'removed'), ('cd ..', 'prefix is'), ('rmdir #5:SHELL.CODE', 'not a sub-volume'),
                      ('rmdir c', 'removed'), ('dir #5:C', 'blocks used'),
                      ('mkdir #5:E 60', 'a sub-volume'), ('copy #5:WC.CODE #5:E/', 'WC.CODE'),
                      ('move #5:E #9:', 'SPARE:E'), ('dir #9:E', 'blocks used'), ('rmdir #9:E', '(Y/N)', 'N'),
                      ('dir #9:E', 'blocks used'), ('rmdir #9:E', '(Y/N)', 'Y'),
                      ('mkdir #5:T 200', 'a sub-volume'), ('copy #5:\\\\*.code #5:\\\\t', 'WC.CODE'),
                      ('dir #5:\\\\t\\\\', 'blocks used'), ('cd #5:\\\\t', 'path is'), ('cd #5:', 'prefix is'),
                      ('#5:wc #5:\\\\a\\\\b\\\\*.TEXT', 'exit status'), ('#5:grep -n blocks #5:\\\\a\\\\b\\\\*.text', 'exit status'),
                      ('copy #5:WC.CODE #5:X\\\\Y', 'no volume'), ('rmdir #5:\\\\t', '(Y/N)', 'Y'),
                      ('delete #5:?TMP', '(Y/N)', 'Y')):
        cmd, wait = step[:2]
        s.append('TYPE "%s\\r"' % cmd)
        if wait:
            s.append('WAIT "%s"' % wait)
        if len(step) > 2:
            s.append('TYPE "%s"' % step[2])     # a key: the answer to a question
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
        same = vb.read('C3.TEXT') == vb.read('COPY.TEXT')      # (#9: and back, moved, renamed)
        kinds = (ea[2] & 15, eb[2] & 15)
        lens = (ea[1] - ea[0], eb[1] - eb[0], va.eov, vb.eov)
    except (Exception, SystemExit) as x:
        names_a, copy, kinds, lens, same = ['(%s)' % x], '', (), (), False
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
               ('CD to no sub-volume: refused', 'cd: no sub-volume #5:/NONE' in tr),
               ('COPY out of a sub-volume, into one (by a path, by its name)', 'WORK:/A/B/COPY.TEXT -> SPARE:COPY.TEXT' in tr
                and 'SPARE:COPY.TEXT -> WORK:/A/C2.TEXT' in tr and 'WORK:WC.CODE -> WORK:/A/WC.CODE' in tr),
               ('MOVE into a nested one, RENAME there, TYPE it', 'WORK:/A/C2.TEXT -> WORK:/A/B/C2.TEXT' in tr
                and 'WORK:/A/B/C2.TEXT -> WORK:/A/B/C3.TEXT' in tr and 'blocks used' in seg('shell> type #5:A/B/C3', 'shell> ')),
               ('C3.TEXT (COPY.TEXT to #9: and back, MOVEd, RENAMEd): the same bytes and kind', same),
               ('MOVE out, onto an old file: replaced', 'WORK:/A/WC.CODE -> WORK:WC.CODE (replaced)' in tr),
               ('refused: a sub-volume into itself, RENAME to another place, DELETE of a sub-volume, MOVE of the path',
                'WORK:A: not into itself' in tr and 'rename: to another place: MOVE' in tr
                and 'delete: WORK:A is a sub-volume' in tr and "move: the shell's path is in WORK:A" in tr),
               ('a whole sub-volume MOVEd to another disk, its files there, RMDIR with files after a Y',
                'WORK:E -> SPARE:E' in tr and 'WC.CODE' in seg('shell> dir #9:E', 'shell> ')
                and 'SPARE:/E/ (#9)' in tr and '#9:E removed' in tr)]
    names = sorted(e[3] for e in ucsdvol.Volume(img).entries)
    dirc = seg('shell> dir c\n', 'shell> ')
    checks += [('DIR c (a sub-volume on the prefix): what is in it, D marked Sub-volume',
                'WORK:/C/ (#5)' in dirc and re.search(r'^D +10 .*Sub-volume', dirc, re.M) is not None),
               ('DIR #5:A/: B marked Sub-volume', re.search(r'^B +60 .*Sub-volume', tr, re.M) is not None),
               ('CD c, CD d: into sub-volumes by their names', 'path is WORK:/C/D (#5)' in tr),
               ('RMDIR of the path the shell is in (or one that holds it): refused', "rmdir: the shell's path is in #5:/C/D" in tr
                and "rmdir: the shell's path is in #5:C (CD" in tr),
               ('RMDIR of a sub-volume with files: asks; N keeps it', tr.count('#9:E holds 1 file: remove it and that one (Y/N)? N')
                == 1 and tr.count('SPARE:/E/ (#9)') == 2),
               ('RMDIR of a file: refused', 'rmdir: #5:SHELL.CODE is not a sub-volume' in tr),
               ('RMDIR d, RMDIR c: gone (#5: holds %s)' % ' '.join(names),
                'd removed' in tr and 'c removed' in tr and 'C' not in names and 'A' in names)]
    bs = tr[tr.find('shell> mkdir #5:T 200'):]
    checks += [('backslashes as slashes: COPY #5:\\*.code #5:\\t, DIR, CD, WC, GREP in sub-volumes',
                'WORK:WC.CODE -> WORK:/T/WC.CODE' in bs and 'WORK:/T/ (#5)' in bs and 'path is WORK:/T (#5)' in bs
                and re.search(r'125 #5:.a.b.C3\.TEXT', bs) is not None and 'WORK:/A/B/C3.TEXT:' in bs),
               ('no file named with a backslash (COPY to #5:X\\Y: no such sub-volume; the old \\TMP: DELETE ?TMP), T removed (%s)'
                % ' '.join(names), not any('\\' in n for n in names) and 'no volume or sub-volume' in bs and 'T' not in names
                and 'WORK:\\TMP deleted' in bs)]
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
