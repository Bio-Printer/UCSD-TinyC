#!/usr/bin/env python3
"""voltest.py -- test the three volumes (build/TINY-C.BLK, build/TCEXTRA.BLK,
build/TCTESTS.BLK, made by mkvolume.py) on the P-System: TINY-C: on unit #5,
TCEXTRA: on #9, TCTESTS: on #10.

  @LIBS    on TINY-C:  -> TCLIB2.OBJ (/Z /C), CMPCODE: IDENTICAL to TCLIB.OBJ
  @BUILD   on TINY-C:  -> TINYC2.CODE, CMPCODE: IDENTICAL to TINYC.CODE
  @DEMOS   on TCEXTRA: (compiler and headers from TINY-C:), then QUEENS runs
  @TESTS   on TCTESTS: (likewise), then LONGS runs

Prints the result and the least free memory seen, and where.
"""
import os, sys, re, shutil, subprocess, tempfile
HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
sys.path.insert(0, HERE)
from psys import ensure_setup, BUILD

# the first line LONGS prints (tests/longs.expect)
LONGS1 = open(os.path.join(ROOT, 'tests', 'longs.expect')).readline().strip()


def prefix(v):
    return ['TYPE "F"', 'WAIT "Filer:"', 'TYPE "P"', 'WAIT "Prefix"', 'TYPE "%s\\r"' % v,
            'WAIT "Filer:"', 'TYPE "Q"', 'WAIT "Command:"']


def tinyc(prog, cmd, waits):
    return (['TYPE "X"', 'WAIT "Execute what file?"', 'TYPE "%s\\r"' % prog,
             'WAIT "Compile what file?"', 'TYPE "%s\\r"' % cmd] +
            ['WAIT "%s"' % w for w in waits] + ['WAIT "Done."', 'WAIT "Command:"'])


def cmpcode(a, b):
    return ['TYPE "X"', 'WAIT "Execute what file?"', 'TYPE "TCEXTRA:CMPCODE\\r"',
            'WAIT "First file?"', 'TYPE "%s\\r"' % a, 'WAIT "Second file?"', 'TYPE "%s\\r"' % b,
            'WAIT "IDENTICAL"', 'WAIT "Command:"']


def script():
    L = ['WAIT "Command:"'] + prefix('TINY-C:')
    L += tinyc('TINYC', '@LIBS', ['> /Z /C ASSERT', '> /Z /C TCRT', 'Joining TCLIB2.OBJ'])
    L += cmpcode('TCLIB.OBJ', 'TCLIB2.OBJ')
    L += tinyc('TINYC', '@BUILD', ['> /C MAIN', '> /L TINYC2='])
    L += cmpcode('TINYC.CODE', 'TINYC2.CODE')
    L += prefix('TCEXTRA:')
    L += tinyc('TINY-C:TINYC', '@DEMOS', ['> /Z BOXES', '> /Z CMPCODE', '> /Z SHELL'])
    L += ['TYPE "X"', 'WAIT "Execute what file?"', 'TYPE "QUEENS\\r"', 'WAIT "92"', 'WAIT "Command:"']
    L += prefix('TCTESTS:')
    L += tinyc('TINY-C:TINYC', '@TESTS', ['> /Z CONTROL', '> /Z STRUCTS', '> /Z SYSCOM'])
    L += ['TYPE "X"', 'WAIT "Execute what file?"', 'TYPE "LONGS\\r"', 'WAIT "%s"' % LONGS1, 'WAIT "Command:"']
    return '\n'.join(L) + '\n'


def main():
    ensure_setup()
    d = tempfile.mkdtemp(prefix='voltest_')
    for v in ('TINY-C', 'TCEXTRA', 'TCTESTS'):
        shutil.copy(os.path.join(ROOT, 'build', v + '.BLK'), d)
    sp = os.path.join(d, 'voltest.script')
    open(sp, 'w').write(script())
    env = dict(os.environ, VERIFY_RECLAIM='1', VERIFY_UNIT10=os.path.join(d, 'TCTESTS.BLK'))
    r = subprocess.run([os.path.join(BUILD, 'run_verify'), os.path.join(BUILD, 'data'),
                        os.path.join(d, 'TINY-C.BLK'), os.path.join(d, 'TCEXTRA.BLK'), sp, 'native',
                        os.path.join(d, 'out'), '', '600'], capture_output=True, text=True, env=env)
    ok = 'VERIFY SCRIPT COMPLETED' in r.stdout
    tr = open(os.path.join(d, 'out', 'transcript.txt'), encoding='latin1').read()
    least, where, cmd, step = None, '', '', ''
    for l in tr.split('\n'):
        if l.startswith('> '):
            cmd = l[2:].strip()
        elif re.match(r'(Preprocessing|Compiling|Generating|Linking|Joining)', l):
            step = l.split()[0]
        m = re.search(r'\((\d+) words free\)', l)
        if m and (least is None or int(m.group(1)) < least):
            least, where = int(m.group(1)), '%s, %s' % (cmd, step)
    print([l for l in r.stdout.split('\n') if 'VERIFY' in l][-1:])
    print('least memory: %s words free (%s)' % (least, where))
    print('volume test: %s   (work: %s)' % ('PASSED' if ok else 'FAILED', d))
    return 0 if ok else 1


if __name__ == '__main__':
    sys.exit(main())
