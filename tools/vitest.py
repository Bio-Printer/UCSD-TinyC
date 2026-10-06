#!/usr/bin/env python3
"""vitest.py -- ports/vi: the same editing session (60 commands: moves,
deletes, yank/put, change, join, marks, search, :s, :set, :w NAME, ZZ, ...)
on Linux (gcc -DSTANDALONE, in a pseudo-terminal) and on the P-System (from
the mini-shell: VI #5:X.C), and the files they save must be identical.

Uses the mode of PSYS_MODE (native or z80) like the other tools; the
P-System VI is built with -z (tclibz.obj) so that it runs in both.
"""
import os, sys, pty, time, select, subprocess, tempfile
HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
sys.path.insert(0, HERE)
from psys import PSystem
from tcrun import compile_c, build_lib, TC, INC
import ucsdvol

VI = os.path.join(ROOT, 'ports', 'vi', 'vi.c')
TEXT = "".join("%s line %d: the quick brown fox (jumps) over [the] lazy {dog}\n" % (w, i)
               for i, w in enumerate(["alpha", "beta", "gamma", "delta", "epsilon",
                                      "zeta", "eta", "theta", "iota", "kappa"] * 3))
KEYS = ["3j", "dd", "p", "2k", "yy", "P", "w", "cw", "CHANGED", "\x1b", "$", "x", "0", "dw",
        "f(", "%", "x", "G", "o", "new last line", "\x1b", "gg", "5J", "/fox\r", "n", "N", "D",
        "10G", "ma", "4j", "d'a", ":s/quick/QUICK/\r", ":3,5d\r", "J", ">>", "<<", "A", " end",
        "\x1b", "I", "start ", "\x1b", "~~~~", "rX", "xp", "ddp", "U", "15|", "i", "MID", "\x1b",
        "e", "b", "B", "W", "E", ":set ts=4\r", ":w #5:Y.C\r", ":1\r", "x", "ZZ"]


def linux(work):
    exe = os.path.join(work, 'vilinux')
    subprocess.check_call(['gcc', '-w', '-DSTANDALONE', '-o', exe, VI])
    x = os.path.join(work, 'X.C')
    open(x, 'w').write(TEXT)
    pid, fd = pty.fork()
    if pid == 0:
        os.chdir(work)
        os.environ['LINES'] = '45'
        os.environ['COLUMNS'] = '132'
        os.execv(exe, [exe, 'X.C'])

    def drain(t):
        end = time.time() + t
        while time.time() < end:
            r, _, _ = select.select([fd], [], [], 0.05)
            if r:
                try:
                    os.read(fd, 4096)
                except OSError:
                    return
    drain(1.0)
    for k in KEYS:
        os.write(fd, k.replace(':w #5:Y.C', ':w Y.C').encode())
        drain(0.15)
    drain(0.5)
    os.waitpid(pid, 0)
    return open(x).read(), open(os.path.join(work, 'Y.C')).read()


def psystem(work):
    lib = build_lib(True)
    code = os.path.join(work, 'VI.CODE')
    r = subprocess.run([TC, '-z', '-I', INC, '-L', lib, VI, '-o', code], capture_output=True, text=True)
    if r.returncode != 0:
        raise SystemExit('vi.c does not compile:\n' + r.stdout + r.stderr)
    ps = PSystem()
    ps.put('VI.CODE', open(code, 'rb').read())
    base, sc = compile_c(os.path.join(ROOT, 'examples', 'shell.c'), ps.dir)
    ps.put('SHELL.CODE', open(sc, 'rb').read())
    ps.put('X.C', TEXT)
    esc = lambda s: s.replace('\\', '\\\\').replace('"', '\\"').replace('\r', '\\r').replace('\x1b', '\\e')
    script = ['WAIT "Command:"', 'TYPE "X"', 'WAIT "Execute what file?"', 'TYPE "#5:SHELL\\r"',
              'WAIT "shell> "', 'TYPE "#5:VI #5:X.C\\r"', 'WAIT "X.C"',
              'TYPE "%s"' % esc(''.join(KEYS)), 'WAIT "shell> "']
    ok, tr, info = ps.run_script('\n'.join(script) + '\n', 300)
    if not ok:
        raise SystemExit('the P-System run did not complete:\n' + info[-500:])
    v = ucsdvol.Volume(os.path.join(ps.out, 'VERIFY_SOURCE.BLK'))
    text = lambda n: ucsdvol.ucsd_to_text(v.read(n)[0])
    return text('X.C'), text('Y.C')


def main():
    work = tempfile.mkdtemp(prefix='vitest_')
    lx, ly = linux(work)
    px, py = psystem(work)
    mode = os.environ.get('PSYS_MODE', 'native')
    good = True
    for name, a, b in (('X.C (ZZ)', lx, px), ('Y.C (:w)', ly, py)):
        same = a == b and a != TEXT
        good = good and same
        print('%-9s %s' % (name, 'same as Linux' if same else 'DIFFERENT from Linux' if a != b else 'NOT EDITED'))
        if a != b:
            import difflib
            for l in difflib.unified_diff(a.split('\n'), b.split('\n'), 'linux', 'p-system', lineterm='', n=0):
                print('  ' + l)
    print('vi test (%s): %s' % (mode, 'PASSED' if good else 'FAILED'))
    return 0 if good else 1


if __name__ == '__main__':
    sys.exit(main())
