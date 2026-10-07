#!/usr/bin/env python3
"""findtest.py -- which disk CC takes a file from, on the P-System: the
#include files and the sources an @batch file names.

  A name with a volume (VOL:, #n:, *): there, and only there.  Without:
  every disk is looked at; on one only: that one; on several: the one on
  the disk of the file that names it (the including file, the batch
  file), else the one on the prefix volume, else an error that lists the
  volumes.

Disks: #4 the boot volume, #5 TINY-C: (the compiler, <stdio.h>), #9 ONE:,
#10 TWO:, #11 THREE: (the prefix, set with the Filer).  Every copy CC must
not take is a syntax error, so a clean compile shows it took the right
ones; the program it makes prints what the right ones define.

  ONE:T.TEXT (only on ONE: found by itself)
    /Z MAIN       MAIN.C on ONE: (the batch file's disk), not THREE:'s
    /Z /C MAIN2   F.H on TINY-C: and TWO: only: error, Stopped.
  ONE:MAIN.C includes
    <stdio.h>     only on TINY-C:
    "a.h"         only on TWO:; it includes "e.h": on TWO: and THREE:,
                  TWO:'s (the including file's disk beats the prefix)
    "b.h"         on ONE: and TWO:: ONE:'s (the including file's disk)
    "c.h"         on TWO: and THREE:: THREE:'s (the prefix)
    "TWO:d.h"     a volume named: TWO:'s, though ONE: has one too
  X THREE:MAIN    prints 1 2 3 4 5

Uses the mode of PSYS_MODE (native or z80) like the other tools.
"""
import os, sys, re, shutil, subprocess, tempfile
HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
sys.path.insert(0, HERE)
import ucsdvol
from psys import ensure_setup, BUILD

WRONG = 'this is the wrong copy\n'
FILES = {
    'ONE': {'T.TEXT': '/Z MAIN\n/Z /C MAIN2\n',
            'MAIN.C': '#include <stdio.h>\n#include "a.h"\n#include "b.h"\n#include "c.h"\n'
                      '#include "TWO:d.h"\n'
                      'int main(void) { printf("values %d %d %d %d %d\\n", A, B, C, D, E); return 0; }\n',
            'MAIN2.C': '#include "f.h"\nint x;\n',
            'B.H': '#define B 2\n',
            'D.H': WRONG},
    'TWO': {'A.H': '#define A 1\n#include "e.h"\n',
            'B.H': WRONG,
            'C.H': WRONG,
            'D.H': '#define D 4\n',
            'E.H': '#define E 5\n',
            'F.H': WRONG},
    'THREE': {'MAIN.C': WRONG,
              'C.H': '#define C 3\n',
              'E.H': WRONG},
    'TINY-C': {'F.H': WRONG},
}
SCRIPT = r'''WAIT "Command:"
TYPE "F"
WAIT "Filer:"
TYPE "P"
WAIT "Prefix"
TYPE "THREE:\r"
WAIT "Filer:"
TYPE "Q"
WAIT "Command:"
TYPE "X"
WAIT "Execute what file?"
TYPE "TINY-C:CC\r"
WAIT "Compile what file?"
TYPE "@T\r"
WAIT "Stopped."
WAIT "Command:"
TYPE "X"
WAIT "Execute what file?"
TYPE "THREE:MAIN\r"
WAIT "Command:"
'''


def main():
    ensure_setup()
    d = tempfile.mkdtemp(prefix='findtest_')
    shutil.copy(os.path.join(ROOT, 'build', 'TINY-C.BLK'), d)
    units = {'ONE': 9, 'TWO': 10, 'THREE': 11}
    for name in units:
        ucsdvol.main(['new', os.path.join(d, name + '.BLK'), name, '400'])
    for vol, files in FILES.items():
        v = ucsdvol.Volume(os.path.join(d, vol + '.BLK'))
        for name, text in files.items():
            v.write(name, ucsdvol.text_to_ucsd(text), 3)
        v.save()
    sp = os.path.join(d, 'findtest.script')
    open(sp, 'w').write(SCRIPT)
    mode = os.environ.get('PSYS_MODE', 'native')
    env = dict(os.environ, VERIFY_UNIT10=os.path.join(d, 'TWO.BLK'), VERIFY_UNIT11=os.path.join(d, 'THREE.BLK'))
    r = subprocess.run([os.path.join(BUILD, 'run_verify'), os.path.join(BUILD, 'data'),
                        os.path.join(d, 'TINY-C.BLK'), os.path.join(d, 'ONE.BLK'), sp, mode,
                        os.path.join(d, 'out'), '', '600'], capture_output=True, text=True, env=env)
    tr = open(os.path.join(d, 'out', 'transcript.txt'), encoding='latin1').read().replace('\r', '\n')
    tr = re.sub(r'\n+', '\n', tr)
    main_part = tr[tr.find('> /Z MAIN\n'):tr.find('> /Z /C MAIN2')]
    main2_part = tr[tr.find('> /Z /C MAIN2'):]
    print('\n'.join(l for l in tr.split('\n') if re.search(r'^> |rror|wrong|several|  on |Stopped|values|Linking', l)))
    checks = [
        ('the script completed', 'VERIFY SCRIPT COMPLETED' in r.stdout),
        ('@T found on ONE: (the only disk with it); MAIN.C there, not THREE:\'s', '> /Z MAIN' in tr
         and 'ONE:MAIN.C' in main_part and 'rror' not in main_part and 'Linking' in main_part),
        ('MAIN: each include from the right disk (1 2 3 4 5)', 'values 1 2 3 4 5' in tr),
        ('MAIN2: F.H on TINY-C: and TWO:: an error listing them, Stopped',
         re.search(r'on TINY-C: TWO:', main2_part) is not None and 'Stopped.' in main2_part),
    ]
    bad = [n for n, good in checks if not good]
    for n, good in checks:
        print('%-5s %s' % ('ok' if good else 'FAIL', n))
    print('find test (%s): %s   (work: %s)' % (mode, 'FAILED' if bad else 'PASSED', d))
    return 1 if bad else 0


if __name__ == '__main__':
    sys.exit(main())
