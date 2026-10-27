#!/usr/bin/env python3
"""batchtest.py -- the shell's batch files (examples/shell.c: @FILE):

  @B          B.TEXT: a comment and an empty line (skipped), CD, DIR, RENAME,
              a program (GREP: the shell ends while it runs and the batch
              file goes on after it), @C (C.TEXT: a MOVE onto a file that is
              there, which replaces it), then the rest of B
  @D          D.TEXT: a program that ends with exit status 1 stops it: the
              line after it does not run
  @NOSUCH     no such file
  dir #4:SYSTEM.BATCH   gone after the batch files

Uses the mode of PSYS_MODE (native or z80) like the other tools."""
import os, sys, re
HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
sys.path.insert(0, HERE)
from psys import PSystem
import ucsdvol
from tcrun import compile_c

FILES = {'A.C': 'int main(void)\n{\n    printf("Hello");\n    return 0;\n}\n',
         'Y1.TEXT': 'one\n', 'Y2.TEXT': 'two\n', 'END.TEXT': 'END-OF-B\n', 'ENDD.TEXT': 'NOT-REACHED\n',
         'B.TEXT': "' a batch file\n\ncd #5\ndir Y*\nrename Y1.TEXT Z1.TEXT\ngrep Hello A.C\n"
                   "  @C\ntype Z1.TEXT\ntype END.TEXT\n",
         'C.TEXT': 'move Y2.TEXT Z1.TEXT\n',
         'D.TEXT': 'grep nosuch A.C\ntype ENDD.TEXT\n'}


def main():
    ps = PSystem()
    z = os.environ.get('PSYS_MODE') == 'z80'
    for src in (os.path.join(ROOT, 'examples', 'shell.c'), os.path.join(ROOT, 'ports', 'grep', 'grep.c')):
        base, code = compile_c(src, ps.dir, z)
        ps.put(base + '.CODE', open(code, 'rb').read())
    v = ucsdvol.Volume(ps.src)
    for name, text in FILES.items():
        v.write(name, ucsdvol.text_to_ucsd(text), 3)
    v.save()
    script = ['WAIT "Command:"', 'TYPE "X"', 'WAIT "Execute what file?"', 'TYPE "#5:SHELL\\r"', 'WAIT "> "',
              'TYPE "@#5:B\\r"', 'WAIT "END-OF-B"', 'WAIT "> "',
              'TYPE "@d\\r"', 'WAIT "stopped"', 'WAIT "> "',
              'TYPE "@nosuch\\r"', 'WAIT "no file"', 'WAIT "> "',
              'TYPE "dir #4:SYSTEM.BATCH\\r"', 'WAIT "> "', 'TYPE "bye\\r"', 'WAIT "Command:"']
    ok, tr, info = ps.run_script('\n'.join(script) + '\n', 900)
    tr = re.sub(r'\n+', '\n', tr.replace('\r', '\n'))
    print(tr)
    checks = [
        ('the script completed', ok),
        ('the lines shown after the prompt', 'shell> cd #5\n' in tr and 'shell> rename Y1.TEXT Z1.TEXT\n' in tr),
        ('comments skipped', 'a batch file' not in tr.replace("' a batch file", '')),
        ('DIR and RENAME ran', 'Y1.TEXT' in tr and 'WORK:Y1.TEXT -> WORK:Z1.TEXT' in tr),
        ('a program, then the batch file goes on', 'WORK:A.C:3:' in tr or 'printf("Hello")' in tr),
        ('@C in it; MOVE replaces', 'shell> move Y2.TEXT Z1.TEXT\n' in tr and 'WORK:Y2.TEXT -> WORK:Z1.TEXT (replaced)' in tr),
        ('the rest of B after C', tr.find('(replaced)') < tr.find('END-OF-B') and 'two' in tr[tr.find('type Z1.TEXT'):]),
        ('exit status 1 stops it', '@: stopped: exit status 1' in tr and 'NOT-REACHED' not in tr),
        ('no such batch file', '@: no file' in tr),
        ('SYSTEM.BATCH gone', 'SYSTEM.BATCH' not in tr[tr.rfind('dir #4:SYSTEM.BATCH') + 20:]),
    ]
    bad = [n for n, good in checks if not good]
    for n, good in checks:
        print('%-5s %s' % ('ok' if good else 'FAIL', n))
    print('batch test: %s   (work: %s)' % ('FAILED' if bad else 'PASSED', ps.dir))
    return 1 if bad else 0


if __name__ == '__main__':
    sys.exit(main())
