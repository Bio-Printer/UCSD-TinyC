#!/usr/bin/env python3
"""ccsubvoltest.py -- CC (tinyc/pp.c findfile, openinc; main.c) with sources, includes, objects and
batch files in a sub-volume, on the P-System.

Disks: #4 the boot volume, #5 TINY-C: (CC, the headers, TCLIB.OBJ), #9 WORK: with the shell and the
sub-volume A (made on the host).  WORK: has copies of X.H and Y.H that CC must not take (syntax errors).

  CD #9:/A; CC /Z MAIN          MAIN.C and "x.h" from A (the shell's path first), MAIN.OBJ in A,
                                MAIN.CODE on WORK: (the OS runs no code file from a sub-volume)
  CD #9:;   CC /Z #9:\\a\\prog2   a path (\\ as /): PROG2.C there, and its "y.h" from A (the including file's
                                sub-volume), PROG2.CODE on WORK:
  CD #9:/A; CC /Z /C M1, /Z /C M2, /L BOTH=M1,M2   the objects found in A
            CC @MK              the batch file MK.TEXT in A, its PROG3.C there
            CC /Z GREP          a big source (TOOLS' GREP.C) on the path: the memory
  #9:MAIN, #9:PROG2, #9:BOTH, #9:PROG3 print what A's files define
then on the host: the .OBJ files in A, no TCTEMP left in A, the .CODE files on WORK:.
Uses the mode of PSYS_MODE (native or z80) like the other tools."""
import os, sys, re, shutil, subprocess, tempfile
HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
sys.path.insert(0, HERE)
import ucsdvol
from psys import ensure_setup, BUILD
from tcrun import compile_c

WRONG = 'this is the wrong copy\n'
A_FILES = {
    'MAIN.C': '#include <stdio.h>\n#include "x.h"\nint main(void) { printf("main %d\\n", X); return 0; }\n',
    'X.H': '#define X 11\n',
    'PROG2.C': '#include <stdio.h>\n#include "y.h"\nint main(void) { printf("prog2 %d\\n", Y); return 0; }\n',
    'Y.H': '#define Y 22\n',
    'M1.C': '#include <stdio.h>\nint two(void);\nint main(void) { printf("both %d\\n", two()); return 0; }\n',
    'M2.C': '#include "x.h"\nint two(void) { return X + 22; }\n',
    'MK.TEXT': '/Z PROG3\n',
    'PROG3.C': '#include <stdio.h>\n#include "x.h"\nint main(void) { printf("prog3 %d\\n", X + 33); return 0; }\n',
    'GREP.C': open(os.path.join(ROOT, 'ports/grep/grep.c')).read(),        # a big one: the memory
}
WORK_FILES = {'X.H': WRONG, 'Y.H': WRONG, 'MAIN.C': WRONG, 'PROG3.C': WRONG}
CMDS = [('cd #9:/A', 'path is'), ('tiny-c:cc /z main', 'exit status'), ('#9:main', 'exit status'),
        ('cd #9:', 'prefix is'), ('tiny-c:cc /z #9:\\\\a\\\\prog2', 'exit status'), ('#9:prog2', 'exit status'),
        ('cd #9:/A', 'path is'), ('tiny-c:cc /z /c m1', 'exit status'), ('tiny-c:cc /z /c m2', 'exit status'),
        ('tiny-c:cc /l both=m1,m2', 'exit status'), ('#9:both', 'exit status'),
        ('tiny-c:cc @mk', 'exit status'), ('#9:prog3', 'exit status'),
        ('tiny-c:cc /z grep', 'exit status'), ('cd #9:', 'prefix is')]


