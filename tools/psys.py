#!/usr/bin/env python3
"""psys.py -- drive the UCSD Pascal II.0 P-System (native P-Code mode) on Linux.

Library use:
    from psys import PSystem
    ps = PSystem(workdir)                 # fresh unit #5 volume WORK:
    ps.put('HELLO.TEXT', src_text)        # host text -> UCSD .TEXT
    out = ps.run([('C', 'Compile what text?', 'HELLO\r'), ...])

Command line:
    psys.py pascal FILE.pas        compile and run a Pascal program, print console output
    psys.py script SCRIPT [files]  run a WAIT/TYPE script with files put on unit #5

Disks: #4 = boot disk (Big_Disk.BLK copy), #5 = WORK: (scratch, receives
output), #9 = spare empty volume.
"""
import os, re, sys, subprocess, shutil, tempfile, atexit

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
BUILD = os.path.join(ROOT, 'build')
sys.path.insert(0, HERE)
import ucsdvol


# The shell's prompt is where names without a volume are (examples/shell.c: BIGGY:> , BIGGY:/A/B> ).
# The test scripts wait for "shell> " and read transcripts as if every prompt said that: shell_script
# waits for "> " instead (the runner types only when the system waits for a key, so a "> " in a
# command's output cannot make it type early), shell_transcript turns each prompt -- at a line's
# start, no blank in it -- back into "shell> ".
PROMPT_RE = re.compile(r'(?:(?<=[\r\n])|^)[^\s>]+> ')


def shell_script(text):
    return text.replace('WAIT "shell> "', 'WAIT "> "')


def shell_transcript(tr):
    return PROMPT_RE.sub('shell> ', tr)


def ensure_setup():
    if not os.path.exists(os.path.join(BUILD, 'run_verify')):
        subprocess.check_call([os.path.join(HERE, 'setup.sh')], stdout=subprocess.DEVNULL)


class PSystem:
    def __init__(self, workdir=None, blocks=4000):
        ensure_setup()
        self.dir = workdir or tempfile.mkdtemp(prefix='psys_')
        if not workdir and not os.environ.get('PSYS_KEEP'):
            # 17 MB per run; the suites make thousands (PSYS_KEEP=1 keeps them to look at)
            atexit.register(shutil.rmtree, self.dir, True)
        os.makedirs(self.dir, exist_ok=True)
        self.src = os.path.join(self.dir, 'WORK.BLK')
        ucsdvol.main(['new', self.src, 'WORK', str(blocks)])
        self.spare = os.path.join(self.dir, 'SPARE.BLK')
        ucsdvol.main(['new', self.spare, 'SPARE', str(blocks)])
        self.out = os.path.join(self.dir, 'out')

    def put(self, name, data):
        v = ucsdvol.Volume(self.src)
        if isinstance(data, str):
            v.write(name.upper(), ucsdvol.text_to_ucsd(data), 3)
        else:
            v.write(name.upper(), data, 2 if name.upper().endswith('.CODE') else 5)
        v.save()

    def get(self, name, after=True):
        v = ucsdvol.Volume(os.path.join(self.out, 'VERIFY_SOURCE.BLK') if after else self.src)
        raw, kind = v.read(name)
        return ucsdvol.ucsd_to_text(raw) if kind == 3 else raw

    def env(self):
        e = dict(os.environ)
        if e.get('VERIFY_RECLAIM', '1') == '1':
            e['VERIFY_RECLAIM'] = '1'
        else:
            e.pop('VERIFY_RECLAIM', None)
        return e

    def run_script(self, script_text, timeout=600):
        sp = os.path.join(self.dir, 'run.script')
        open(sp, 'w').write(shell_script(script_text))
        shutil.rmtree(self.out, ignore_errors=True)
        r = subprocess.run([os.path.join(BUILD, 'run_verify'), os.path.join(BUILD, 'data'),
                            self.src, self.spare, sp, os.environ.get('PSYS_MODE', 'native'), self.out, '', str(timeout)],
                           capture_output=True, text=True,
                           env=self.env())
        tr = ''
        tp = os.path.join(self.out, 'transcript.txt')
        if os.path.exists(tp):
            tr = shell_transcript(open(tp, encoding='latin1').read())
        ok = 'VERIFY SCRIPT COMPLETED' in r.stdout
        return ok, tr, r.stdout

    def run(self, steps, timeout=600):
        """steps: list of (wait_text, type_text) pairs; wait None = no wait"""
        lines = ['WAIT "Command:"']
        for w, t in steps:
            if w:
                lines.append('WAIT "%s"' % esc(w))
            if t:
                lines.append('TYPE "%s"' % esc(t))
        return self.run_script('\n'.join(lines) + '\n', timeout)


def esc(s):
    return s.replace('\\', '\\\\').replace('"', '\\"').replace('\r', '\\r').replace('\x1b', '\\e')


def compile_steps(name, codename=None):
    codename = codename or name
    return [(None, 'C'), ('Compile what text?', '#5:%s\r' % name), ('To what codefile?', '#5:%s\r' % codename),
            ('Smallest available space', None), ('Command:', None)]


def exec_steps(name, wait_end='Command:'):
    return [(None, 'X'), ('Execute what file?', '#5:%s\r' % name), (wait_end, None)]


def main(a):
    if not a:
        print(__doc__)
        return 2
    if a[0] == 'pascal':
        ps = PSystem()
        base = os.path.splitext(os.path.basename(a[1]))[0].upper()[:10]
        ps.put(base + '.TEXT', open(a[1]).read())
        ok, tr, info = ps.run(compile_steps(base) + exec_steps(base))
        print(tr)
        if not ok:
            print(info)
        print('workdir:', ps.dir)
        return 0 if ok else 1
    if a[0] == 'script':
        ps = PSystem()
        for f in a[2:]:
            n = os.path.basename(f).upper()
            ps.put(n, open(f).read() if n.endswith('.TEXT') else open(f, 'rb').read())
        ok, tr, info = ps.run_script(open(a[1]).read())
        print(tr)
        print(info)
        return 0 if ok else 1
    print(__doc__)
    return 2


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
