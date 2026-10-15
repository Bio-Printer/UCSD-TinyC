#!/usr/bin/env python3
"""alltest.py -- TINY-C:ALL.TEXT on the P-System: one batch file that runs
others.  With the prefix on SPARE: (an empty volume), X(ecute TINY-C:CC,
@TINY-C:ALL:

  @TINY-C:/SRC/CC/BUILD   the compiler -> TINY-C:CC2.CODE: CMPCODE IDENTICAL
                          to CC.CODE
  @TINY-C:/SRC/LIB/LIBS   the library  -> TINY-C:/LIB/TCLIB2.OBJ: IDENTICAL
                          to TCLIB.OBJ
  @TOOLS:/SRC/TOOLS       every tool linked onto TOOLS: (its tools removed
                          first): the same bytes as mkvolume's
and every object is in TINY-C:/OBJ or TOOLS:/OBJ (the batch files' -O),
none among the sources (TINY-C:/SRC/CC, TOOLS:/SRC/VI ...: the names in a
batch file are looked for beside it) or on the prefix volume.

Disks: #5 TINY-C:, #9 SPARE:, #11 TOOLS:, #12 TCEXTRA: (CMPCODE).  Uses the
mode of PSYS_MODE (native or z80); about 25 minutes in Z80 mode.
"""
import os, sys, re, shutil, subprocess, tempfile
HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
sys.path.insert(0, HERE)
import ucsdvol
from psys import ensure_setup, BUILD
from voltest import prefix, cmpcode
from mkvolume import TOOLS, MODULES, LIBMODS_

# what the run prints, in turn
WAITS = ['> @TINY-C:/SRC/CC/BUILD', '> -L TINY-C:CC2=', '> @TINY-C:/SRC/LIB/LIBS',
         'Joining TINY-C:/LIB/TCLIB2.OBJ', '> @TOOLS:/SRC/TOOLS', '> -L TOOLS:GREP=']


def prepare(d):
    """the disks in d: TINY-C, TOOLS (without its tools: @TOOLS makes them), TCEXTRA, an empty
    SPARE; the names of the tools' code files"""
    for v in ('TINY-C', 'TOOLS', 'TCEXTRA'):
        shutil.copy(os.path.join(BUILD, v + '.BLK'), d)
    ucsdvol.main(['new', os.path.join(d, 'SPARE.BLK'), 'SPARE', '400'])
    names = [name + '.CODE' for name, mods, hdrs, desc in TOOLS]
    tv = ucsdvol.Volume(os.path.join(d, 'TOOLS.BLK'))
    for n in names:
        tv.remove(n)
    tv.save()
    return names


def results(d, r, tr, names):
    """the checks of a run: the volumes as it left them (out/ has the first two units' images)"""
    vols = {}
    for f in [os.path.join(d, 'out', f) for f in os.listdir(os.path.join(d, 'out'))] + \
             [os.path.join(d, v + '.BLK') for v in ('TOOLS',)]:
        if f.endswith('.BLK'):
            v = ucsdvol.Volume(f)
            vols.setdefault(v.volname, v)
    shipped = ucsdvol.Volume(os.path.join(BUILD, 'TOOLS.BLK'))
    tools, tinyc, spare = vols.get('TOOLS'), vols.get('TINY-C'), vols.get('SPARE')

    def at(v, path):
        try:
            for p in path:
                v = v.sub(p)
            return v
        except (Exception, SystemExit):
            return None

    def objs_in(v, path, objs):
        v = at(v, path)
        return v is not None and all(v.find(o) for o in objs)

    def no_objs(v, path):
        v = at(v, path)
        return v is not None and not any(e[3].endswith('.OBJ') for e in v.entries)
    pos = [tr.find(w) for w in WAITS]
    checks = [
        ('the script completed', 'VERIFY SCRIPT COMPLETED' in r.stdout),
        ('@TINY-C:ALL ran BUILD, LIBS and TOOLS in turn', -1 not in pos and pos == sorted(pos) and 'Stopped' not in tr),
        ('TINY-C:CC2.CODE and TINY-C:/LIB/TCLIB2.OBJ IDENTICAL (CMPCODE)', tr.count('IDENTICAL') >= 2),
        ('every tool linked onto TOOLS:, as mkvolume made it', tools is not None
         and all(tools.find(n) and tools.read(n)[0] == shipped.read(n)[0] for n in names)),
        ('every object in TINY-C:/OBJ or TOOLS:/OBJ (-O), none in SRC/CC, SRC/LIB, TOOLS:/SRC/VI ... or on the prefix SPARE:',
         tinyc is not None and tools is not None and spare is not None
         and objs_in(tinyc, ['OBJ'], [m.upper() + '.OBJ' for m in MODULES + LIBMODS_])
         and objs_in(tools, ['OBJ'], [os.path.splitext(os.path.basename(m))[0].upper() + '.OBJ'
                                      for name, mods, hdrs, desc in TOOLS for m in mods])
         and no_objs(tinyc, ['SRC', 'CC']) and no_objs(tinyc, ['SRC', 'LIB'])
         and all(no_objs(tools, ['SRC', name]) for name, mods, hdrs, desc in TOOLS)
         and not any(e[3].endswith('.OBJ') for e in spare.entries)),
    ]
    return checks


def main():
    ensure_setup()
    d = tempfile.mkdtemp(prefix='alltest_')
    names = prepare(d)
    L = ['WAIT "Command:"'] + prefix('SPARE:')
    L += ['TYPE "X"', 'WAIT "Execute what file?"', 'TYPE "TINY-C:CC\\r"', 'WAIT "Compile what file?"',
          'TYPE "@TINY-C:ALL\\r"'] + ['WAIT "%s"' % w for w in WAITS] + ['WAIT "Done."', 'WAIT "Command:"']
    L += cmpcode('TINY-C:CC.CODE', 'TINY-C:CC2.CODE')
    L += cmpcode('TINY-C:/LIB/TCLIB.OBJ', 'TINY-C:/LIB/TCLIB2.OBJ')
    sp = os.path.join(d, 'alltest.script')
    open(sp, 'w').write('\n'.join(L) + '\n')
    mode = os.environ.get('PSYS_MODE', 'native')
    env = dict(os.environ, VERIFY_UNIT11=os.path.join(d, 'TOOLS.BLK'),
               VERIFY_UNIT12=os.path.join(d, 'TCEXTRA.BLK'), VERIFY_LOWWATER='1')
    if mode == 'native':
        env['VERIFY_RECLAIM'] = '1'
    r = subprocess.run([os.path.join(BUILD, 'run_verify'), os.path.join(BUILD, 'data'),
                        os.path.join(d, 'TINY-C.BLK'), os.path.join(d, 'SPARE.BLK'), sp, mode,
                        os.path.join(d, 'out'), '', '3600'], capture_output=True, text=True, env=env)
    tr = open(os.path.join(d, 'out', 'transcript.txt'), encoding='latin1').read()
    checks = results(d, r, tr, names)
    lw = [l for l in r.stdout.split('\n') if l.startswith('least free memory')]
    print([l for l in r.stdout.split('\n') if 'VERIFY' in l][-1:])
    if lw:
        print('tracked ' + lw[0])
    bad = [n for n, good in checks if not good]
    for n, good in checks:
        print('%-5s %s' % ('ok' if good else 'FAIL', n))
    if bad:
        print(re.sub(r'\n+', '\n', tr.replace('\r', '\n'))[-2000:])
    print('all test (%s): %s   (work: %s)' % (mode, 'FAILED' if bad else 'PASSED', d))
    return 1 if bad else 0


if __name__ == '__main__':
    sys.exit(main())
