#!/usr/bin/env python3
"""viscrolltest.py -- Tiny-C's vi (TOOLS:VI.CODE) on the P-System with scroll
codes (emulator 2.03: SYSCOM^.EXPANSION[1] = 25607, ESC [ n S / T) and
without (an emulator that leaves the word: vi falls back to rewriting every
row), the screen after every key compared:

  build/run_verify_new   run_verify built from UCSD-Pascal_Windows_Emulator
                         (2.03 or later; g++ as tools/setup.sh does it); run
                         with VERIFY_NOSCROLL=1 it behaves as an emulator
                         before 2.03: the word is not answered, vi notices

vi starts on a 1500-line file; keys: j past the bottom and k past the top of
the screen (a scroll of one line), ^E ^Y, 3 lines a notch, ^D ^U ^F ^B, G, gg,
edits on the way (x u dd p).  PSYS_MODE as the other tools (native default).
"""
import os, sys, subprocess, tempfile, shutil
HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
sys.path.insert(0, HERE)
from psys import PSystem, BUILD
from tcrun import compile_c, compile_modules
from mkvolume import VI_MODULES
import vitest

KEYS = (['j'] * 50 + ['k'] * 55 + ['\x05'] * 4 + ['\x19'] * 5 + ['x', 'u', 'dd', 'j', 'p', 'u', '\x04', '\x15',
        '\x06', '\x02', 'G'] + ['k'] * 3 + ['gg', '30G', 'x', 'dd'] + ['j'] * 20 + ['\x89', '\x88', '\x89'] +
        ['k'] * 10 + ['5G', 'k', 'k', 'ZZ'])


def esc(s):
    return ''.join('\\\\' if c == '\\' else '\\"' if c == '"' else '\\r' if c == '\r' else '\\e' if c == '\x1b'
                   else c if ' ' <= c <= '~' else '\\x%02X' % ord(c) for c in s)


def run(binary, work, text, noscroll):
    ps = PSystem()
    code = compile_modules([os.path.join(ROOT, m) for m in VI_MODULES], os.path.join(work, 'VI.CODE'), True)
    ps.put('VI.CODE', open(code, 'rb').read())
    base, sc = compile_c(os.path.join(ROOT, 'examples', 'shell.c'), ps.dir)
    ps.put('SHELL.CODE', open(sc, 'rb').read())
    ps.put('X.C', text)
    script = ['WAIT "Command:"', 'TYPE "X"', 'WAIT "Execute what file?"', 'TYPE "#5:SHELL\\r"',
              'WAIT "shell> "', 'TYPE "#5:VI #5:X.C\\r"', 'WAIT "X.C"']
    script += ['TYPE "%s"' % esc(k) for k in KEYS]
    script += ['WAIT "shell> "']
    sp = os.path.join(ps.dir, 'run.script')
    open(sp, 'w').write('\n'.join(script) + '\n')
    scr = os.path.join(ps.dir, 'screens.txt')
    shutil.rmtree(ps.out, ignore_errors=True)
    import time
    t0 = time.time()
    r = subprocess.run([binary, os.path.join(BUILD, 'data'), ps.src, ps.spare, sp, os.environ.get('PSYS_MODE', 'native'),
                        ps.out, '', '1800'], capture_output=True, text=True, env=dict(ps.env(), VERIFY_SCREEN=scr, **({'VERIFY_NOSCROLL': '1'} if noscroll else {})))
    print('  %s: %.1f s' % ('without scroll codes' if noscroll else 'with scroll codes', time.time() - t0))
    ok = 'VERIFY SCRIPT COMPLETED' in r.stdout
    taken = [l for l in r.stdout.split('\n') if l.startswith('scroll codes taken')]
    return ok, open(scr, encoding='latin1').read().split('== ')[1:], taken[0] if taken else '?'


def main():
    new = os.path.join(BUILD, 'run_verify_new')
    work = tempfile.mkdtemp(prefix='viscroll_')
    ok1, a, ta = run(new, work, vitest.BIG, True)
    ok2, b, tb = run(new, work, vitest.BIG, False)
    print('old emulator: %d screens, %s; new emulator: %d screens, %s' % (len(a), ta, len(b), tb))
    bad = [i for i in range(min(len(a), len(b))) if a[i].split('\n', 1)[1] != b[i].split('\n', 1)[1]]
    for i in bad[:2]:
        print('screen %d differs (%s)' % (i, a[i].split('\n')[0]))
    good = ok1 and ok2 and not bad and len(a) == len(b) and len(a) > len(KEYS) // 2 \
        and ta.endswith(': 0') and not tb.endswith(': 0') and tb != '?'
    print('vi scroll test (%s): %s' % (os.environ.get('PSYS_MODE', 'native'), 'PASSED' if good else 'FAILED'))
    return 0 if good else 1


if __name__ == '__main__':
    sys.exit(main())
