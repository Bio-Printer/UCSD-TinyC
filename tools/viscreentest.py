#!/usr/bin/env python3
"""viscreentest.py [REF_DIR] -- the screen vi shows after every key, compared
with a reference vi (REF_DIR: the ports/vi sources of an earlier version;
default: the committed ones, git HEAD) on Linux (gcc -DSTANDALONE: the
checksum-per-row screen and refresh's skipping of an unchanged screen, as on
the P-System; in a pseudo-terminal, a small terminal emulator reads what
vi writes).  Edits, motions, scrolling, ^L, :s, undo, a long line with
horizontal scrolling: every screen after every key must be the same.
"""
import os, sys, re, pty, time, select, subprocess, tempfile
HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
MODS = ('vimain', 'viscreen', 'vitext', 'vicolon', 'vicmd', 'vipage', 'vipage2', 'viucsd')
ROWS, COLS = 24, 80

TEXT = ''.join('line %d:\t%s\n' % (i, 'abcabc xyz ' * (1 + i % 9)) for i in range(1, 120)) + 'x' * 300 + '\n' + 'a   a\n' * 5
KEYS = ['j'] * 5 + ['l'] * 10 + ['\x06', '\x06', '\x02', 'G', 'gg', '30G', 'x', 'u', 'dd', 'p', 'u', 'k', 'k', 'h',
        ':s/a/b/g\r', 'u', 'ia', '\x1b', 'j', 'ZZ'[:0], '$', 'j', 'j', '0', '/line 9\r', 'n', 'N', '\x0c', 'w', 'w',
        '~', '~', 'r!', 'J', 'yy', '3p', 'dw', '.', 'G', 'k', 'k', '$', 'l', 'l', '5l', 'x', '120G', '$', 'j', 'j', '0',
        ':set ts=4\r', 'j', ':set ts=8\r', '2G', 'ma', '90G', "'a", 'D', 'u', 'O', 'new\x1b', 'u', 'U', ':1,3d\r', 'u',
        'cwXYZ\x1b', 'j', '.', 'j', '.', '100G', 'k', 'k', 'k', 'k', 'k', 'k', 'k', 'k', 'k', 'k', 'k', 'k', 'k',
        '\x05', '\x05', '\x19', '\x04', '\x15', 'H', 'L', 'M'] + ['j'] * 40 + ['k'] * 45 + ['\x05'] * 6 + ['\x19'] * 7 + ['G'] + ['k'] * 3 + ['\x06', '\x02', '3\x04', '3\x15', 'ihello world', ' ' * 12, '\x7f' * 5, '\x1b', 'j', 'A', 'abc', 'd', '\x7f\x7f', '\x1b', 'xxxxx', 'u', 'wcwfoo', '\x1b', 'kk', 'ix', 'y', 'z', '\r', 'q', '\x1b', 'j' * 30, 'k' * 33, 'j' * 7 + 'k' * 3 + 'j' * 12, ':q!\r']


def build(srcdir, exe):
    subprocess.check_call(['gcc', '-w', '-DSTANDALONE', '-o', exe] +
                          [os.path.join(srcdir, m + '.c') for m in MODS if os.path.exists(os.path.join(srcdir, m + '.c'))])


class Term:
    def __init__(self):
        self.g = [[' '] * COLS for _ in range(ROWS)]
        self.r = self.c = 0
        self.buf = ''
        self.top, self.bot = 0, ROWS - 1
        self.scrolls = 0

    def feed(self, s):
        s = self.buf + s
        self.buf = ''
        i = 0
        while i < len(s):
            ch = s[i]
            if ch == '\x1b':
                if i + 1 >= len(s):
                    self.buf = s[i:]
                    break
                if s[i + 1] == '[':
                    j = i + 2
                    while j < len(s) and not ('@' <= s[j] <= '~'):
                        j += 1
                    if j >= len(s):
                        self.buf = s[i:]
                        break
                    self.csi(s[i + 2:j], s[j])
                    i = j + 1
                    continue
                i += 2
                continue
            if ch == '\r':
                self.c = 0
            elif ch == '\n':
                if self.r == ROWS - 1:
                    self.g = self.g[1:] + [[' '] * COLS]
                    self.scrolls += 1
                else:
                    self.r += 1
            elif ch == '\b':
                self.c = max(0, self.c - 1)
            elif ch >= ' ':
                if self.c < COLS:
                    self.g[self.r][self.c] = ch
                self.c = min(COLS, self.c + 1)
            i += 1

    def csi(self, args, cmd):
        n = [int(a) if a.isdigit() else 0 for a in args.split(';')] if args and args[0] != '?' else [0]
        if cmd == 'H':
            self.r = min(ROWS - 1, max(0, (n[0] or 1) - 1))
            self.c = min(COLS - 1, max(0, ((n[1] if len(n) > 1 else 0) or 1) - 1))
        elif cmd == 'r':
            t = n[0] or 1
            b = (n[1] if len(n) > 1 else 0) or ROWS
            self.top, self.bot = (t - 1, b - 1) if t < b else (0, ROWS - 1)
        elif cmd in 'ST':
            k = min(n[0] or 1, self.bot - self.top + 1)
            reg = self.g[self.top:self.bot + 1]
            blank = [[' '] * COLS for _ in range(k)]
            self.g[self.top:self.bot + 1] = reg[k:] + blank if cmd == 'S' else blank + reg[:len(reg) - k]
            self.scrolls += 1
        elif cmd == 'A':
            self.r = max(0, self.r - (n[0] or 1))
        elif cmd == 'K':
            for x in range(self.c, COLS):
                self.g[self.r][x] = ' '
        elif cmd == 'J':
            for x in range(self.c, COLS):
                self.g[self.r][x] = ' '
            for y in range(self.r + 1, ROWS):
                self.g[y] = [' '] * COLS

    def snap(self):
        return ['' .join(r).rstrip() for r in self.g], (self.r, self.c)


