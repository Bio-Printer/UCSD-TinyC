#!/usr/bin/env python3
"""vitest.py -- ports/vi: the same editing sessions on Linux (gcc
-DSTANDALONE: the whole file in memory, in a pseudo-terminal) and on the
P-System (from the mini-shell: VI #5:X.C; the file paged through VI.SWAP),
and the files they save must be identical:
  * 60 commands on a small file (moves, deletes, yank/put, change, join,
    marks, search, :s, :set, :w NAME, ZZ, ...)
  * a 1500-line file, many times VI's window: G, gg, NG, searches both
    ways and wrapping, j over window edges, ^F ^B, marks, ranges, ZZ
  * the PC's Page Up, Page Down, Home, End, Insert and Delete keys, in
    command and insert mode (Linux: the terminal's sequences; the
    P-System: the one-byte codes the emulator sends for vi)

Uses the mode of PSYS_MODE (native or z80) like the other tools; the
P-System VI is built with -z (tclibz.obj) so that it runs in both.

  vitest.py --build   on the volumes (build/, tools/mkvolume.py): @TOOLS on
                      TOOLSRC: with TINY-C:CC, in PSYS_MODE's mode and the
                      normal layout; --build harvard: in P-Code mode with
                      the Harvard layout; the VI.CODE it makes must be
                      TOOLS:VI.CODE
"""
import os, sys, pty, time, select, subprocess, tempfile
HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
sys.path.insert(0, HERE)
from psys import PSystem
from tcrun import compile_c, compile_modules
from mkvolume import VI_MODULES
import ucsdvol

VI = [os.path.join(ROOT, m) for m in VI_MODULES]
TEXT = "".join("%s line %d: the quick brown fox (jumps) over [the] lazy {dog}\n" % (w, i)
               for i, w in enumerate(["alpha", "beta", "gamma", "delta", "epsilon",
                                      "zeta", "eta", "theta", "iota", "kappa"] * 3))
KEYS = ["3j", "dd", "p", "2k", "yy", "P", "w", "cw", "CHANGED", "\x1b", "$", "x", "0", "dw",
        "f(", "%", "x", "G", "o", "new last line", "\x1b", "gg", "5J", "/fox\r", "n", "N", "D",
        "10G", "ma", "4j", "d'a", ":s/quick/QUICK/\r", ":3,5d\r", "J", ">>", "<<", "A", " end",
        "\x1b", "I", "start ", "\x1b", "~~~~", "rX", "xp", "ddp", "U", "15|", "i", "MID", "\x1b",
        "e", "b", "B", "W", "E", ":set ts=4\r", ":w #5:Y.C\r", ":1\r", "x", "ZZ"]


BIG = "".join("line %d: %s\n" % (i, " ".join(["alpha", "beta", "gamma", "delta"][(i * j) % 4]
                                                for j in range(1 + i % 7))) if i % 13 else "\n"
              for i in range(1, 1501))
BIGKEYS = ["G", "o", "the end", "\x1b", "gg", "O", "the start", "\x1b", "700G", "dd", "x",
           "/line 1234:\r", "x", "n", "?line 99:\r", "dw", "1400G", "yy", "gg", "p"] + ["j"] * 60 + \
          ["x"] + ["\x06"] * 20 + ["dd"] + ["\x02"] * 7 + ["x", ":1000,1010d\r", "500G", "ma", "900G",
           "'a", "x", ":1450\r", "5dd", "/line 3:\r", "x", "ZZ"]
# the PC's Page Up ... Delete keys: on Linux the terminal's sequences; on the
# P-System one code each (psys.h KEY_PGUP ...: what the emulator sends while
# vi has set SYSCOM->expansion[1] = PX_KEYS)
PCKEYS = {'<PGDN>': ('\x1b[6~', '\x89'), '<PGUP>': ('\x1b[5~', '\x88'), '<HOME>': ('\x1b[H', '\x84'),
          '<END>': ('\x1b[F', '\x85'), '<INS>': ('\x1b[2~', '\x86'), '<DEL>': ('\x1b[3~', '\x87')}
KEYKEYS = ["<PGDN>", "x", "<PGUP>", "j", "<END>", "x", "<HOME>", "x", "3j", "<DEL>", "<DEL>", "<INS>", "abc",
           "<END>", " end", "<HOME>", "start ", "\x1b", "<PGDN>", "<PGUP>", "5j", "<HOME>", "<DEL>", "ZZ"]
SESSIONS = [('small file', TEXT, KEYS, True), ('1500 lines', BIG, BIGKEYS, False),
            ('PC keys', TEXT, KEYKEYS, False)]


def pckeys(keys, which):
    return [PCKEYS[k][which] if k in PCKEYS else k for k in keys]


def linux(work, text, keys, y):
    exe = os.path.join(work, 'vilinux')
    if not os.path.exists(exe):
        subprocess.check_call(['gcc', '-w', '-DSTANDALONE', '-o', exe] + VI)
    x = os.path.join(work, 'X.C')
    open(x, 'w').write(text)
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
    for k in pckeys(keys, 0):
        os.write(fd, k.replace(':w #5:Y.C', ':w Y.C').encode())
        drain(0.15)
    drain(0.5)
    os.waitpid(pid, 0)
    return open(x).read(), open(os.path.join(work, 'Y.C')).read() if y else None


