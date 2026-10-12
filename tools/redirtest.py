#!/usr/bin/env python3
"""redirtest.py -- the shell's redirections and pipes (examples/shell.c, library redirect.c):

  grep hello #5:*.c >#5:G.TEXT      a program's output into a file (it takes >NAME from its command line)
  type #5:G.TEXT                    the file
  dir #5:= >#5:L.TEXT                a builtin's output into a file
  grep Hello <#5:A.C             a program's standard input from a file
  dir #5:= | grep .C                 the output of one into the next (a pipe file, gone afterwards)
  dir #5:= | grep .TEXT | grep L     three programs
  grep hello #5:*.c >>#5:G.TEXT     append
  grep p-\\>q #5:X.C                \\> is an ordinary character

Uses the mode of PSYS_MODE (native or z80) like the other tools."""
import os, sys, re
HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
sys.path.insert(0, HERE)
from psys import PSystem
import ucsdvol
from tcrun import compile_c

FILES = {'A.C': 'int main(void)\n{\n    printf("Hello");\n    return 0;\n}\n',
         'X.C': 'p->q = 1;\nr = 2;\n'}
CMDS = ['grep hello #5:*.c >#5:G.TEXT', 'type #5:G.TEXT', 'dir #5:= >#5:L.TEXT', 'type #5:L.TEXT',
        'grep Hello <#5:A.C', 'dir #5:= | grep .C', 'dir #5:= | grep .TEXT | grep L', 'dir #4:PIPE*',
        'grep hello #5:*.c >>#5:G.TEXT', 'type  #5:G.TEXT', 'grep p-\\>q #5:X.C']


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
    script = ['WAIT "Command:"', 'TYPE "X"', 'WAIT "Execute what file?"', 'TYPE "#5:SHELL\\r"', 'WAIT "shell> "',
              'TYPE "cd #5\\r"', 'WAIT "shell> "']
    for c in CMDS:
        script += ['TYPE "%s\\r"' % c.replace('\\', '\\\\'), 'WAIT "shell> "']
    script += ['TYPE "bye\\r"', 'WAIT "Command:"']
    ok, tr, info = ps.run_script('\n'.join(script) + '\n', 900)
    tr = re.sub(r'\n+', '\n', tr.replace('\r', '\n'))

    def part(cmd):
        i = tr.find('shell> ' + cmd + '\n')
        if i < 0:
            return ''
        i += 7 + len(cmd) + 1
        return tr[i:tr.find('shell> ', i)]
    for c in CMDS:
        print('shell> ' + c)
        print(part(c), end='')
    p = {c: part(c) for c in CMDS}
    checks = [
        ('the script completed', ok),
        ('grep ... >file: nothing on the console but the status', 'Hello' not in p[CMDS[0]] and '[exit status 0]' in p[CMDS[0]]),
        ('type: the lines grep wrote', 'WORK:A.C:3:     printf("Hello");' in p[CMDS[1]] and '1 line in 1 of 2 files' in p[CMDS[1]]),
        ('dir >file, then type it', 'A.C' in p[CMDS[3]] and 'X.C' in p[CMDS[3]] and 'A.C' not in p[CMDS[2]]),
        ('grep <file: stdin', 'printf("Hello");' in p[CMDS[4]] and 'main' not in p[CMDS[4]]),
        ('dir | grep .C: one line per .C file, no other', 'A.C' in p[CMDS[5]] and 'X.C' in p[CMDS[5]]
         and 'G.TEXT' not in p[CMDS[5]] and 'L.TEXT' not in p[CMDS[5]]),
        ('three stages', 'L.TEXT' in p[CMDS[6]] and 'G.TEXT' not in p[CMDS[6]] and 'A.C' not in p[CMDS[6]]),
        ('the pipe files are gone', 'PIPE' not in p[CMDS[7]].replace('PIPE*', '') and 'SYSTEM.PIPE' not in p[CMDS[7]]),
        ('>> appends', p[CMDS[9]].count('WORK:A.C:3:') == 2),
        ('\\> is an ordinary character', 'WORK:X.C:1: p->q = 1;' in p[CMDS[10]]),
    ]
    bad = [n for n, good in checks if not good]
    for n, good in checks:
        print('%-5s %s' % ('ok' if good else 'FAIL', n))
    print('redirect test: %s   (work: %s)' % ('FAILED' if bad else 'PASSED', ps.dir))
    return 1 if bad else 0


if __name__ == '__main__':
    sys.exit(main())
