#!/usr/bin/env python3
"""vipagetest.py [SEEDS] [REF_DIR] -- vi's window into big files
(ports/vi/vipage.c) on Linux: random sessions of 60 commands on a 1500-line
file, run by the paged build (gcc -DENABLE_FEATURE_VI_PAGING=1, a small window:
VI_PAGECAP=6000) of an earlier version (REF_DIR: its ports/vi sources; default
the committed ones, git HEAD) and by the paged build of these sources, which
loads the file lazily: as it is needed (VI_NOIDLE: not while vi waits for a key)
and while vi waits.  All must save the same file (ZZ).  (The paged builds are
not always the same as the build that holds the whole file in memory: a range
that does not fit the window is refused, and some moves of a small window
differ.)
"""
import os, sys, random, subprocess, tempfile, pty, select, time
HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
sys.path.insert(0, HERE)
from mkvolume import VI_MODULES
import vitest

CMDS = ['j', 'k', 'G', 'gg', '{n}G', 'dd', 'x', 'p', 'u', '/line {n}:\r', 'n', 'N', '\x06', '\x02', ':{n}\r', 'ma', "'a",
        'yy', 'J', '5dd', '3j', '10k', 'O', 'ofoo\x1b', 'ixx\x1b', 'dw', 'cwNEW\x1b', '~', '$', '0', '?line {n}:\r',
        ':{n},{m}d\r', '>>', '\x04', '\x15', 'D', 'P', '.', ':s/alpha/ALPHA/\r', 'H', 'L', 'M']


def session(rng):
    keys = []
    for _ in range(60):
        c = rng.choice(CMDS)
        n = rng.randint(1, 1500)
        keys.append(c.replace('{n}', str(n)).replace('{m}', str(min(1500, n + rng.randint(0, 12)))))
    keys += ['\x1b', '\x1b', 'ZZ']
    return keys


def build(exe, src):
    subprocess.check_call(['gcc', '-w', '-DSTANDALONE', '-DENABLE_FEATURE_VI_PAGING=1', '-o', exe] +
                          [os.path.join(src, os.path.basename(m)) for m in VI_MODULES])


def run(exe, keys, env):
    w = tempfile.mkdtemp(prefix='vipage_')
    open(os.path.join(w, 'X.C'), 'w').write(vitest.BIG)
    pid, fd = pty.fork()
    if pid == 0:
        os.chdir(w)
        os.environ.update(env, LINES='45', COLUMNS='132')
        os.execv(exe, [exe, 'X.C'])

    def drain(t):
        end = time.time() + t
        while time.time() < end:
            r, _, _ = select.select([fd], [], [], 0.02)
            if r:
                try:
                    os.read(fd, 65536)
                except OSError:
                    return
    drain(0.3)
    for k in keys:
        os.write(fd, k.encode())
        drain(0.03)
    drain(0.3)
    for _ in range(50):
        if os.waitpid(pid, os.WNOHANG)[0]:
            break
        drain(0.1)
    else:
        os.kill(pid, 9)
        os.waitpid(pid, 0)
    return open(os.path.join(w, 'X.C')).read()


def main():
    seeds = int(sys.argv[1]) if len(sys.argv) > 1 else 12
    w = tempfile.mkdtemp(prefix='vipagebuild_')
    ref = sys.argv[2] if len(sys.argv) > 2 else None
    if not ref:
        ref = os.path.join(w, 'ref')
        os.makedirs(ref)
        for f in os.listdir(os.path.join(ROOT, 'ports', 'vi')):
            if f.endswith(('.c', '.h')):
                open(os.path.join(ref, f), 'wb').write(subprocess.check_output(
                    ['git', '-C', ROOT, 'show', 'HEAD:ports/vi/' + f]))
    old, new = os.path.join(w, 'vi_old'), os.path.join(w, 'vi_new')
    build(old, ref)
    build(new, os.path.join(ROOT, 'ports', 'vi'))
    bad = 0
    for seed in range(seeds):
        keys = session(random.Random(seed))
        want = run(old, keys, {'VI_PAGECAP': '6000'})
        for name, env in (('lazy', {'VI_PAGECAP': '6000', 'VI_NOIDLE': '1'}), ('idle', {'VI_PAGECAP': '6000'})):
            got = run(new, keys, env)
            if got != want:
                bad += 1
                print('seed %d, %s: the saved file differs' % (seed, name))
    print('vi paging test: %d sessions x 2 loads: %s' % (seeds, 'FAILED' if bad else 'PASSED'))
    return 1 if bad else 0


if __name__ == '__main__':
    sys.exit(main())
