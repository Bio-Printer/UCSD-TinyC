#!/usr/bin/env python3
"""pchunk.py -- the compiler's least free memory against PCHUNK (util.c: the
size of the blocks its permanent pool takes from the heap).

  pchunk.py [SIZE ...]        default: 128 192 256 320 384 448 512 640 768 1024

For each size it builds CC.CODE with that PCHUNK (util.c is put back
afterwards), then runs on the P-System, one X(ecute TINY-C:CC each, every
command of @BUILD and @LIBS: /C of each compiler module, /L CC2=..., /Z /C
of each library module, /J TCLIB2=....  The emulator tracks the least free
memory (SP - NP at every P-code instruction, VERIFY_LOWWATER; emulator 1.97
or later) of each command.  Printed: the worst over all commands, and the
commands within 300 words of it.

P-Code mode without reclaimed memory: the same layout, and the same
figures, as Z80 mode, many times faster.  Needs build/TINY-C.BLK and
build/TCSRC.BLK (mkvolume.py).  The least depends on how each file's
declarations fill the blocks, so it jumps about from size to size; prefer
a small size near the best to the best by a few words.
"""
import os, sys, re, shutil, subprocess, tempfile
HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
sys.path.insert(0, HERE)
import ucsdvol, buildtc
from psys import BUILD, ensure_setup
from voltest import prefix

UTIL = os.path.join(ROOT, 'tinyc', 'util.c')
LIBMODS = sorted(f[:-2].upper() for f in os.listdir(os.path.join(ROOT, 'tinyc', 'lib')) if f.endswith('.c'))
COMMANDS = (['/C ' + m.upper() for m in buildtc.MODULES] +
            ['/L CC2=' + ','.join(m.upper() for m in buildtc.MODULES)] +
            ['/Z /C ' + m for m in LIBMODS] + ['/J TCLIB2=' + ','.join(LIBMODS)])


def build(size, work):
    """CC.CODE with PCHUNK size"""
    src = open(UTIL).read()
    try:
        open(UTIL, 'w').write(re.sub(r'#define PCHUNK \d+', '#define PCHUNK %d' % size, src, 1))
        return buildtc.build(os.path.join(work, 'pc%d' % size))[0]
    finally:
        open(UTIL, 'w').write(src)


def measure(cc, work):
    """the least free memory (words) of each command, and whether all were Done"""
    d = tempfile.mkdtemp(prefix='run_', dir=work)
    for v in ('TINY-C', 'TCSRC'):
        shutil.copy(os.path.join(ROOT, 'build', v + '.BLK'), d)
    vol = ucsdvol.Volume(os.path.join(d, 'TINY-C.BLK'))
    vol.write('CC.CODE', open(cc, 'rb').read(), 2)
    vol.save()
    L = ['WAIT "Command:"'] + prefix('TCSRC:')
    first = len(L) + 1                  # the first step of the first command
    for c in COMMANDS:
        L += ['TYPE "X"', 'WAIT "Execute what file?"', 'TYPE "TINY-C:CC\\r"',
              'WAIT "Compile what file?"', 'TYPE "%s\\r"' % c, 'WAIT "Command:"']
    sp = os.path.join(d, 'script')
    open(sp, 'w').write('\n'.join(L) + '\n')
    r = subprocess.run([os.path.join(BUILD, 'run_verify'), os.path.join(BUILD, 'data'), os.path.join(d, 'TINY-C.BLK'),
                        os.path.join(d, 'TCSRC.BLK'), sp, 'native', os.path.join(d, 'out'), '', '1800'],
                       capture_output=True, text=True, env=dict(os.environ, VERIFY_LOWWATER='2'))
    steps = dict((int(a), int(b)) for a, b in re.findall(r'low water: step (\d+): (-?\d+) words', r.stdout))
    tr = open(os.path.join(d, 'out', 'transcript.txt'), encoding='latin1').read()
    ok = ('VERIFY SCRIPT COMPLETED' in r.stdout and tr.count('Done.') == len(COMMANDS)
          and not re.search(r'error:|fatal:|Stopped|OFLOW', tr))
    least = []
    for i in range(len(COMMANDS)):
        v = [steps[k] for k in range(first + 6 * i, first + 6 * i + 6) if k in steps]
        least.append(min(v) if v else None)
    shutil.rmtree(d)
    return least, ok


def main(argv):
    sizes = [int(a) for a in argv] or [128, 192, 256, 320, 384, 448, 512, 640, 768, 1024]
    ensure_setup()
    work = tempfile.mkdtemp(prefix='pchunk_')
    for size in sizes:
        least, ok = measure(build(size, work), work)
        known = [x for x in least if x is not None]
        worst = min(known) if known else None
        near = ['%s %d' % (c.split()[-1].split('=')[0], x) for c, x in zip(COMMANDS, least)
                if x is not None and x < worst + 300]
        print('PCHUNK %5d  least %5s words  %s  %s' % (size, worst, '' if ok else '(NOT ALL DONE)', ', '.join(near)),
              flush=True)
    shutil.rmtree(work)
    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
