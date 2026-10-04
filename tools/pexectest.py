#!/usr/bin/env python3
"""pexectest.py -- test pexec() (psys.h) and the operating system's part of
it (OS 1.08, BIGGY 1.10) with the mini-shell examples/shell.c:

  X MEMFREE                  free memory of a program started by the OS
  X SHELL, then in the shell
    MEMFREE                  the same program started by pexec: exactly as
                             much free memory (nothing of the shell is left),
                             exit status 7 back in the shell
    mem                      the shell's own free memory (less: its code)
    NOSUCH                   "no such program"
    CRASH                    an execution error: the system re-initializes,
                             the shell comes back with status -2
    *SYSTEM.FILER.           a Pascal program (status 0)
    bye                      back to the Command: prompt

Uses the mode of PSYS_MODE (native or z80) like the other tools.
"""
import os, sys, re
HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
sys.path.insert(0, HERE)
from psys import PSystem
from tcrun import compile_c

CRASH = '''#include <stdio.h>
int zero;
int main(void) { printf("crash: dividing by zero\\n"); return 5 / zero; }
'''

SCRIPT = r'''WAIT "Command:"
TYPE "X"
WAIT "Execute what file?"
TYPE "#5:MEMFREE\r"
WAIT "Command:"
TYPE "X"
WAIT "Execute what file?"
TYPE "#5:SHELL\r"
WAIT "shell> "
TYPE "#5:MEMFREE\r"
WAIT "shell> "
TYPE "mem\r"
WAIT "shell> "
TYPE "NOSUCH\r"
WAIT "shell> "
TYPE "#5:CRASH\r"
WAIT "continue"
TYPE " "
WAIT "shell> "
TYPE "*SYSTEM.FILER.\r"
WAIT "Filer:"
TYPE "Q"
WAIT "shell> "
TYPE "bye\r"
WAIT "Command:"
'''


def main():
    ps = PSystem()
    crash = os.path.join(ps.dir, 'crash.c')
    open(crash, 'w').write(CRASH)
    for src in (os.path.join(ROOT, 'examples', 'shell.c'), os.path.join(ROOT, 'examples', 'memfree.c'), crash):
        base, code = compile_c(src, ps.dir)
        ps.put(base + '.CODE', open(code, 'rb').read())
    ok, tr, info = ps.run_script(SCRIPT, 600)
    tr = tr.replace('\r', '\n')
    free = [int(x) for x in re.findall(r'memfree: (\d+) words free', tr)]
    shell = re.findall(r'shell: (\d+) words free', tr)
    status = re.findall(r'\[exit status (-?\d+)\]', tr)
    print('\n'.join(l for l in tr.split('\n') if re.search(r'free|status|no such|re-init', l)))
    checks = [
        ('the script completed', ok),
        ('memfree ran twice', len(free) == 2),
        ('same free memory from X(ecute and from the shell', len(free) == 2 and free[0] == free[1]),
        ('the shell has less (its own code)', len(free) == 2 and len(shell) == 1 and int(shell[0]) < free[0]),
        ('exit statuses 7, -2 (execution error), 0 (the Filer)', status == ['7', '-2', '0']),
        ('NOSUCH: no such program', 'NOSUCH: no such program' in tr),
    ]
    bad = [name for name, good in checks if not good]
    for name, good in checks:
        print('%-5s %s' % ('ok' if good else 'FAIL', name))
    print('pexec test: %s   (work: %s)' % ('FAILED' if bad else 'PASSED', ps.dir))
    return 1 if bad else 0


if __name__ == '__main__':
    sys.exit(main())
