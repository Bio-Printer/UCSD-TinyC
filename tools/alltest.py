#!/usr/bin/env python3
"""alltest.py -- TOOLSRC:ALL.TEXT on the P-System: one batch file that runs
others.  With the prefix on TOOLSRC:, X(ecute TINY-C:CC, @ALL:

  @TCSRC:BUILD   the compiler -> TINY-C:CC2.CODE: CMPCODE IDENTICAL to CC.CODE
  @TCSRC:LIBS    the library  -> TINY-C:TCLIB2.OBJ: IDENTICAL to TCLIB.OBJ
  @TOOLS         every tool linked onto TOOLS: (its tools removed first):
                 the same bytes as mkvolume's
and the compiler's and the library's objects are on TCSRC: (their
sources' volume), none on TOOLSRC: (the prefix).

Disks: #5 TINY-C:, #9 TCSRC:, #10 TOOLSRC:, #11 TOOLS:, #12 TCEXTRA:
(CMPCODE).  Uses the mode of PSYS_MODE (native or z80); about 25 minutes
in Z80 mode.
"""
import os, sys, re, shutil, subprocess, tempfile
HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
sys.path.insert(0, HERE)
import ucsdvol
from psys import ensure_setup, BUILD
from voltest import prefix, cmpcode
from mkvolume import TOOLS, MODULES, LIBMODS_


def main():
    ensure_setup()
    d = tempfile.mkdtemp(prefix='alltest_')
    for v in ('TINY-C', 'TCSRC', 'TOOLSRC', 'TOOLS', 'TCEXTRA'):
        shutil.copy(os.path.join(BUILD, v + '.BLK'), d)
    names = [name + '.CODE' for name, mods, hdrs, desc in TOOLS]
    tv = ucsdvol.Volume(os.path.join(d, 'TOOLS.BLK'))
    for n in names:                     # what is there after the run, @TOOLS made
        tv.remove(n)
    tv.save()
    L = ['WAIT "Command:"'] + prefix('TOOLSRC:')
    L += ['TYPE "X"', 'WAIT "Execute what file?"', 'TYPE "TINY-C:CC\\r"', 'WAIT "Compile what file?"',
          'TYPE "@ALL\\r"', 'WAIT "> @TCSRC:BUILD"', 'WAIT "> /L TINY-C:CC2="', 'WAIT "> @TCSRC:LIBS"',
          'WAIT "Joining TINY-C:TCLIB2.OBJ"', 'WAIT "> @TOOLS"', 'WAIT "> /L TOOLS:GREP="', 'WAIT "Done."',
          'WAIT "Command:"']
    L += cmpcode('TINY-C:CC.CODE', 'TINY-C:CC2.CODE')
    L += cmpcode('TINY-C:TCLIB.OBJ', 'TINY-C:TCLIB2.OBJ')
    sp = os.path.join(d, 'alltest.script')
    open(sp, 'w').write('\n'.join(L) + '\n')
    mode = os.environ.get('PSYS_MODE', 'native')
    env = dict(os.environ, VERIFY_UNIT10=os.path.join(d, 'TOOLSRC.BLK'), VERIFY_UNIT11=os.path.join(d, 'TOOLS.BLK'),
               VERIFY_UNIT12=os.path.join(d, 'TCEXTRA.BLK'), VERIFY_LOWWATER='1')
    if mode == 'native':
        env['VERIFY_RECLAIM'] = '1'
    r = subprocess.run([os.path.join(BUILD, 'run_verify'), os.path.join(BUILD, 'data'),
                        os.path.join(d, 'TINY-C.BLK'), os.path.join(d, 'TCSRC.BLK'), sp, mode,
                        os.path.join(d, 'out'), '', '3600'], capture_output=True, text=True, env=env)
    vols = {}
    for f in [os.path.join(d, 'out', f) for f in os.listdir(os.path.join(d, 'out'))] + \
             [os.path.join(d, v + '.BLK') for v in ('TOOLSRC', 'TOOLS')]:
        if f.endswith('.BLK'):
            v = ucsdvol.Volume(f)
            if v.volname not in vols or v.volname in ('TCSRC', 'TINY-C'):
                vols.setdefault(v.volname, v)
    shipped = ucsdvol.Volume(os.path.join(BUILD, 'TOOLS.BLK'))
    tools = vols.get('TOOLS')
    tr = open(os.path.join(d, 'out', 'transcript.txt'), encoding='latin1').read()
    objs = [m.upper() + '.OBJ' for m in MODULES + LIBMODS_]
    tcsrc, toolsrc = vols.get('TCSRC'), vols.get('TOOLSRC')
    checks = [
        ('the script completed', 'VERIFY SCRIPT COMPLETED' in r.stdout),
        ('@ALL ran BUILD, LIBS and TOOLS in turn', tr.find('> @TCSRC:BUILD') < tr.find('> @TCSRC:LIBS') < tr.find('> @TOOLS')
         and 'Stopped' not in tr),
        ('TINY-C:CC2.CODE and TINY-C:TCLIB2.OBJ IDENTICAL (CMPCODE)', tr.count('IDENTICAL') >= 2),
        ('every tool linked onto TOOLS:, as mkvolume made it', tools is not None
         and all(tools.find(n) and tools.read(n)[0] == shipped.read(n)[0] for n in names)),
        ('the objects of BUILD and LIBS on TCSRC:, not on TOOLSRC:', tcsrc is not None and toolsrc is not None
         and all(tcsrc.find(o) for o in objs) and not any(toolsrc.find(o) for o in objs)),
    ]
    lw = [l for l in r.stdout.split('\n') if l.startswith('least free memory')]
    print([l for l in r.stdout.split('\n') if 'VERIFY' in l][-1:])
    if lw:
        print('tracked ' + lw[0])
    bad = [n for n, good in checks if not good]
    for n, good in checks:
        print('%-5s %s' % ('ok' if good else 'FAIL', n))
    print('all test (%s): %s   (work: %s)' % (mode, 'FAILED' if bad else 'PASSED', d))
    return 1 if bad else 0


if __name__ == '__main__':
    sys.exit(main())
