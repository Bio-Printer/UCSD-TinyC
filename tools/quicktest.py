#!/usr/bin/env python3
"""quicktest.py -- the normal regression test: one short session on the P-System (the Tiny-C shell, the
BIGGY.BLK that tools/mkbiggy.py made, the volumes of build/) that uses the compiler, the linker, the
shell's pipes and most of the tools together:

  cc @libs | grep Joining          the library compiles itself (CMPCODE: TCLIB2.OBJ IDENTICAL to the host's)
  hexdump -i system.list | grep -n :10
                                   the 3 lines that are not 16-byte data records: two 64 KB segment records and
                                   the end record (SYSTEM.LIST is 287 blocks)
  lc -1 #11:*.CODE | wc -l         the number of programs on TOOLS:
  wc -lw #11:README.TEXT           lines and words, as the host counts them
  cat #11:README.TEXT | grep -n a | wc -l
                                   the lines without an "a" (case ignored), through two pipes
  diff -u A B >P, patch -i P A, diff A B
                                   a round trip: patching A with the diff gives B (status 0)
  vi: dd, :w, ZZ                   the first line deleted and saved (no crash, the file changed)

The full set of tools/*test*.py is for when a full regression test is asked for.
Uses the mode of PSYS_MODE (native or z80) like the other tools."""
import os, re, sys, shutil, subprocess, tempfile
HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
sys.path.insert(0, HERE)
import ucsdvol
from psys import ensure_setup, BUILD
from voltest import cmpcode
from difftest import make_text

A = ['line %02d of the file' % i for i in range(1, 41)]
B = list(A)
B[4] = 'line 05 CHANGED'
del B[12]
B[20:20] = ['an inserted line', 'and another one']
V = ['first line of V', 'second line of V', 'third line of V']


