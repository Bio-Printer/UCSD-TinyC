#!/usr/bin/env python3
"""buildtc.py -- build CC.CODE (the compiler, P-code) from its modules
with the host Tiny-C: each tinyc/*.c module -> .obj, linked with tclib."""
import os, sys, subprocess
HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
sys.path.insert(0, HERE)
from tcrun import build_lib, TC, INC

MODULES = ['main', 'util', 'types', 'pp', 'lex', 'psym', 'expr', 'decl', 'stmt', 'ir', 'gen', 'link']


def build(outdir=None):
    lib = build_lib(True)               # the library of the shipped volumes (-z): CC.CODE is the same either way
    outdir = outdir or os.path.join(ROOT, 'build', 'tcmod')
    os.makedirs(outdir, exist_ok=True)
    objs = []
    for m in MODULES:
        obj = os.path.join(outdir, m + '.obj')
        r = subprocess.run([TC, '-c', '-I', INC, os.path.join(ROOT, 'tinyc', m + '.c'), '-o', obj],
                           capture_output=True, text=True)
        if r.returncode:
            raise SystemExit('%s: %s' % (m, r.stdout + r.stderr))
        objs.append(obj)
    code = os.path.join(outdir, 'CC.CODE')
    r = subprocess.run([TC, '-L', lib] + objs + ['-o', code], capture_output=True, text=True)
    if r.returncode:
        raise SystemExit(r.stdout + r.stderr)
    return code, r.stdout


if __name__ == '__main__':
    code, log = build()
    print(log)
    print(code)
