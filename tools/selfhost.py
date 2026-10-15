#!/usr/bin/env python3
"""selfhost.py -- run the Tiny-C compiler ON the P-System.

  selfhost.py prog.c [TC.CODE]    compile prog.c on the P-System with the
                                  P-code compiler, return the transcript
                                  and the produced .CODE file.

The compiler code file defaults to one built by the host compiler from
tinyc/tc.c.  All headers go onto the work volume as NAME.H.TEXT.
"""
import os, sys, subprocess
HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
sys.path.insert(0, HERE)
from psys import PSystem, esc
from tcrun import compile_c, build_lib, INC, z80calls


def put_headers(ps):
    for f in sorted(os.listdir(INC)):
        if f.endswith('.h'):
            ps.put(f.upper() + '.TEXT', open(os.path.join(INC, f)).read())
    ps.put('TCMSGS.TEXT', open(os.path.join(INC, 'tcmsgs.txt')).read())
    ps.put('TCLIB.OBJ', open(build_lib(), 'rb').read())


def compile_on_psystem(src_files, main_name, tc_code=None, timeout=1800, blocks=4000):
    """src_files: list of (UCSD name, host path). main_name: e.g. 'HELLO'"""
    ps = PSystem(blocks=blocks)
    if tc_code is None:
        base, tc_code = compile_c(os.path.join(ROOT, 'tinyc', 'tc.c'), ps.dir)
    ps.put('TC.CODE', open(tc_code, 'rb').read())
    put_headers(ps)
    for name, path in src_files:
        ps.put(name, open(path).read())
    script = ['WAIT "Command:"', 'TYPE "F"', 'WAIT "Filer:"', 'TYPE "P"', 'WAIT "Prefix"',
              'TYPE "#5:\\r"', 'WAIT "Filer:"', 'TYPE "Q"', 'WAIT "Command:"',
              'TYPE "X"', 'WAIT "Execute what file?"', 'TYPE "TC\\r"',
              'WAIT "Compile what file?"', 'TYPE "%s%s\\r"' % ('-Z ' if z80calls() else '', main_name), 'WAIT "Command:"']
    ok, tr, info = ps.run_script('\n'.join(script) + '\n', timeout)
    return ok, tr, ps, info


if __name__ == '__main__':
    src = sys.argv[1]
    name = os.path.splitext(os.path.basename(src))[0].upper()
    ok, tr, ps, info = compile_on_psystem([(name + '.TEXT', src)], name, sys.argv[2] if len(sys.argv) > 2 else None)
    i = tr.find('Tiny-C')
    print(tr[i:] if i >= 0 else tr[-2000:])
    if not ok:
        print(info)
    try:
        code = ps.get(name + '.CODE')
        open(name + '.CODE', 'wb').write(code)
        print('wrote', name + '.CODE', len(code), 'bytes')
    except SystemExit as e:
        print(e)
