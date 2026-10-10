#!/usr/bin/env python3
"""bindertest.py -- BINDERC (ports/binder/binderc.c, TOOLS:BINDERC.CODE) against
the Pascal System's BINDER.CODE (its source is lost): both are run from X(ecute)
on the boot disk of the emulator's data (BIGGY's MYGOTOXY is the GOTOXY file)
and must make the very same SYSTEM.PASCAL.  Mode: PSYS_MODE (native or z80).
"""
import os, sys, struct
HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
sys.path.insert(0, HERE)
from psys import PSystem
import ucsdvol
from tcrun import compile_c


def run(prog, ps):
    script = ['WAIT "Command:"', 'TYPE "X"', 'WAIT "Execute what file?"', 'TYPE "%s\\r"' % prog,
              'WAIT "GOTOXY"', 'TYPE "MYGOTOXY\\r"', 'WAIT "Command:"']
    ok, tr, out = ps.run_script('\n'.join(script) + '\n', 300)
    boot = ucsdvol.Volume(os.path.join(ps.out, 'VERIFY_BOOT.BLK'))
    return ok, tr, boot.read('SYSTEM.PASCAL')[0]


def main():
    ps = PSystem()
    base, code = compile_c(os.path.join(ROOT, 'ports', 'binder', 'binderc.c'), ps.dir)
    ps.put(base + '.CODE', open(code, 'rb').read())
    ok1, tr1, ref = run('BINDER', ps)
    ok2, tr2, got = run('#5:BINDERC', ps)
    bad = []
    if not (ok1 and ok2):
        bad.append('a run did not finish')
    # (the bytes after the end of a segment, to its block's end, are whatever
    # the Binder's buffer held: not compared)
    if len(ref) != len(got):
        bad.append('SYSTEM.PASCAL: BINDER %d bytes, BINDERC %d bytes' % (len(ref), len(got)))
    else:
        keep = bytearray(len(ref))
        for i in range(512):
            keep[i] = 1
        for i in range(16):
            a, n = struct.unpack_from('<HH', ref, i * 4)
            for k in range(a * 512, a * 512 + n):
                keep[k] = 1
        diff = [i for i in range(len(ref)) if keep[i] and ref[i] != got[i]]
        if diff:
            bad.append('SYSTEM.PASCAL differs at bytes %s' % diff[:10])
    if len(ref) < 10000 or ref[:4] == b'\0\0\0\0':
        bad.append('BINDER made nothing')
    print('bindertest (%s): %s' % (os.environ.get('PSYS_MODE', 'native'), 'FAILED: ' + '; '.join(bad) if bad else 'PASSED'))
    return 1 if bad else 0


if __name__ == '__main__':
    sys.exit(main())
