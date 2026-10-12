#!/usr/bin/env python3
"""quicktest.py -- the normal regression test: the library compiles itself on the P-System, through a pipe.

From the Tiny-C shell:  cc @libs | grep Joining   (@LIBS: every library module compiled and joined into
TINY-C:TCLIB2.OBJ; the shell pipes CC's output into GREP, which looks for the line CC prints when the join
is done).  Then CMPCODE says TCLIB2.OBJ is IDENTICAL to the TCLIB.OBJ the host built.  So one short run
(about a minute) uses the compiler, the linker, the shell's pipes, GREP, and the volumes together.

The full set of tools/*test*.py is for when a full regression test is asked for.
Uses the mode of PSYS_MODE (native or z80) like the other tools."""
import os, sys, shutil, subprocess, tempfile
HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
sys.path.insert(0, HERE)
import ucsdvol
from psys import ensure_setup, BUILD
from tcrun import compile_c
from voltest import cmpcode


def main():
    ensure_setup()
    mode = os.environ.get('PSYS_MODE', 'native')
    d = tempfile.mkdtemp(prefix='quick_')
    for v in ('TINY-C', 'TCSRC', 'TCEXTRA'):
        shutil.copy(os.path.join(BUILD, v + '.BLK'), d)
    tv = ucsdvol.Volume(os.path.join(d, 'TINY-C.BLK'))
    tv.remove('TCLIB2.OBJ')
    tv.save()
    L = ['WAIT "Command:"', 'TYPE "S"', 'WAIT "shell> "', 'TYPE "cc @libs | grep Joining\\r"',
         'WAIT "Joining TINY-C:TCLIB2.OBJ"', 'WAIT "shell> "', 'TYPE "bye\\r"', 'WAIT "Command:"']
    L += cmpcode('TINY-C:TCLIB.OBJ', 'TINY-C:TCLIB2.OBJ')
    sp = os.path.join(d, 'quick.script')
    open(sp, 'w').write('\n'.join(L) + '\n')
    env = dict(os.environ, VERIFY_UNIT12=os.path.join(d, 'TCEXTRA.BLK'))
    r = subprocess.run([os.path.join(BUILD, 'run_verify'), os.path.join(BUILD, 'data'),
                        os.path.join(d, 'TINY-C.BLK'), os.path.join(d, 'TCSRC.BLK'), sp, mode,
                        os.path.join(d, 'out'), '', '1800'], capture_output=True, text=True, env=env)
    tr = open(os.path.join(d, 'out', 'transcript.txt'), encoding='latin1').read() if os.path.exists(os.path.join(d, 'out', 'transcript.txt')) else ''
    i = tr.find('cc @libs | grep Joining')
    after = tr[i:].replace('\r', '\n')
    checks = [
        ('the script completed', 'VERIFY SCRIPT COMPLETED' in r.stdout),
        ('cc @libs | grep Joining: GREP found the join line', 'Joining TINY-C:TCLIB2.OBJ' in after),
        ('TCLIB2.OBJ IDENTICAL to the host-built TCLIB.OBJ', 'IDENTICAL' in tr),
    ]
    bad = [n for n, g in checks if not g]
    for n, g in checks:
        print('%-5s %s' % ('ok' if g else 'FAIL', n))
    if bad:
        print(tr[-800:])
    print('quick test (%s): %s   (work: %s)' % (mode, 'FAILED' if bad else 'PASSED', d))
    return 1 if bad else 0


if __name__ == '__main__':
    sys.exit(main())