def psystem(work, text, keys, y):
    code = compile_modules(VI, os.path.join(work, 'VI.CODE'), True)
    ps = PSystem()
    ps.put('VI.CODE', open(code, 'rb').read())
    base, sc = compile_c(os.path.join(ROOT, 'examples', 'shell.c'), ps.dir)
    ps.put('SHELL.CODE', open(sc, 'rb').read())
    ps.put('X.C', text)
    esc = lambda s: ''.join('\\\\' if c == '\\' else '\\"' if c == '"' else '\\r' if c == '\r' else '\\e' if c == '\x1b'
                            else c if ' ' <= c <= '~' else '\\x%02X' % ord(c) for c in s)
    script = ['WAIT "Command:"', 'TYPE "X"', 'WAIT "Execute what file?"', 'TYPE "#5:SHELL\\r"',
              'WAIT "shell> "', 'TYPE "#5:VI #5:X.C\\r"', 'WAIT "X.C"',
              'TYPE "%s"' % esc(''.join(pckeys(keys, 1))), 'WAIT "shell> "']
    ok, tr, info = ps.run_script('\n'.join(script) + '\n', 900)
    if not ok:
        raise SystemExit('the P-System run did not complete:\n' + info[-500:])
    v = ucsdvol.Volume(os.path.join(ps.out, 'VERIFY_SOURCE.BLK'))
    text = lambda n: ucsdvol.ucsd_to_text(v.read(n)[0])
    return text('X.C'), text('Y.C') if y else None


def build_on_psystem(harvard):
    import shutil
    from psys import BUILD
    from voltest import prefix
    mode = 'native' if harvard else os.environ.get('PSYS_MODE', 'native')
    layout = 'P-Code mode, Harvard layout' if harvard else '%s mode, normal layout' % mode
    work = tempfile.mkdtemp(prefix='vibuild_')
    for v in ('TINY-C', 'TOOLSRC', 'TOOLS'):
        shutil.copy(os.path.join(BUILD, v + '.BLK'), work)
    script = ['WAIT "Command:"'] + prefix('TOOLSRC:') + [
        'TYPE "X"', 'WAIT "Execute what file?"', 'TYPE "TINY-C:CC\\r"',
        'WAIT "Compile what file?"', 'TYPE "@TOOLS\\r"', 'WAIT "Done."', 'WAIT "Command:"']
    sp = os.path.join(work, 'script')
    open(sp, 'w').write('\n'.join(script) + '\n')
    out = os.path.join(work, 'out')
    r = subprocess.run([os.path.join(BUILD, 'run_verify'), os.path.join(BUILD, 'data'), os.path.join(work, 'TINY-C.BLK'),
                        os.path.join(work, 'TOOLSRC.BLK'), sp, mode, out, '', '1800'], capture_output=True, text=True,
                       env=dict(os.environ, VERIFY_RECLAIM='1', VERIFY_HARVARD='1') if harvard else os.environ)
    tr = open(os.path.join(out, 'transcript.txt'), encoding='latin1').read()
    built = None
    for f in os.listdir(out):
        if f.endswith('.BLK'):
            v = ucsdvol.Volume(os.path.join(out, f))
            if v.volname == 'TOOLSRC' and v.find('VI.CODE'):
                built = v.read('VI.CODE')[0]
    shipped = ucsdvol.Volume(os.path.join(work, 'TOOLS.BLK')).read('VI.CODE')[0]
    ok = 'VERIFY SCRIPT COMPLETED' in r.stdout and (not harvard or 'harvard layout: yes' in r.stdout) and built == shipped
    print('@TOOLS on TOOLSRC: (%s): %s' % (layout, 'VI.CODE identical to TOOLS:VI.CODE' if ok else 'FAILED'))
    if not ok:
        print(tr[tr.find('Compile what'):][-800:])
    print('vi build test: %s' % ('PASSED' if ok else 'FAILED'))
    return 0 if ok else 1


def main():
    if sys.argv[1:2] == ['--build']:
        return build_on_psystem(sys.argv[2:] == ['harvard'])
    work = tempfile.mkdtemp(prefix='vitest_')
    mode = os.environ.get('PSYS_MODE', 'native')
    good = True
    for title, text, keys, y in SESSIONS:
        lx, ly = linux(work, text, keys, y)
        px, py = psystem(work, text, keys, y)
        for name, a, b in (('X.C (ZZ)', lx, px), ('Y.C (:w)', ly, py)):
            if a is None:
                continue
            same = a == b and a != text
            good = good and same
            print('%-11s %-9s %s' % (title, name, 'same as Linux' if same else 'DIFFERENT from Linux' if a != b else 'NOT EDITED'))
            if a != b:
                import difflib
                for l in list(difflib.unified_diff(a.split('\n'), b.split('\n'), 'linux', 'p-system', lineterm='', n=0))[:20]:
                    print('  ' + l)
    print('vi test (%s): %s' % (mode, 'PASSED' if good else 'FAILED'))
    return 0 if good else 1


if __name__ == '__main__':
    sys.exit(main())