def session(exe, work, extra=None):
    x = os.path.join(work, 'X.C')
    open(x, 'w').write(TEXT)
    pid, fd = pty.fork()
    if pid == 0:
        os.chdir(work)
        os.environ['LINES'] = str(ROWS)
        os.environ['COLUMNS'] = str(COLS)
        os.environ['VI_PAGECAP'] = '4500'
        os.environ.update(extra or {})
        os.execv(exe, [exe, 'X.C'])
    t = Term()

    def drain(quiet):
        last = time.time()
        while time.time() - last < quiet:
            r, _, _ = select.select([fd], [], [], 0.02)
            if r:
                try:
                    d = os.read(fd, 65536)
                except OSError:
                    return False
                if not d:
                    return False
                t.feed(d.decode('latin1'))
                last = time.time()
        return True
    drain(0.4)
    def snap():
        s = t.snap()
        s[0][ROWS - 1] = re.sub(r' +w\d+\+\d+ b\d+/\d+ a\d+/\d+( L)?', '', s[0][ROWS - 1])  # (paging: where the window is)
        return s
    shots = [snap()]
    shots[0][0][ROWS - 1] = re.sub(r'\d+/\d+ \d+%', 'N/N P%', shots[0][0][ROWS - 1])     # (paging: the lines loaded so far)
    for k in KEYS:
        if not k:
            continue
        os.write(fd, k.encode())
        if not drain(0.08):
            break
        shots.append(snap())
    try:
        os.waitpid(pid, 0)
    except ChildProcessError:
        pass
    shots.append(t.scrolls)
    return shots


def main():
    ref = sys.argv[1] if len(sys.argv) > 1 else None
    work = tempfile.mkdtemp(prefix='viscreen_')
    if not ref:
        ref = os.path.join(work, 'ref')
        os.makedirs(ref)
        for f in os.listdir(os.path.join(ROOT, 'ports', 'vi')):
            if f.endswith(('.c', '.h')):
                try:
                    open(os.path.join(ref, f), 'wb').write(subprocess.check_output(
                        ['git', '-C', ROOT, 'show', 'HEAD:ports/vi/' + f], stderr=subprocess.DEVNULL))
                except subprocess.CalledProcessError:
                    pass                        # (a file the committed version does not have)
    ok = True
    for paged in (False, True):         # a window that holds the whole file; a 4500 byte window
        out = {}
        new = os.path.join(ROOT, 'ports', 'vi')
        # the reference (the committed vi) and the new vi, both with the console's scroll codes
        for name, src, extra in (('ref', ref, None), ('new', new, None)):
            d = os.path.join(work, 'run_%s%d' % (name, paged))
            os.makedirs(d)
            build(src, os.path.join(d, 'vi'))
            out[name] = session(os.path.join(d, 'vi'), d, extra if paged else {'VI_PAGECAP': '30000'})
        a = out['ref']
        a.pop()
        for name in ('new',):
            b = out[name]
            nscroll = b.pop()
            bad = [i for i in range(min(len(a), len(b))) if a[i] != b[i]]
            print('%s, %s: %d screens compared; %d differ; reference %d screens, new %d; the new vi scrolled %d times' %
                  ('small window' if paged else 'whole file', name, min(len(a), len(b)), len(bad), len(a), len(b), nscroll))
            for i in bad[:3]:
                sent = [k for k in KEYS if k]                # (the screens are those after the keys sent)
                print('after key %d (%r):' % (i, sent[i - 1] if i else 'start'))
                for y in range(ROWS):
                    if a[i][0][y] != b[i][0][y]:
                        print('  row %2d ref: %r\n         new: %r' % (y, a[i][0][y], b[i][0][y]))
            ok = ok and not bad and len(a) == len(b) and len(a) > 50 and nscroll > 5
    print('viscreentest: %s' % ('PASSED' if ok else 'FAILED'))
    return 0 if ok else 1


if __name__ == '__main__':
    sys.exit(main())
