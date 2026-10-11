#!/usr/bin/env python3
"""viscrolltest.py -- Tiny-C's vi (TOOLS:VI.CODE) on the P-System with the console's scroll
codes (emulator 2.03 and later: ESC [ n S / T) and with a vi that never scrolls (a build with
scroll_ok = 0: it rewrites every row that changed), the screen after every key compared:

  build/run_verify_new   run_verify built from UCSD-Pascal_Windows_Emulator
                         (2.04 or later; g++ as tools/setup.sh does it)

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
    mods = [os.path.join(ROOT, m) for m in VI_MODULES]
    if noscroll:                                # a copy of vi that does not use the scroll codes
        d = os.path.join(work, 'noscroll')
        shutil.copytree(os.path.join(ROOT, 'ports', 'vi'), d)
        u = os.path.join(d, 'viucsd.c')
        t = open(u).read()
        assert 'scroll_ok = 1;' in t
        open(u, 'w').write(t.replace('scroll_ok = 1;', 'scroll_ok = 0;'))
        mods = [os.path.join(d, os.path.basename(m)) for m in VI_MODULES]
    code = compile_modules(mods, os.path.join(work, 'VI%d.CODE' % noscroll), True)
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
                        ps.out, '', '1800'], capture_output=True, text=True, env=dict(ps.env(), VERIFY_SCREEN=scr))
    print('  %s: %.1f s' % ('vi without scrolling' if noscroll else 'vi with scroll codes', time.time() - t0))
    ok = 'VERIFY SCRIPT COMPLETED' in r.stdout
    taken = [l for l in r.stdout.split('\n') if l.startswith('scroll codes taken')]
    return ok, open(scr, encoding='latin1').read().split('== ')[1:], taken[0] if taken else '?'


def main():
    new = os.path.join(BUILD, 'run_verify_new')
    work = tempfile.mkdtemp(prefix='viscroll_')
    ok1, a, ta = run(new, work, vitest.BIG, True)
    ok2, b, tb = run(new, work, vitest.BIG, False)
    print('without scrolling: %d screens, %s; with: %d screens, %s' % (len(a), ta, len(b), tb))
    # (the screens are labelled by the number of keys typed: a screen the run did not see, a key
    # that came while vi was busy, is left out of both)
    ka = {x.split('\n')[0].split()[1]: x for x in a}
    kb = {x.split('\n')[0].split()[1]: x for x in b}
    common = sorted(set(ka) & set(kb), key=int)
    bad = [k for k in common if ka[k].split('\n', 1)[1] != kb[k].split('\n', 1)[1]]
    for k in bad[:2]:
        print('screen after key %s differs' % k)
        la, lb = ka[k].split('\n')[1:], kb[k].split('\n')[1:]
        for y in range(min(len(la), len(lb))):
            if la[y] != lb[y]:
                print('  row %2d without: %r\n         with:    %r' % (y, la[y][:100], lb[y][:100]))
    good = ok1 and ok2 and not bad and len(common) > len(KEYS) // 2 \
        and ta.endswith(': 0') and not tb.endswith(': 0') and tb != '?'
    print('vi scroll test (%s): %s' % (os.environ.get('PSYS_MODE', 'native'), 'PASSED' if good else 'FAILED'))
    return 0 if good else 1


if __name__ == '__main__':
    sys.exit(main())
