#!/usr/bin/env python3
"""selfcompile.py [module ...] -- compile the compiler's modules ON the
P-System with the P-code compiler (built by the host from modules), link
them there, and compare every .OBJ and the final code file with the host
build.  Without arguments: all modules, then the link."""
import os, sys
HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
sys.path.insert(0, HERE)
from psys import PSystem
from selfhost import put_headers
from buildtc import build, MODULES


def main(mods):
    code, log = build()
    hostdir = os.path.dirname(code)
    ps = PSystem(blocks=6000)
    ps.put('CC.CODE', open(code, 'rb').read())
    put_headers(ps)
    ps.put('TC.H.TEXT', open(os.path.join(ROOT, 'tinyc', 'tc.h')).read())
    ps.put('PARSE.H.TEXT', open(os.path.join(ROOT, 'tinyc', 'parse.h')).read())
    ps.put('GEN.H.TEXT', open(os.path.join(ROOT, 'tinyc', 'gen.h')).read())
    for m in MODULES:
        ps.put(m.upper() + '.C', open(os.path.join(ROOT, 'tinyc', m + '.c')).read())
    todo = mods or MODULES
    script = ['WAIT "Command:"', 'TYPE "F"', 'WAIT "Filer:"', 'TYPE "P"', 'WAIT "Prefix"',
              'TYPE "#5:\\r"', 'WAIT "Filer:"', 'TYPE "Q"', 'WAIT "Command:"']
    for m in todo:
        script += ['TYPE "X"', 'WAIT "Execute what file?"', 'TYPE "CC\\r"',
                   'WAIT "Compile what file?"', 'TYPE "/C %s\\r"' % m.upper(), 'WAIT "Command:"']
    if not mods:
        script += ['TYPE "X"', 'WAIT "Execute what file?"', 'TYPE "CC\\r"',
                   'WAIT "Compile what file?"', 'TYPE "/L CC2=%s\\r"' % ','.join(m.upper() for m in MODULES),
                   'WAIT "Command:"']
    ok, tr, info = ps.run_script('\n'.join(script) + '\n', 3000)
    for l in tr.split('\n'):
        if any(k in l for k in ('Preprocessing', 'free', 'error', 'fatal', 'Linking', 'segment', 'OFLOW')):
            print('   ', l.strip())
    same = 0
    for m in todo:
        try:
            p = ps.get(m.upper() + '.OBJ')
        except SystemExit:
            print('NO OBJ ', m)
            continue
        h = open(os.path.join(hostdir, m + '.obj'), 'rb').read()
        p = p[:len(h)] if len(p) >= len(h) and not any(p[len(h):]) else p
        print('SAME   ' if p == h else 'DIFF   ', m, len(h))
        same += p == h
    if not mods:
        try:
            p = ps.get('CC2.CODE')
            h = open(code, 'rb').read()
            # block 0 names segment 1 after the program: CC2 here, CC on the host
            h = h[:72] + b'CC2     ' + h[80:]
            print('CC2.CODE', 'IDENTICAL to the host-built CC.CODE (apart from its name)' if p == h
                  else 'DIFFERENT', len(p), len(h))
        except SystemExit as e:
            print(e)
    if not ok:
        print(info[-500:])


if __name__ == '__main__':
    main(sys.argv[1:])
