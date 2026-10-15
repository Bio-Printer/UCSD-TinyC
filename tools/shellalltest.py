#!/usr/bin/env python3
"""shellalltest.py -- a total system rebuild the way a user starts it: at the
Command: prompt, S (the Tiny-C shell), then  cc @tiny-c:all  (the
compiler, the library and every tool), and the least free memory of it.

  @TINY-C:/SRC/CC/BUILD   the compiler -> TINY-C:CC2.CODE: CMPCODE IDENTICAL
                          to CC.CODE
  @TINY-C:/SRC/LIB/LIBS   the library  -> TINY-C:/LIB/TCLIB2.OBJ: IDENTICAL
                          to TCLIB.OBJ
  @TOOLS:/SRC/TOOLS       every tool linked onto TOOLS: (its tools removed
                          first): the same bytes as mkvolume's
and every object in TINY-C:/OBJ or TOOLS:/OBJ, none among the sources or on
the prefix volume (alltest's checks).  The Filer's Prefix is not set: nothing else at the Command: prompt
has less memory than the rebuild.

Reported: the emulator's tracked least free memory (SP - NP at every P-code
instruction, VERIFY_LOWWATER) of the whole run, and the lowest passes by the
"(N free)" CC prints after each (its pass's least: memleast(), psys.h).
Z80 mode and P-Code mode without reclaimed memory (the default here) have the
same layout, so the same figures; PSYS_MODE=z80 takes about 25 minutes.

Disks: #5 TINY-C:, #9 SPARE:, #11 TOOLS:, #12 TCEXTRA: (CMPCODE).
"""
import os, sys, re, shutil, subprocess, tempfile
HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
sys.path.insert(0, HERE)
import ucsdvol
from psys import ensure_setup, BUILD, shell_script, shell_transcript
from voltest import cmpcode
from alltest import WAITS, prepare, results


def passes(tr):
    """(words free, pass, command) of every pass in a transcript"""
    res = []
    cmd = None
    for line in tr.split('\n'):
        m = re.match(r'> (.*)', line)
        if m:
            cmd = m.group(1).strip()
        for m in re.finditer(r'(Preprocessing|Compiling|Generating code)[^(]*\((\d+) free', line):
            res.append((int(m.group(2)), m.group(1), cmd))
        m = re.match(r'\s*\((\d+) free\)', line)       # the linker's, on a line of its own
        if m:
            res.append((int(m.group(1)), 'Linking', cmd))
    return sorted(res)


def main():
    ensure_setup()
    d = tempfile.mkdtemp(prefix='shellall_')
    names = prepare(d)
    L = ['WAIT "Command:"', 'TYPE "S"', 'WAIT "shell> "', 'TYPE "cc @tiny-c:all\\r"'] + \
        ['WAIT "%s"' % w for w in WAITS] + \
        ['WAIT "[exit status 0]"', 'WAIT "shell> "', 'TYPE "bye\\r"', 'WAIT "Command:"']
    L += cmpcode('TINY-C:CC.CODE', 'TINY-C:CC2.CODE')
    L += cmpcode('TINY-C:/LIB/TCLIB.OBJ', 'TINY-C:/LIB/TCLIB2.OBJ')
    sp = os.path.join(d, 'shellall.script')
    open(sp, 'w').write(shell_script('\n'.join(L) + '\n'))
    mode = os.environ.get('PSYS_MODE', 'native')
    env = dict(os.environ, VERIFY_UNIT11=os.path.join(d, 'TOOLS.BLK'),
               VERIFY_UNIT12=os.path.join(d, 'TCEXTRA.BLK'), VERIFY_LOWWATER='1')
    # (P-Code mode without VERIFY_RECLAIM: Z80 mode's layout, many times faster)
    r = subprocess.run([os.path.join(BUILD, 'run_verify'), os.path.join(BUILD, 'data'),
                        os.path.join(d, 'TINY-C.BLK'), os.path.join(d, 'SPARE.BLK'), sp, mode,
                        os.path.join(d, 'out'), '', '3600'], capture_output=True, text=True, env=env)
    tr = shell_transcript(open(os.path.join(d, 'out', 'transcript.txt'), encoding='latin1').read())
    checks = results(d, r, tr, names)
    checks.insert(2, ('the shell: cc exit status 0', '[exit status 0]' in tr))
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
