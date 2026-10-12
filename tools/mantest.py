#!/usr/bin/env python3
"""mantest.py -- MAN (ports/man/man.c, TOOLS:MAN.CODE) with the real ports/man/MAN.TEXT beside it, from the
shell on the P-System: the list of pages, one page, several, case, an alias, -k, a missing page, a bad
option, the --More-- stop of a long page and its q, and the page redirected into a file; and that every
program that tools/mkvolume.py puts on TOOLS: has a page.

Uses the mode of PSYS_MODE (native or z80) like the other tools."""
import os, sys, re
HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
sys.path.insert(0, HERE)
from psys import PSystem
import ucsdvol
from tcrun import compile_c
from mkvolume import TOOLS

MAN = open(os.path.join(ROOT, 'ports', 'man', 'MAN.TEXT')).read()
CMDS = ['man', 'man grep', 'man Cat hexdump | cat', 'man -k hex', 'man nosuch', 'man -x', 'man shell >#5:S.TEXT', 'cat #5:S.TEXT | cat -s']


def pages():
    out, cur = {}, None
    for line in MAN.split('\n'):
        if line.startswith('%% '):
            cur = line[3:].split()
            for n in cur:
                out[n.upper()] = (cur, [])
        elif cur is not None:
            out[cur[0].upper()][1].append(line)
    return out


def main():
    ps = PSystem()
    z = os.environ.get('PSYS_MODE') == 'z80'
    for src in ('examples/shell.c', 'ports/man/man.c', 'ports/cat/cat.c'):
        base, code = compile_c(os.path.join(ROOT, src), ps.dir, z)
        ps.put(base + '.CODE', open(code, 'rb').read())
    v = ucsdvol.Volume(ps.src)
    v.write('MAN.TEXT', ucsdvol.text_to_ucsd(MAN), 3)
    v.save()
    script = ['WAIT "Command:"', 'TYPE "X"', 'WAIT "Execute what file?"', 'TYPE "#5:SHELL\\r"', 'WAIT "shell> "',
              'TYPE "cd #5\\r"', 'WAIT "shell> "']
    for c in CMDS:
        script += ['TYPE "%s\\r"' % c, 'WAIT "shell> "']
    script += ['TYPE "man vi\\r"', 'WAIT "--More--"', 'TYPE "\\r"', 'WAIT "--More--"', 'TYPE "q"', 'WAIT "shell> "',
               'TYPE "bye\\r"', 'WAIT "Command:"']
    ok, tr, info = ps.run_script('\n'.join(script) + '\n', 600)
    tr = tr.replace('\r', '\n')

    def part(cmd):
        i = tr.find('shell> ' + cmd + '\n')
        if i < 0:
            return ''
        i += 7 + len(cmd) + 1
        return re.sub(r'\n\n', '\n', tr[i:tr.find('shell> ', i)])
    pg = pages()
    names = {n.upper() for n in (x[0][0] for x in pg.values())}
    p = {c: part(c) for c in CMDS}
    after = tr[tr.rfind('shell> man vi'):]
    checks = [
        ('the script completed', ok),
        ('man: one line a page', all(re.search(r'^%-10s' % n.upper(), p['man'], re.M) for n in names)
         and 'GREP' in p['man'] and 'the lines of text files that match a pattern' in p['man']),
        ('man grep: the page', 'GREP [-i] PATTERN' in p['man grep'] and '-i       case counts' in p['man grep']
         and 'VI [-R]' not in p['man grep']),
        ('man Cat hexdump: case ignored, two pages', 'CAT [-AbeEnstTuv]' in p['man Cat hexdump | cat'] and
         'HEXDUMP [-i]' in p['man Cat hexdump | cat']),
        ('man -k hex: HEXDUMP and OBJCOPY, not GREP', 'HEXDUMP' in p['man -k hex'] and 'OBJCOPY' in p['man -k hex']
         and 'GREP' not in p['man -k hex']),
        ('man nosuch: no manual entry, status 1', 'no manual entry for nosuch' in p['man nosuch'] and
         '[exit status 1]' in p['man nosuch']),
        ('man -x: invalid option, status 1', "invalid option -- 'x'" in p['man -x'] and '[exit status 1]' in p['man -x']),
        ('man shell >FILE: the page in a file', 'SHELL' in p['cat #5:S.TEXT | cat -s'] and 'REDIRECTION AND PIPES' in p['cat #5:S.TEXT | cat -s']),
        ('man vi stops at --More--, RETURN goes on a line, q ends it', tr.count('--More--') >= 2 and '[exit status 0]' in after),
        ('every program on TOOLS: has a page', all(n in pg for n, _, _, _ in TOOLS)),
    ]
    bad = [n for n, good in checks if not good]
    for n, good in checks:
        print('%-5s %s' % ('ok' if good else 'FAIL', n))
    print('man test: %s   (work: %s)' % ('FAILED' if bad else 'PASSED', ps.dir))
    return 1 if bad else 0


if __name__ == '__main__':
    sys.exit(main())
