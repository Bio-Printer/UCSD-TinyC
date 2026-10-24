#!/usr/bin/env python3
"""fulltest.py -- CC when a file it writes does not fit on its disk, on the
P-System: it says so (message 118, the file's name), the file goes (no short
one is left that looks whole), and CC stops -- it does not go on as if the
file were there.

Disks: #5 TINY-C:, #9 BIG: (HELLO.C), #10 TINY: (10 blocks: 4 free).  From
the shell:

  cd #9; cc -c hello            HELLO.OBJ on BIG:
  cc -l #10:hello=hello         the code file does not fit on TINY:: message
                                118 for #10:HELLO.CODE, none on TINY: after
  cc -l hello2=hello            the same link onto BIG: works
  cd #10; cc #9:hello           TCTEMP.TEXT (the prefix's, TINY:) does not
                                fit: message 118 for it, none left on TINY:

Uses the mode of PSYS_MODE (native or z80) like the other tools.
"""
import os, re, shutil, subprocess, sys, tempfile
HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
sys.path.insert(0, HERE)
import ucsdvol
from psys import ensure_setup, BUILD, shell_script, shell_transcript

HELLO = '#include <stdio.h>\nint main(void)\n{\n    printf("hello, %s\\n", "world");\n    return 0;\n}\n'
MSG = 'cannot write all of it'


def main():
    ensure_setup()
    d = tempfile.mkdtemp(prefix='fulltest_')
    shutil.copy(os.path.join(BUILD, 'TINY-C.BLK'), d)
    big = os.path.join(d, 'BIG.BLK')
    ucsdvol.main(['new', big, 'BIG', '400'])
    v = ucsdvol.Volume(big)
    v.write('HELLO.C', ucsdvol.text_to_ucsd(HELLO), 3)
    v.save()
    tiny = os.path.join(d, 'TINY.BLK')
    ucsdvol.main(['new', tiny, 'TINY', '10'])
    cmds = ['cd #9', 'cc -c hello', 'cc -l #10:hello=hello', 'cc -l hello2=hello', 'cd #10', 'cc #9:hello']
    L = ['WAIT "Command:"', 'TYPE "S"']
    for c in cmds:
        L += ['WAIT "shell> "', 'TYPE "%s\\r"' % c]
    L += ['WAIT "shell> "', 'TYPE "bye\\r"', 'WAIT "Command:"']
    sp = os.path.join(d, 'full.script')
    open(sp, 'w').write(shell_script('\n'.join(L) + '\n'))
    mode = os.environ.get('PSYS_MODE', 'native')
    r = subprocess.run([os.path.join(BUILD, 'run_verify'), os.path.join(BUILD, 'data'),
                        os.path.join(d, 'TINY-C.BLK'), big, sp, mode, os.path.join(d, 'out'), '', '1200'],
                       capture_output=True, text=True, env=dict(os.environ, VERIFY_UNIT10=tiny))
    tp = os.path.join(d, 'out', 'transcript.txt')
    tr = shell_transcript(open(tp, encoding='latin1').read()).replace('\r', '\n') if os.path.exists(tp) else ''
    tr = re.sub(r'\n+', '\n', re.sub(r'[A-Z0-9-]*:[/A-Z0-9]*> ', 'shell> ', tr))

    def output(cmd):
        i = tr.find('shell> ' + cmd + '\n')
        out = tr[i + len(cmd) + 8:] if i >= 0 else ''
        return out[:out.find('shell> ')] if 'shell> ' in out else out
    outs = [os.path.join(d, 'out', f) for f in os.listdir(os.path.join(d, 'out')) if f.endswith('.BLK')]
    bv = [v for v in map(ucsdvol.Volume, outs) if v.volname == 'BIG'][0]   # (unit #9 as the run left it: out/)
    tv = ucsdvol.Volume(tiny)
    link, ok2, tmp = output('cc -l #10:hello=hello'), output('cc -l hello2=hello'), output('cc #9:hello')
    print('\n'.join(l for l in tr.split('\n') if re.search(r'^shell> cc|fatal|rror|Linking', l)))
    checks = [
        ('the script completed', 'VERIFY SCRIPT COMPLETED' in r.stdout),
        ('cc -l #10:hello=hello: message 118 for the code file', MSG in link and 'HELLO.CODE' in link.upper()),
        ('... and no HELLO.CODE on TINY:', tv.find('HELLO.CODE') is None),
        ('cc -l hello2=hello: linked onto BIG:', MSG not in ok2 and bv.find('HELLO2.CODE') is not None),
        ('cc #9:hello on TINY:: message 118 for TCTEMP.TEXT', MSG in tmp and 'TCTEMP' in tmp.upper()),
        ('... and nothing left on TINY:', not tv.entries),
    ]
    bad = [n for n, good in checks if not good]
    for n, good in checks:
        print('%-5s %s' % ('ok' if good else 'FAIL', n))
    if bad:
        print(tr[-2500:])
    print('disk full test (%s): %s   (work: %s)' % (mode, 'FAILED' if bad else 'PASSED', d))
    if not bad:
        shutil.rmtree(d, True)
    return 1 if bad else 0


if __name__ == '__main__':
    sys.exit(main())