def main():
    ensure_setup()
    z = os.environ.get('PSYS_MODE') == 'z80'
    d = tempfile.mkdtemp(prefix='ccsubvol_')
    shutil.copy(os.path.join(ROOT, 'build', 'TINY-C.BLK'), d)
    work = os.path.join(d, 'WORK.BLK')
    ucsdvol.main(['new', work, 'WORK', '2000'])
    sub = os.path.join(d, 'A.BLK')
    ucsdvol.main(['new', sub, 'A', '300'])
    v = ucsdvol.Volume(sub)
    for name, text in A_FILES.items():
        v.write(name, ucsdvol.text_to_ucsd(text), 3)
    v.save()
    v = ucsdvol.Volume(work)
    for name, text in WORK_FILES.items():
        v.write(name, ucsdvol.text_to_ucsd(text), 3)
    b, code = compile_c(os.path.join(ROOT, 'examples/shell.c'), d)
    v.write('SHELL.CODE', open(code, 'rb').read(), 2)
    v.write('A', open(sub, 'rb').read()[:300 * 512], 5)        # the sub-volume: a data file
    v.save()
    s = ['WAIT "Command:"', 'TYPE "X"', 'WAIT "Execute what file?"', 'TYPE "#9:SHELL\\r"', 'WAIT "shell> "']
    for cmd, wait in CMDS:
        s += ['TYPE "%s\\r"' % cmd, 'WAIT "%s"' % wait, 'WAIT "shell> "']
    s += ['TYPE "bye\\r"', 'WAIT "Command:"']
    sp = os.path.join(d, 'run.script')
    open(sp, 'w').write('\n'.join(s) + '\n')
    r = subprocess.run([os.path.join(BUILD, 'run_verify'), os.path.join(BUILD, 'data'),
                        os.path.join(d, 'TINY-C.BLK'), work, sp, os.environ.get('PSYS_MODE', 'native'),
                        os.path.join(d, 'out'), '', '3600' if z else '600'], capture_output=True, text=True)
    tp = os.path.join(d, 'out', 'transcript.txt')
    tr = open(tp, encoding='latin1').read().replace('\r', '\n') if os.path.exists(tp) else ''
    tr = re.sub(r'\n+', '\n', tr)

    def part(cmd):
        a = tr.find('shell> ' + cmd + '\n')
        return tr[a:tr.find('shell> ', a + 7)] if a >= 0 else ''

    try:
        img = ucsdvol.Volume(os.path.join(d, 'out', 'VERIFY_COMPASM.BLK'))
        work_names = sorted(e[3] for e in img.entries)
        e = [e for e in img.entries if e[3] == 'A'][0]
        data = open(os.path.join(d, 'out', 'VERIFY_COMPASM.BLK'), 'rb').read()[e[0] * 512:e[1] * 512]
        p = os.path.join(d, 'A_after.BLK')
        open(p, 'wb').write(data)
        a_names = sorted(x[3] for x in ucsdvol.Volume(p).entries)
    except (Exception, SystemExit) as x:
        work_names, a_names = ['(%s)' % x], []
    print('\n'.join(l for l in tr.split('\n') if re.search(r'^shell> |rror|wrong|Linking|^main|^prog|^both|Preprocessing', l)))
    checks = [('the script completed', 'VERIFY SCRIPT COMPLETED' in r.stdout),
              ('CC MAIN on the path: no error, linked', 'rror' not in part('tiny-c:cc /z main')
               and 'Linking WORK:MAIN.CODE' in part('tiny-c:cc /z main')),
              ('MAIN prints A\'s X (11)', 'main 11' in part('#9:main')),
              ('CC #9:\\a\\prog2 (backslashes: a path): its y.h from A, PROG2.CODE on #9:', 'rror' not in part('tiny-c:cc /z #9:\\a\\prog2')
               and 'Linking #9:PROG2.CODE' in part('tiny-c:cc /z #9:\\a\\prog2') and 'prog2 22' in part('#9:prog2')),
              ('/C M1, /C M2, /L BOTH=M1,M2 on the path', 'rror' not in part('tiny-c:cc /l both=m1,m2')
               and 'Linking WORK:BOTH.CODE' in part('tiny-c:cc /l both=m1,m2') and 'both 33' in part('#9:both')),
              ('@MK: the batch file in A, its PROG3.C there', 'rror' not in part('tiny-c:cc @mk')
               and 'prog3 44' in part('#9:prog3')),
              ('CC GREP (TOOLS\' GREP.C) on the path: compiled and linked', 'rror' not in part('tiny-c:cc /z grep')
               and 'Linking WORK:GREP.CODE' in part('tiny-c:cc /z grep')),
              ('on the host: the objects in A, no TCTEMP there (%s)' % ' '.join(a_names),
               all(n in a_names for n in ('MAIN.OBJ', 'PROG2.OBJ', 'M1.OBJ', 'M2.OBJ', 'PROG3.OBJ'))
               and not any(n.startswith('TCTEMP') for n in a_names)),
              ('on the host: the code files on WORK:, no TCTEMP left (%s)' % ' '.join(work_names),
               all(n in work_names for n in ('MAIN.CODE', 'PROG2.CODE', 'BOTH.CODE', 'PROG3.CODE'))
               and not any(n.startswith('TCTEMP') for n in work_names))]
    bad = 0
    for what, good in checks:
        print('%-4s  %s' % ('ok' if good else 'BAD', what))
        bad += not good
    if bad:
        print(tr[-3000:])
        print(r.stdout[-600:])
    print('cc sub-volume test (%s): %s   (work: %s)' % ('z80' if z else 'native', 'FAILED' if bad else 'PASSED', d))
    return 1 if bad else 0


if __name__ == '__main__':
    sys.exit(main())
