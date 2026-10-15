#!/usr/bin/env python3
"""pagertest.py -- LESS and MORE (ports/less/less.c, ports/more/more.c; TOOLS:LESS.CODE, TOOLS:MORE.CODE)
from the shell on the P-System, the screen (the emulator's 45 x 132 character grid) read after every key:

  less: the first screen, j k, SPACE b, G (END) g, 50g, /text ?text n, d u, the wheel and PgDn, -N, -S and
        wrapped long lines, a pipe (cat F | less) and <FILE, :n :p with two files, q back to the shell
  more: the first screen and its --More-- prompt, SPACE, RETURN, d, /text, -10, q

Uses the mode of PSYS_MODE (native or z80) like the other tools."""
import os, sys, re
HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
sys.path.insert(0, HERE)
from psys import PSystem
import ucsdvol
from tcrun import compile_c

N = 200
LONG = 120                              # the line that is longer than the screen is wide


def text():
    L = []
    for i in range(1, N + 1):
        s = 'line %03d: the quick brown fox' % i
        if i == LONG:
            s += ' ' + '0123456789' * 15 + ' END'
        L.append(s)
    return '\n'.join(L) + '\n'


def make_text(t):
    pages, page = [], b''
    for line in t.split('\n')[:-1]:
        b = line.encode('latin1') + b'\r'
        if len(page) + len(b) > 1023:
            pages.append(page)
            page = b''
        page += b
    pages.append(page)
    return bytes(1024) + b''.join(p + bytes(1024 - len(p)) for p in pages)


