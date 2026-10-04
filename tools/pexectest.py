#!/usr/bin/env python3
"""pexectest.py -- test pexec() (psys.h) and the operating system's part of
it (BIGGY 1.10; the $ part BIGGY 1.11) with the mini-shell examples/shell.c:

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
    ARGS ...                 examples/args.c: main(argc, argv) gets the
                             command line (none, ADD 2 3, MUL 6 7, REPEAT,
                             ECHO with extra blanks, unknown, 81 characters)
    args add 2 3             no volume: found on the one disk that has it
    MEMFREE                  on two disks (#5 and #9): the shell asks, 2
                             runs it, RETURN runs nothing
    bye                      back to the Command: prompt
  X ARGS                     started by the OS afterwards: no arguments
  ?, $                       the Command: prompt's $ starts the shell
                             (*SYSTEM.SHELL), MEMFREE from it, bye

Uses the mode of PSYS_MODE (native or z80) like the other tools.
"""
import os, sys, re
HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
sys.path.insert(0, HERE)
from psys import PSystem
import ucsdvol
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
TYPE "#5:ARGS\r"
WAIT "shell> "
TYPE "#5:ARGS ADD 2 3\r"
WAIT "shell> "
TYPE "#5:ARGS mul 6 7\r"
WAIT "shell> "
TYPE "#5:ARGS REPEAT 2 HELLO\r"
WAIT "shell> "
TYPE "  #5:ARGS   ECHO  A   B  \r"
WAIT "shell> "
TYPE "#5:ARGS FOO\r"
WAIT "shell> "
TYPE "#5:ARGS ECHO LLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLL\r"
WAIT "shell> "
TYPE "args add 2 3\r"
WAIT "shell> "
TYPE "MEMFREE\r"
WAIT "Which one"
TYPE "2\r"
WAIT "shell> "
TYPE "MEMFREE\r"
WAIT "Which one"
TYPE "\r"
WAIT "shell> "
TYPE "bye\r"
WAIT "Command:"
TYPE "X"
WAIT "Execute what file?"
TYPE "#5:ARGS\r"
WAIT "Command:"
TYPE "?"
WAIT "$(hell"
TYPE "$"
WAIT "shell> "
TYPE "#5:MEMFREE\r"
WAIT "shell> "
TYPE "bye\r"
WAIT "Command:"
'''


def main():
    ps = PSystem()
    crash = os.path.join(ps.dir, 'crash.c')
    open(crash, 'w').write(CRASH)
    for src in (os.path.join(ROOT, 'examples', 'shell.c'), os.path.join(ROOT, 'examples', 'memfree.c'),
                os.path.join(ROOT, 'examples', 'args.c'), crash):
        base, code = compile_c(src, ps.dir)
        ps.put(base + '.CODE', open(code, 'rb').read())
    v = ucsdvol.Volume(ps.spare)        # MEMFREE on a second disk (#9) too
    v.write('MEMFREE.CODE', open(os.path.join(ps.dir, 'MEMFREE.CODE'), 'rb').read(), 2)
    v.save()
    ok, tr, info = ps.run_script(SCRIPT, 600)
    tr = tr.replace('\r', '\n')
    free = [int(x) for x in re.findall(r'memfree: (\d+) words free', tr)]
    shell = re.findall(r'shell: (\d+) words free', tr)
    status = re.findall(r'\[exit status (-?\d+)\]', tr)
    print('\n'.join(l for l in tr.split('\n') if re.search(r'free|status|no such|re-init|args:|too long', l)))
    checks = [
        ('the script completed', ok),
        ('memfree ran four times', len(free) == 4),
        ('same free memory from X(ecute and from the shell', len(free) == 4 and len(set(free)) == 1),
        ('the shell has less (its own code)', len(free) == 4 and len(shell) == 1 and int(shell[0]) < free[0]),
        ('exit statuses 7, -2 (execution error), 0 (the Filer), 1 5 42 0 4 2 (ARGS), 5 7 (searched), 7 (from $)',
         status == ['7', '-2', '0', '1', '5', '42', '0', '4', '2', '5', '7', '7']),
        ('MEMFREE on two disks: the shell lists both', 'WORK:MEMFREE  (#5:MEMFREE)' in tr and 'SPARE:MEMFREE  (#9:MEMFREE)' in tr),
        ('ARGS with no arguments: argc 1 (shell, then X(ecute)', tr.count('args: no arguments (argc 1)') == 2),
        ('ARGS ADD 2 3, mul 6 7', 'args: 2 + 3 = 5' in tr and 'args: 6 * 7 = 42' in tr),
        ('ARGS REPEAT 2 HELLO', 'args: 1 HELLO\n' in tr and 'args: 2 HELLO\n' in tr),
        ('ARGS ECHO: words, extra blanks dropped', 'args: argc 4' in tr and 'argv[0] = "#5:ARGS"' in tr
         and 'argv[1] = "ECHO"' in tr and 'argv[2] = "A"' in tr and 'argv[3] = "B"' in tr),
        ('ARGS FOO: unknown', "args: don't know FOO with 1 arguments" in tr),
        ('81 characters: too long', 'command line too long' in tr),
        ('the ? prompt offers $(hell', 'H(alt, $(hell' in tr),
        ('NOSUCH: no such program', 'NOSUCH: no such program' in tr),
    ]
    bad = [name for name, good in checks if not good]
    for name, good in checks:
        print('%-5s %s' % ('ok' if good else 'FAIL', name))
    print('pexec test: %s   (work: %s)' % ('FAILED' if bad else 'PASSED', ps.dir))
    return 1 if bad else 0


if __name__ == '__main__':
    sys.exit(main())