def main():
    ensure_setup()
    mode = os.environ.get('PSYS_MODE', 'native')
    d = tempfile.mkdtemp(prefix='quick_')
    for v in ('TINY-C', 'TCSRC', 'TCEXTRA', 'TOOLS'):
        shutil.copy(os.path.join(BUILD, v + '.BLK'), d)
    data = os.path.join(d, 'data')                  # BIGGY as mkbiggy.py made it is the boot disk
    os.makedirs(data)
    for f in os.listdir(os.path.join(BUILD, 'data')):
        shutil.copy(os.path.join(BUILD, 'data', f), data)
    shutil.copy(os.path.join(BUILD, 'BIGGY.BLK'), os.path.join(data, 'Big_Disk.BLK'))
    tv = ucsdvol.Volume(os.path.join(d, 'TINY-C.BLK'))
    tv.remove('TCLIB2.OBJ')
    tv.save()
    work = os.path.join(d, 'WORK.BLK')
    ucsdvol.main(['new', work, 'WORK', '600'])
    w = ucsdvol.Volume(work)
    for n, ls in (('A.TEXT', A), ('B.TEXT', B), ('V.TEXT', V)):
        w.write(n, make_text(ls), 3)
    w.save()
    cmds = ['hexdump -i system.list | grep -n :10',
            'lc -1 #11:*.CODE | wc -l',
            'wc -lw #11:README.TEXT',
            'cat #11:README.TEXT | grep -n a | wc -l',
            'diff -u #13:A.TEXT #13:B.TEXT >#13:P.TEXT',
            'patch -s -i #13:P.TEXT #13:A.TEXT',
            'diff #13:A.TEXT #13:B.TEXT',
            'cd #13']
    L = ['WAIT "Command:"', 'TYPE "S"', 'WAIT "shell> "', 'TYPE "cc @libs | grep Joining\\r"',
         'WAIT "Joining TINY-C:TCLIB2.OBJ"', 'WAIT "shell> "']
    for c in cmds:
        L += ['TYPE "%s\\r"' % c, 'WAIT "shell> "']
    L += ['TYPE "vi V.TEXT\\r"', 'WAIT "V.TEXT"', 'TYPE "dd:w\\rZZ"', 'WAIT "shell> "', 'TYPE "bye\\r"', 'WAIT "Command:"']
    L += cmpcode('TINY-C:TCLIB.OBJ', 'TINY-C:TCLIB2.OBJ')
    sp = os.path.join(d, 'quick.script')
    open(sp, 'w').write('\n'.join(L) + '\n')
    env = dict(os.environ, VERIFY_UNIT11=os.path.join(d, 'TOOLS.BLK'), VERIFY_UNIT12=os.path.join(d, 'TCEXTRA.BLK'),
               VERIFY_UNIT13=work)
    r = subprocess.run([os.path.join(BUILD, 'run_verify'), data, os.path.join(d, 'TINY-C.BLK'), os.path.join(d, 'TCSRC.BLK'),
                        sp, mode, os.path.join(d, 'out'), '', '3000'], capture_output=True, text=True, env=env)
    tp = os.path.join(d, 'out', 'transcript.txt')
    tr = open(tp, encoding='latin1').read().replace('\r', '\n') if os.path.exists(tp) else ''
    tr = re.sub(r'\n+', '\n', tr)

    def part(cmd):
        i = tr.find('shell> ' + cmd + '\n')
        if i < 0:
            return None
        i += 8 + len(cmd)
        return tr[i:tr.find('shell> ', i)]

    # what the host expects
    biggy = ucsdvol.Volume(os.path.join(BUILD, 'BIGGY.BLK'))
    sl = biggy.read('SYSTEM.LIST')[0]
    n = (len(sl) + 511) // 512 * 512        # (a file that is not text: whole blocks)
    short = [':02000004%04X%02X' % (off >> 16, (-(2 + 4 + (off >> 16))) & 255) for off in range(65536, n, 65536)]
    if n % 16:                          # (the short last record is not a :10 one either)
        short.append(None)
    short.append(':00000001FF')
    tools = ucsdvol.Volume(os.path.join(BUILD, 'TOOLS.BLK'))
    ncode = len([e for e in tools.entries if e[3].endswith('.CODE')])
    readme = ucsdvol.ucsd_to_text(tools.read('README.TEXT')[0])
    rl = readme.split('\n')[:-1]
    nl, nw = len(rl), len(readme.split())
    noa = len([l for l in rl if 'a' not in l.lower()])
    img = ucsdvol.Volume(work)
    got = lambda n: (ucsdvol.ucsd_to_text(img.read(n)[0]).split('\n')[:-1] if img.find(n) else None)
    hx = part(cmds[0]) or ''
    hxl = [l for l in hx.split('\n') if l and not l.startswith('[exit')]
    checks = [
        ('the script completed', 'VERIFY SCRIPT COMPLETED' in r.stdout),
        ('cc @libs | grep Joining: GREP found the join line', 'Joining TINY-C:TCLIB2.OBJ' in tr[tr.find('cc @libs'):]),
        ('TCLIB2.OBJ IDENTICAL to the host-built TCLIB.OBJ', 'IDENTICAL' in tr),
        ('hexdump -i system.list | grep -n :10: the %d lines that are not data records' % len(short), None not in short and hxl == short),
        ('lc -1 #11:*.CODE | wc -l: %d programs' % ncode, str(ncode) in (part(cmds[1]) or '').split()),
        ('wc -lw #11:README.TEXT: %d lines, %d words' % (nl, nw), [str(nl), str(nw)] == (part(cmds[2]) or '').split()[:2]),
        ('cat | grep -n a | wc -l: %d lines without an a' % noa, str(noa) in (part(cmds[3]) or '').split()),
        ('diff -u | patch | diff: A.TEXT became B.TEXT', got('A.TEXT') == B and '[exit status 0]' in (part(cmds[6]) or '')
         and '[exit status 0]' in (part(cmds[5]) or '')),
        ('vi: dd, :w, ZZ: V.TEXT without its first line, no crash', got('V.TEXT') == V[1:]
         and 'System re-initialized' not in tr and 'STK OFLOW' not in tr),
    ]
    bad = [n for n, g in checks if not g]
    for n, g in checks:
        print('%-5s %s' % ('ok' if g else 'FAIL', n))
    if bad:
        print(tr[-1500:])
        print('A.TEXT:', got('A.TEXT') == B, 'V.TEXT:', got('V.TEXT'), 'hexdump:', hxl[:6])
    print('quick test (%s): %s   (work: %s)' % (mode, 'FAILED' if bad else 'PASSED', d))
    return 1 if bad else 0


if __name__ == '__main__':
    sys.exit(main())
