#!/usr/bin/env python3
"""tcrun.py -- compile a C program with the host Tiny-C and run it on the P-System.

  tcrun.py prog.c [input-keys]      prints the program's console output

The host compiler is built into build/tc from tinyc/tc.c if needed.
"""
import os, sys, subprocess, shutil
HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
sys.path.insert(0, HERE)
from psys import PSystem, esc

TC = os.path.join(ROOT, 'build', 'tc')
INC = os.path.join(ROOT, 'tinyc', 'include')


def build_tc():
    srcs = [os.path.join(ROOT, 'tinyc', f) for f in os.listdir(os.path.join(ROOT, 'tinyc')) if f.endswith(('.c', '.h'))]
    if not os.path.exists(TC) or any(os.path.getmtime(s) > os.path.getmtime(TC) for s in srcs):
        os.makedirs(os.path.dirname(TC), exist_ok=True)
        subprocess.check_call(['gcc', '-O1', '-w', '-o', TC, os.path.join(ROOT, 'tinyc', 'tc.c')])


LIBSRC = os.path.join(ROOT, 'tinyc', 'lib')
LIB = os.path.join(INC, 'tclib.obj')
LIBZ = os.path.join(INC, 'tclibz.obj')


def z80calls(z80=None):
    """Calls through function pointers for the Z80 interpreter (tc -z; no CSP 138)?
    Default: yes when running in Z80 mode (PSYS_MODE=z80) or TINYC_Z80CALLS=1;
    TINYC_Z80CALLS=0: no, also in Z80 mode (the emulator's Z80-mode coprocessor,
    engine 1.93, does CSP 138). Volumes and packs that must work in both
    modes pass z80=True."""
    if z80 is None:
        zc = os.environ.get('TINYC_Z80CALLS')
        z80 = zc == '1' or (zc != '0' and os.environ.get('PSYS_MODE') == 'z80')
    return bool(z80)


def build_lib(z80=None):
    """compile tinyc/lib/*.c and concatenate the modules into tclib.obj
    (tclibz.obj, built with -z, for the Z80 interpreter)"""
    z = z80calls(z80)
    lib = LIBZ if z else LIB
    build_tc()
    srcs = sorted(f for f in os.listdir(LIBSRC) if f.endswith('.c'))
    newest = max([os.path.getmtime(os.path.join(LIBSRC, f)) for f in srcs] +
                 [os.path.getmtime(os.path.join(INC, f)) for f in os.listdir(INC) if f.endswith('.h')] +
                 [os.path.getmtime(TC)])
    if os.path.exists(lib) and os.path.getmtime(lib) >= newest:
        return lib
    objdir = os.path.join(ROOT, 'build', 'libz' if z else 'lib')
    os.makedirs(objdir, exist_ok=True)
    data = b''
    for f in srcs:
        obj = os.path.join(objdir, f[:-2] + '.obj')
        r = subprocess.run([TC, '-c'] + (['-z'] if z else []) + ['-I', INC, os.path.join(LIBSRC, f), '-o', obj], capture_output=True, text=True)
        if r.returncode != 0:
            raise SystemExit('library build failed (%s):\n%s' % (f, r.stdout + r.stderr))
        data += open(obj, 'rb').read()
    open(lib, 'wb').write(data)
    return lib


def compile_c(src, outdir, z80=None):
    z = z80calls(z80)
    lib = build_lib(z)
    base = os.path.splitext(os.path.basename(src))[0].upper()[:10]
    out = os.path.join(outdir, base + '.CODE')
    r = subprocess.run([TC] + (['-z'] if z else []) + ['-I', INC, '-L', lib, src, '-o', out], capture_output=True, text=True)
    if r.returncode != 0:
        raise SystemExit('compile failed:\n' + r.stdout + r.stderr)
    return base, out


def run_c(src, keys='', extra_files=(), timeout=300):
    ps = PSystem()
    base, code = compile_c(src, ps.dir)
    ps.put(base + '.CODE', open(code, 'rb').read())
    for name, data in extra_files:
        ps.put(name, data)
    script = ['WAIT "Command:"', 'TYPE "X"', 'WAIT "Execute what file?"', 'TYPE "#5:%s\\r"' % base]
    if keys:
        script.append('TYPE "%s"' % esc(keys))
    script.append('WAIT "Command:"')
    ok, tr, info = ps.run_script('\n'.join(script) + '\n', timeout)
    i = tr.find('#5:%s' % base)
    out = tr[i + len(base) + 4:] if i >= 0 else tr
    # the Command: prompt the run ended at -- perhaps only partly printed
    # when the script's WAIT "Command:" stopped the run
    j = out.rfind('Command:')
    if j >= 0 and '\n' not in out[j:]:
        out = out[:j]
    return ok, out.strip('\n'), ps, info


if __name__ == '__main__':
    ok, out, ps, info = run_c(sys.argv[1], sys.argv[2] if len(sys.argv) > 2 else '')
    print(out)
    if not ok:
        print('--- run did not complete:', info)