# every step: the command typed at "shell> " and then keys; after each key group the checks
def main():
    ps = PSystem()
    z = os.environ.get('PSYS_MODE') == 'z80'
    for src in ('examples/shell.c', 'ports/less/less.c', 'ports/more/more.c', 'ports/cat/cat.c'):
        base, code = compile_c(os.path.join(ROOT, src), ps.dir, z)
        ps.put(base + '.CODE', open(code, 'rb').read())
    v = ucsdvol.Volume(ps.src)
    v.write('T.TEXT', make_text(text()), 3)
    v.write('U.TEXT', make_text('second file, line one\nsecond file, line two\n'), 3)
    v.save()
    script = ['WAIT "Command:"', 'TYPE "X"', 'WAIT "Execute what file?"', 'TYPE "#5:SHELL\\r"', 'WAIT "shell> "',
              'TYPE "cd #5\\r"', 'WAIT "shell> "']
    nkeys = len('X#5:SHELL\r' + 'cd #5\r')
    marks = []                          # (key number after the group, what to check)

    def typ(keys, name):
        nonlocal nkeys
        script.append('TYPE "%s"' % keys.replace('\\', '\\\\').replace('"', '\\"').replace('\r', '\\r'))
        nkeys += len(keys)
        marks.append((nkeys, name))

    def cmd(c, name):
        typ(c + '\r', name)
        script.append('WAIT "lines"') if False else None

    LESS = [
        ('less #5:T.TEXT\r', 'start'), ('j', 'j'), ('k', 'k'), (' ', 'space'), ('b', 'b'), ('G', 'G'), ('g', 'g'),
        ('50g', '50g'), ('/line 120\r', 'search'), ('n', 'n'), ('?line 0\r', 'search back'), ('d', 'd'), ('u', 'u'),
        ('\x97', 'wheel down'), ('\x89', 'pgdn'), ('3j', '3j'), ('q', 'quit'),
    ]
    for keys, name in LESS:
        typ(keys, 'less ' + name)
        script.append('WAIT "~~~"') if False else None
    script += []
    # the TYPE lines were appended in order: shell lines need their WAIT between; rebuild the script in order
    script = script[:7]
    marks_ = []
    nkeys = len('X#5:SHELL\r' + 'cd #5\r')

    def add(keys, name, wait=None):
        nonlocal nkeys
        script.append('TYPE "%s"' % ''.join('\\x%02X' % ord(c) if not ' ' <= c <= '~' or c in '\\"' else c for c in keys))
        nkeys += len(keys)
        marks_.append((nkeys, name))
        if wait:
            script.append('WAIT "%s"' % wait)
    for keys, name in LESS:
        add(keys, 'less ' + name, 'shell> ' if name == 'quit' else None)
    for c, name in (('less -N #5:T.TEXT\r', 'N start'), ('G', 'N G'), ('q', 'N quit')):
        add(c, 'less ' + name, 'shell> ' if name == 'N quit' else None)
    for c, name in (('less -S #5:T.TEXT\r', 'S start'), ('120g', 'S 120g'), ('q', 'S quit')):
        add(c, 'less ' + name, 'shell> ' if name == 'S quit' else None)
    for c, name in (('less #5:T.TEXT\r', 'wrap start'), ('120g', 'wrap 120g'), ('q', 'wrap quit')):
        add(c, 'less ' + name, 'shell> ' if name == 'wrap quit' else None)
    for c, name in (('cat #5:T.TEXT | less\r', 'pipe start'), ('G', 'pipe G'), ('q', 'pipe quit')):
        add(c, 'less ' + name, 'shell> ' if name == 'pipe quit' else None)
    for c, name in (('less <#5:T.TEXT\r', 'stdin start'), ('q', 'stdin quit')):
        add(c, 'less ' + name, 'shell> ' if name == 'stdin quit' else None)
    for c, name in (('less #5:U.TEXT #5:T.TEXT\r', 'files start'), (':n', 'files :n'), (':p', 'files :p'), ('q', 'files quit')):
        add(c, 'less ' + name, 'shell> ' if name == 'files quit' else None)
    for c, name in (('more #5:T.TEXT\r', 'more start'), (' ', 'more space'), ('\r', 'more return'), ('d', 'more d'),
                    ('/line 150\r', 'more search'), ('q', 'more quit')):
        add(c, name, 'shell> ' if name == 'more quit' else None)
    for c, name in (('more -10 #5:T.TEXT\r', 'more10 start'), (' ', 'more10 space'), ('q', 'more10 quit')):
        add(c, name, 'shell> ' if name == 'more10 quit' else None)
    script += ['TYPE "bye\\r"', 'WAIT "Command:"']
    scr = os.path.join(ps.dir, 'screens.txt')
    os.environ['VERIFY_SCREEN'] = scr
    ok, tr, info = ps.run_script('\n'.join(script) + '\n', 3000)
    os.environ.pop('VERIFY_SCREEN', None)
    shots = {}
    if os.path.exists(scr):
        for blk in open(scr, encoding='latin1').read().split('== ')[1:]:
            head, _, rest = blk.partition('\n')
            shots[int(head.split()[1])] = rest.split('\n')
    # the keys that follow a "WAIT" in the script are not in marks_: map by name -> screen
    screens = {}
    for n, name in marks_:
        # a typed group can see fewer screens: take the last screen at or before the group's last key
        k = max([x for x in shots if x <= n], default=None)
        screens[name] = shots.get(k)

    def row(name, r):
        s = screens.get(name)
        return s[r] if s and r < len(s) else ''

    def top(name):                      # the number in the first row's "line NNN"
        m = re.search(r'line (\d+)', row(name, 0).replace('  ', ' '))
        return int(m.group(1)) if m else None

    def stat(name):
        return row(name, 44)
    checks = [('the script completed', ok)]
    exp = {'less start': 1, 'less j': 2, 'less k': 1, 'less space': 44, 'less b': 1, 'less G': 157, 'less g': 1,
           'less 50g': 50, 'less search': 120, 'less search back': 99, 'less d': 121, 'less u': 99,
           'less wheel down': 102, 'less pgdn': 145, 'less 3j': 148}
    for name, want in exp.items():
        got = top(name)
        checks.append(('%s: the first row is line %d' % (name, want), got == want))
        if got != want:
            print('  %s: got %r  row0 %r' % (name, got, row(name, 0)))
    checks.append(('less start: the status line', 'lines 1-44' in stat('less start')))
    checks.append(('less G: (END) in the status line', '(END)' in stat('less G') and '157-200/200' in stat('less G')))
    checks.append(('less n: the pattern is not found again', 'Pattern not found' in stat('less n') and top('less n') == 120))
    prompt = lambda rows: any(re.match(r'\S+>( |$)', x) for x in (rows or []))     # the shell's prompt: WORK:> (where names are)
    checks.append(('less quit: back at the shell', prompt(screens.get('less quit'))))
    checks.append(('less -N: line numbers', row('less N start', 0).startswith('     1 line 001')))
    checks.append(('less -N G', row('less N G', 0).startswith('   157 line 157') and '(END)' in stat('less N G')))
    checks.append(('less -S: a long line is cut', 'END' not in ''.join(screens.get('less S 120g') or []) and
                   row('less S 120g', 0).startswith('line 120')))
    ws = ''.join(screens.get('less wrap 120g') or [])
    checks.append(('less: a long line wraps, its end is on the screen', ' END' in ws or 'END' in ws))
    checks.append(('less in a pipe', top('less pipe start') == 1 and top('less pipe G') == 157))
    checks.append(('less <FILE', top('less stdin start') == 1))
    checks.append(('less two files', row('less files start', 0).startswith('second file, line one') and
                   top('less files :n') == 1 and row('less files :p', 0).startswith('second file, line one')))
    checks.append(('more: the first screen and its prompt', any(x.startswith('--More--(') for x in (screens.get('more start') or []))
                   and 'line 044' in row('more start', 43)))
    checks.append(('more: SPACE shows the next screen', 'line 088' in row('more space', 43)))
    checks.append(('more: RETURN one more line', 'line 089' in row('more return', 43)))
    checks.append(('more: d half a screen', 'line 111' in row('more d', 43)))
    checks.append(('more: /text skips to the line', any('line 150' in x for x in (screens.get('more search') or []))))
    checks.append(('more: q back at the shell, the prompt wiped out', prompt(screens.get('more quit'))
                   and not any('--More--' in x for x in (screens.get('more quit') or []))))
    checks.append(('more -10: ten lines', 'line 010' in row('more10 start', 43) or 'line 010' in ''.join(screens.get('more10 start') or [])))
    if os.environ.get('SHOW'):
        for nm in os.environ['SHOW'].split(','):
            print('=== %s' % nm); print('\n'.join(screens.get(nm) or []))
    bad = [n for n, good in checks if not good]
    for n, good in checks:
        print('%-5s %s' % ('ok' if good else 'FAIL', n))
    print('pager test: %s   (work: %s)' % ('FAILED' if bad else 'PASSED', ps.dir))
    return 1 if bad else 0


if __name__ == '__main__':
    sys.exit(main())
