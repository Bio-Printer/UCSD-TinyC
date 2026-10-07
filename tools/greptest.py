#!/usr/bin/env python3
"""greptest.py -- GREP (ports/grep/grep.c, TOOLS:GREP.CODE) from the shell
on the P-System:

  grep hello *.c         every disk; case ignored: A.C's Hello, B.C's three
  grep -i hello *.c      -i: case counts -- B.C's two lowercase lines
  grep ^int #5:*.c       ^ and a volume named: A.C's line 1 only
  grep "  indented"      an indented line (blanks kept as DLE in the file)
                         comes out with its blanks
  grep marker150 big.text   a file of several 1 KB pages: line 150
  grep [0-9]+$ disk1?:*  a class, +, $, a volume named with a wildcard
  grep shell *.code      code files are not searched: no text file
  grep x nosuch.z        no such file
  grep                   the usage

Uses the mode of PSYS_MODE (native or z80) like the other tools.
"""
import os, sys, re
HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
sys.path.insert(0, HERE)
from psys import PSystem
import ucsdvol
from tcrun import compile_c

FILES = {
    5: {'A.C': 'int main(void)\n{\n    printf("Hello");\n    return 0;\n}\n',
        'BIG.TEXT': ''.join('line %d%s\n' % (i, ' marker%d' % i if i == 150 else '') for i in range(1, 201))},
    9: {'B.C': 'hello world\nHELLO again\n        indented hello\n'},
    10: {'C.H': '#define N 42\n#define M x\n'},
}
CMDS = ['grep hello *.c', 'grep -i hello *.c', 'grep ^int #5:*.c', 'grep marker150 big.text',
        'grep [0-9]+$ disk1?:*', 'grep shell *.code', 'grep x nosuch.z', 'grep']


def main():
    ps = PSystem()
    for src in (os.path.join(ROOT, 'examples', 'shell.c'), os.path.join(ROOT, 'ports', 'grep', 'grep.c')):
        base, code = compile_c(src, ps.dir)
        ps.put(base + '.CODE', open(code, 'rb').read())
    path10 = os.path.join(ps.dir, 'DISK10.BLK')
    ucsdvol.main(['new', path10, 'DISK10', '400'])
    os.environ['VERIFY_UNIT10'] = path10
    for u, files in FILES.items():
        path = {5: ps.src, 9: ps.spare, 10: path10}[u]
        v = ucsdvol.Volume(path)
        for name, text in files.items():
            v.write(name, ucsdvol.text_to_ucsd(text), 3)
        v.save()
    script = ['WAIT "Command:"', 'TYPE "X"', 'WAIT "Execute what file?"', 'TYPE "#5:SHELL\\r"', 'WAIT "shell> "',
              'TYPE "cd #5\\r"', 'WAIT "shell> "']
    for c in CMDS:
        script += ['TYPE "%s\\r"' % c, 'WAIT "shell> "']
    script += ['TYPE "bye\\r"', 'WAIT "Command:"']
    ok, tr, info = ps.run_script('\n'.join(script) + '\n', 600)
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
    checks = [
        ('the script completed', ok),
        ('grep hello *.c: every disk, case ignored',
         'WORK:A.C:3:     printf("Hello");\n' in part(CMDS[0]) and 'SPARE:B.C:1: hello world\n' in part(CMDS[0])
         and 'SPARE:B.C:2: HELLO again\n' in part(CMDS[0]) and '4 lines in 2 of 2 files' in part(CMDS[0])),
        ('grep -i hello *.c: case counts', 'B.C:2:' not in part(CMDS[1]) and 'A.C' not in part(CMDS[1])
         and '2 lines in 1 of 2 files' in part(CMDS[1])),
        ('an indented line keeps its blanks', 'SPARE:B.C:3:         indented hello\n' in part(CMDS[0])),
        ('grep ^int #5:*.c', 'WORK:A.C:1: int main(void)\n' in part(CMDS[2]) and '1 line in 1 of 1 file' in part(CMDS[2])),
        ('grep marker150 big.text: line 150 of a file of several pages',
         'WORK:BIG.TEXT:150: line 150 marker150\n' in part(CMDS[3]) and '1 line in 1 of 1 file' in part(CMDS[3])),
        ('grep [0-9]+$ disk1?:*', 'DISK10:C.H:1: #define N 42\n' in part(CMDS[4]) and 'C.H:2' not in part(CMDS[4])),
        ('grep shell *.code: code files are not searched', 'grep: no text file *.code on any disk on line' in part(CMDS[5])),
        ('grep x nosuch.z', 'grep: no text file nosuch.z on any disk on line' in part(CMDS[6])),
        ('grep alone: the usage', 'use: GREP [-i] PATTERN' in part(CMDS[7])),
    ]
    bad = [n for n, good in checks if not good]
    for n, good in checks:
        print('%-5s %s' % ('ok' if good else 'FAIL', n))
    print('grep test: %s   (work: %s)' % ('FAILED' if bad else 'PASSED', ps.dir))
    return 1 if bad else 0


if __name__ == '__main__':
    sys.exit(main())
