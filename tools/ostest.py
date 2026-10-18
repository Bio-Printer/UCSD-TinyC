#!/usr/bin/env python3
"""ostest.py -- #pragma system (an operating system, SYSTEM.PASCAL's layout):
a small one is compiled, checked and booted in place of SYSTEM.PASCAL.

  the code file: main is segment 0 procedure 1, lex level -1, its data the
    statics (words 1..), its other procedures lex level 0 in the order of the
    file; segment PARTTWO is segment 1; the unused segment names are blank;
  the boot: word 1 is the boot's ^SYSCOM (SYSUNIT 4), the statics are zero and
    reached from every procedure (LOD/STR 1,n), a function's result comes back
    from a Pascal-style call (two words below the parameters), another
    segment's procedure runs (CXP), a value is dropped (FJP 0), and main's XIT
    stops the P-machine.
The full operating system in C is UCSD-SYSTEM-C (its own tests).  Uses the
mode of PSYS_MODE (native or z80).
"""
import os, struct, subprocess, sys, tempfile, shutil
HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
sys.path.insert(0, HERE)
import ucsdvol
from psys import ensure_setup, BUILD
from tcrun import build_tc, TC, INC

OS = r'''#pragma system
static int *syscom;                     /* word 1: the boot's */
static int count;
static unsigned char line[60];

void say(unsigned char *s, int n)
{
    __cspv(6, 1, s, 0, n, 0, 0);        /* UNITWRITE(1, s, n) */
}

int twice(int a, int b)
{
    count = count + 1;
    return a + a + b - b;
}

void number(int v)
{
    int n;
    n = 0;
    line[n++] = '0' + __dvi(v, 100);
    line[n++] = '0' + __mdi(__dvi(v, 10), 10);
    line[n++] = '0' + __mdi(v, 10);
    line[n++] = ' ';
    say(line, n);
}

#pragma segment PARTTWO
void parttwo(int v)
{
    number(v);
    twice(v, 1);                        /* (the result dropped) */
}
#pragma segment MAIN

void main(void)
{
    unsigned char *t;
    int i;
    t = "\r\nOS mode: ";
    for (i = 0; t[i]; i++)
        line[i] = t[i];
    say(line, i);
    number(syscom[2]);                  /* SYSUNIT */
    number(count);
    number(twice(21, 5));
    parttwo(123);
    number(count);
    say("halt\r", 5);
}
'''


def main():
    ensure_setup()
    build_tc()
    d = tempfile.mkdtemp(prefix='ostest_')
    open(os.path.join(d, 'os.c'), 'w').write(OS)
    env = dict(os.environ, TINYC_MAP='1')
    r = subprocess.run([TC, '-I', INC, 'os.c', '-o', 'SYSTEM.PASCAL'], cwd=d, capture_output=True, text=True, env=env)
    checks = [('compiled and linked', r.returncode == 0)]
    code = open(os.path.join(d, 'SYSTEM.PASCAL'), 'rb').read() if r.returncode == 0 else bytes(1024)
    procs = {}
    for line in r.stderr.split('\n'):
        f = line.split()
        if len(f) == 6 and f[0] == 'MAP':
            procs[f[5]] = (int(f[1]), int(f[2]))

    def proc(seg, p):
        blk, length = struct.unpack_from('<HH', code, 4 * seg)
        s = code[blk * 512: blk * 512 + length]
        ptr = len(s) - 2 - 2 * p
        jtab = ptr - struct.unpack_from('<H', s, ptr)[0]
        datasz, parmsz = struct.unpack_from('<HH', s, jtab - 8)
        return s[jtab + 1], parmsz, datasz
    checks += [('main is 0.1, say 0.2, twice 0.3, number 0.4, parttwo 1.1 (%s)' % procs,
                procs == {'main': (0, 1), 'say': (0, 2), 'twice': (0, 3), 'number': (0, 4), 'parttwo': (1, 1)}),
               ('segment names PASCALSY, PARTTWO, the rest blank',
                code[64:80] == b'PASCALSYPARTTWO ' and code[80:192] == b' ' * 112)]
    if r.returncode == 0:
        checks += [('main: lex level -1, no parameters, its data the 32 words of statics, then its own 2',
                    proc(0, 1) == (255, 0, 68)),
                   ('twice: lex level 0, 2 parameters and 2 result words, no locals', proc(0, 3) == (0, 8, 0)),
                   ('number: one local', proc(0, 4) == (0, 2, 2))]
    data = os.path.join(d, 'data')
    os.makedirs(data)
    shutil.copy(os.path.join(BUILD, 'data', 'pascal.bin'), data)
    shutil.copy(os.path.join(BUILD, 'data', 'Big_Disk.BLK'), data)
    v = ucsdvol.Volume(os.path.join(data, 'Big_Disk.BLK'))
    v.write('SYSTEM.PASCAL', code, 2)
    v.save()
    for n in ('W', 'S'):
        ucsdvol.main(['new', os.path.join(d, n + '.BLK'), n, '100'])
    open(os.path.join(d, 'run.script'), 'w').write('WAIT "halt"\n')
    mode = os.environ.get('PSYS_MODE', 'native')
    r = subprocess.run([os.path.join(BUILD, 'run_verify'), data, os.path.join(d, 'W.BLK'), os.path.join(d, 'S.BLK'),
                        os.path.join(d, 'run.script'), mode, os.path.join(d, 'out'), '', '60'], capture_output=True, text=True)
    tr = open(os.path.join(d, 'out', 'transcript.txt'), encoding='latin1').read() if os.path.exists(os.path.join(d, 'out', 'transcript.txt')) else ''
    checks += [('booted: SYSUNIT 4, statics zero, twice(21, 5) = 42 counted, segment 1 ran, a dropped result counted',
                'OS mode: 004 000 042 123 002 halt' in tr)]
    bad = 0
    for what, good in checks:
        print('%-4s  %s' % ('ok' if good else 'BAD', what))
        bad += not good
    if bad:
        print(tr[-600:])
    print('OS mode test (%s): %s   (work: %s)' % (mode, 'FAILED' if bad else 'PASSED', d))
    return 1 if bad else 0


if __name__ == '__main__':
    sys.exit(main())
