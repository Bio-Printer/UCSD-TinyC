#!/usr/bin/env python3
"""shellalltest.py -- a total system rebuild the way a user starts it: at the
Command: prompt, $ (the Tiny-C shell), then  cc @all  (TOOLSRC:ALL.TEXT:
the compiler, the library and every tool), and the least free memory of it.

  @TCSRC:BUILD   the compiler -> TINY-C:CC2.CODE: CMPCODE IDENTICAL to CC.CODE
  @TCSRC:LIBS    the library  -> TINY-C:TCLIB2.OBJ: IDENTICAL to TCLIB.OBJ
  @TOOLS         every tool linked onto TOOLS: (its tools removed first):
                 the same bytes as mkvolume's

The shell finds ALL.TEXT on TOOLSRC: itself (a name without a volume is
looked for on every disk), so the Filer's Prefix is not set: nothing else
at the Command: prompt has less memory than the rebuild.

Reported: the emulator's tracked least free memory (SP - NP at every P-code
instruction, VERIFY_LOWWATER) of the whole run, and the lowest passes by the
"(N words free)" CC prints after each (its pass's least: memleast(), psys.h).
Z80 mode and P-Code mode without reclaimed memory (the default here) have the
same layout, so the same figures; PSYS_MODE=z80 takes about 25 minutes.

Disks: #5 TINY-C:, #9 TCSRC:, #10 TOOLSRC:, #11 TOOLS:, #12 TCEXTRA: (CMPCODE).
"""
import os, sys, re, shutil, subprocess, tempfile
HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
sys.path.insert(0, HERE)
import ucsdvol
from psys import ensure_setup, BUILD
from voltest import cmpcode
from mkvolume import TOOLS, MODULES, LIBMODS_


def passes(tr):
    """(words free, pass, command) of every pass in a transcript"""
    res = []
    cmd = None
    for line in tr.split('\n'):
        m = re.match(r'> (.*)', line)
        if m:
            cmd = m.group(1).strip()
        for m in re.finditer(r'(Preprocessing|Compiling|Generating code)[^(]*\((\d+) words free', line):
            res.append((int(m.group(2)), m.group(1), cmd))
        m = re.match(r'\s*\((\d+) words free\)', line)       # the linker's, on a line of its own
        if m:
            res.append((int(m.group(1)), 'Linking', cmd))
    return sorted(res)


def main():
    ensure_setup()
    d = tempfile.mkdtemp(prefix='shellall_')
    for v in ('TINY-C', 'TCSRC', 'TOOLSRC', 'TOOLS', 'TCEXTRA'):
        shutil.copy(os.path.join(BUILD, v + '.BLK'), d)
    names = [name + '.CODE' for name, mods, hdrs, desc in TOOLS]
    tv = ucsdvol.Volume(os.path.join(d, 'TOOLS.BLK'))
    for n in names:                     # what is there after the run, @TOOLS made
        tv.remove(n)
    tv.save()
    L = ['WAIT "Command:"', 'TYPE "$"', 'WAIT "shell> "', 'TYPE "cc @all\\r"',
         'WAIT "> @TCSRC:BUILD"', 'WAIT "> /L TINY-C:CC2="', 'WAIT "> @TCSRC:LIBS"',
         'WAIT "Joining TINY-C:TCLIB2.OBJ"', 'WAIT "> @TOOLS"', 'WAIT "> /L TOOLS:GREP="',
         'WAIT "[exit status 0]"', 'WAIT "shell> "', 'TYPE "bye\\r"', 'WAIT "Command:"']
    L += cmpcode('TINY-C:CC.CODE', 'TINY-C:CC2.CODE')
    L += cmpcode('TINY-C:TCLIB.OBJ', 'TINY-C:TCLIB2.OBJ')
    sp = os.path.join(d, 'shellall.script')
    open(sp, 'w').write('\n'.join(L) + '\n')
    mode = os.environ.get('PSYS_MODE', 'native')
    env = dict(os.environ, VERIFY_UNIT10=os.path.join(d, 'TOOLSRC.BLK'), VERIFY_UNIT11=os.path.join(d, 'TOOLS.BLK'),
               VERIFY_UNIT12=os.path.join(d, 'TCEXTRA.BLK'), VERIFY_LOWWATER='1')
    # (P-Code mode without VERIFY_RECLAIM: Z80 mode's layout, many times faster)
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
        ('cc @all ran BUILD, LIBS and TOOLS in turn, status 0', tr.find('> @TCSRC:BUILD') < tr.find('> @TCSRC:LIBS') < tr.find('> @TOOLS')
         and 'Stopped' not in tr and '[exit status 0]' in tr),
        ('TINY-C:CC2.CODE and TINY-C:TCLIB2.OBJ IDENTICAL (CMPCODE)', tr.count('IDENTICAL') >= 2),
        ('every tool linked onto TOOLS:, as mkvolume made it', tools is not None
         and all(tools.find(n) and tools.read(n)[0] == shipped.read(n)[0] for n in names)),
        ('the objects of BUILD and LIBS on TCSRC:, not on TOOLSRC:', tcsrc is not None and toolsrc is not None
         and all(tcsrc.find(o) for o in objs) and not any(toolsrc.find(o) for o in objs)),
    ]
    print([l for l in r.stdout.split('\n') if 'VERIFY' in l][-1:])
    lw = [l for l in r.stdout.split('\n') if l.startswith('least free memory')]
    if lw:
        print('tracked ' + lw[0])
    for words, kind, cmd in passes(tr)[:8]:
        print('  %5d  %-13s %s' % (words, kind, cmd))
    bad = [n for n, good in checks if not good]
    for n, good in checks:
        print('%-5s %s' % ('ok' if good else 'FAIL', n))
    print('shell cc @all test (%s): %s   (work: %s)' % (mode, 'FAILED' if bad else 'PASSED', d))
    return 1 if bad else 0


if __name__ == '__main__':
    sys.exit(main())
