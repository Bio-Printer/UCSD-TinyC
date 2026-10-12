#!/usr/bin/env python3
"""cattest.py -- CAT (ports/cat/cat.c, TOOLS:CAT.CODE) from the shell on the P-System: every option of
cat(1) (-A -b -e -E -n -s -t -T -u -v, joined as -bn, --) against the host's cat on the same bytes:
text files (P.TEXT, printable; V.TEXT, with control characters and bytes of 128 and up), a binary
file (B.DAT: whole 512-byte blocks), several files (the numbers go on, -s squeezes across them),
the standard input (<FILE, a pipe, -), a missing file, a bad option.

The console shows a tab as nothing, so the tabs are left out of both sides.
Uses the mode of PSYS_MODE (native or z80) like the other tools."""
import os, sys, re, subprocess
HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
sys.path.insert(0, HERE)
from psys import PSystem
import ucsdvol
from tcrun import compile_c

P = 'one\n\n\ntwo\tx\ty\n\n\n\nfour  \n  indented line\n\nlast\n'
V = 'ctl \x01 and \x1f\nlow \x02\tab\x7f del\nhigh \xe9 \xff \x80\x8a\n\n\n\x01\n'
B = b'\x01\x09\x7f\x80\xff\x8a\x1b\r\nabc\x00\x20z'
BPAD = B + b'\x00' * (512 - len(B))


def make_text(text):
    """a UCSD text file as the library reads it, the bytes of the lines as they are (ucsdvol's
    text_to_ucsd turns tabs into blanks and drops trailing ones): 2 header blocks, then 1K pages of
    whole lines, each ended by a CR, filled up with NULs"""
    pages, page = [], b''
    for line in text.split('\n')[:-1]:
        b = line.encode('latin1') + b'\r'
        if len(page) + len(b) > 1023:
            pages.append(page)
            page = b''
        page += b
    pages.append(page)
    return bytes(1024) + b''.join(p + bytes(1024 - len(p)) for p in pages)

# (command, the host's cat arguments and files)
CASES = [
    ('cat #5:P.TEXT', [], ['P']),
    ('cat -n #5:P.TEXT', ['-n'], ['P']),
    ('cat -b #5:P.TEXT', ['-b'], ['P']),
    ('cat -bn #5:P.TEXT', ['-bn'], ['P']),
    ('cat -s #5:P.TEXT', ['-s'], ['P']),
    ('cat -sn #5:P.TEXT', ['-sn'], ['P']),
    ('cat -E #5:P.TEXT', ['-E'], ['P']),
    ('cat -T #5:P.TEXT', ['-T'], ['P']),
    ('cat -u #5:P.TEXT', ['-u'], ['P']),
    ('cat -v #5:V.TEXT', ['-v'], ['V']),
    ('cat -A #5:V.TEXT', ['-A'], ['V']),
    ('cat -e #5:V.TEXT', ['-e'], ['V']),
    ('cat -t #5:V.TEXT', ['-t'], ['V']),
    ('cat -vET -n #5:V.TEXT', ['-vET', '-n'], ['V']),
    ('cat -sA #5:V.TEXT #5:P.TEXT', ['-sA'], ['V', 'P']),
    ('cat -n #5:P.TEXT #5:P.TEXT', ['-n'], ['P', 'P']),
    ('cat -s #5:P.TEXT #5:P.TEXT', ['-s'], ['P', 'P']),
    ('cat -v #5:B.DAT', ['-v'], ['B']),
    ('cat -n <#5:P.TEXT', ['-n'], ['P']),
    ('cat -s -E - <#5:P.TEXT', ['-sE'], ['P']),
    ('cat -n #5:P.TEXT | cat -sE', None, None),
    ('cat -- -n', None, None),
    ('cat #5:NOSUCH.TEXT', None, None),
    ('cat -x #5:P.TEXT', None, None),
]


def host(args, names):
    data = {'P': P.encode('latin1'), 'V': V.encode('latin1'), 'B': BPAD}
    inp = b''.join(data[n] for n in names)
    out = subprocess.run(['cat'] + args, input=inp, capture_output=True).stdout
    return out.decode('latin1').replace('\t', '')


def main():
    ps = PSystem()
    z = os.environ.get('PSYS_MODE') == 'z80'
    for src in (os.path.join(ROOT, 'examples', 'shell.c'), os.path.join(ROOT, 'ports', 'cat', 'cat.c')):
        base, code = compile_c(src, ps.dir, z)
        ps.put(base + '.CODE', open(code, 'rb').read())
    v = ucsdvol.Volume(ps.src)
    v.write('P.TEXT', make_text(P), 3)
    v.write('V.TEXT', make_text(V), 3)
    v.write('B.DAT', BPAD, 5)
    v.save()
    script = ['WAIT "Command:"', 'TYPE "X"', 'WAIT "Execute what file?"', 'TYPE "#5:SHELL\\r"', 'WAIT "shell> "',
              'TYPE "cd #5\\r"', 'WAIT "shell> "']
    for c, _, _ in CASES:
        script += ['TYPE "%s\\r"' % c.replace('\\', '\\\\'), 'WAIT "shell> "']
    script += ['TYPE "bye\\r"', 'WAIT "Command:"']
    ok, tr, info = ps.run_script('\n'.join(script) + '\n', 900)
    tr = tr.replace('\r', '\n')

    def part(cmd):                      # what the command wrote, one \n a line, the status line cut
        i = tr.find('shell> ' + cmd + '\n')
        if i < 0:
            return None
        i += 7 + len(cmd) + 1
        s = tr[i:tr.find('shell> ', i)]
        s = s.lstrip('\n')
        j = s.rfind('[exit status')
        status = s[j:].strip() if j >= 0 else ''
        s = s[:j] if j >= 0 else s
        return s.replace('\n\n', '\n').replace('\t', ''), status
    checks = [('the script completed', ok)]
    for c, args, names in CASES:
        got = part(c)
        if args is not None:
            want = host(args, names)
            good = got is not None and got[0].rstrip('\n') == want.rstrip('\n') and got[1] == '[exit status 0]'
            if not good and got is not None:
                print('--- %s\n got:  %r\n want: %r' % (c, got[0], want))
        elif c.endswith('| cat -sE'):
            want = subprocess.run('cat -n | cat -sE', shell=True, input=P.encode('latin1'), capture_output=True).stdout.decode('latin1')
            good = got is not None and got[0].rstrip('\n') == want.replace('\t', '').rstrip('\n')
        elif c == 'cat -- -n':
            good = got is not None and 'cannot open it' in got[0] and got[1] == '[exit status 1]'
        elif 'NOSUCH' in c:
            good = got is not None and 'cat: #5:NOSUCH.TEXT: cannot open it' in got[0] and got[1] == '[exit status 1]'
        else:
            good = got is not None and "invalid option -- 'x'" in got[0] and 'use: CAT' in got[0] and got[1] == '[exit status 1]'
        checks.append((c, good))
    bad = [n for n, good in checks if not good]
    for n, good in checks:
        print('%-5s %s' % ('ok' if good else 'FAIL', n))
    print('cat test: %s   (work: %s)' % ('FAILED' if bad else 'PASSED', ps.dir))
    return 1 if bad else 0


if __name__ == '__main__':
    sys.exit(main())
