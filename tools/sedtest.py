#!/usr/bin/env python3
"""sedtest.py -- SED (ports/sed/sed.c + sedre.c, TOOLS:SED.CODE) from the shell on the P-System against the host's
sed (GNU) on the same text: addresses, ranges, every command, s with flags and the regular expression
dialects, y, a i c, branches, hold space, -n -e -f -E -s -i, several files, the exit status.  The output of
each is redirected into a file and read back from the disk image.

Uses the mode of PSYS_MODE (native or z80) like the other tools."""
import os, sys, subprocess, re
HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
sys.path.insert(0, HERE)
from psys import PSystem
import ucsdvol
from tcrun import compile_modules
from mkvolume import TOOLS

A = ('Hello World\nthe quick brown fox\njumps over the lazy dog\n123 abc 456\nfoo bar foo\n   indented line\n'
     'x=1; y=22; z=333\nempty next\n\nlast line here\n')
B = 'second file one\nsecond file two\nfoo again\n'
S1 = 's/o/0/\n/fox/ {\n  s/quick/slow/\n  a\\\nafter fox\n}\n$ i\\\nbefore last\n'      # a script file: spaces, a\ lines
S2 = 's/(a|b)+/X/g\n'
S3 = '/^(the|jumps)/d\ns/(o|a)(.)/<\\2\\1>/\n'
S4 = 's/$/0123456789012345678901234567890123456789012345678901234567890123456789012/\nl\n'
S5 = ':a\ns/^\\(.\\{1,20\\}\\)$/ \\1/\nta\n'
FILES = {'A': A, 'B': B, 'S1': S1, 'S2': S2, 'S3': S3, 'S4': S4, 'S5': S5}

# (the arguments, with #5:A.TEXT for the input where the files are named in the arguments)
FA = '#5:A.TEXT'
FB = '#5:B.TEXT'
CASES = [
    's/o/0/', 's/o/0/g', 's/o/0/2', 's/o/0/2g', '-n s/o/0/p', '-n /fox/p', '/fox/d', '2d', '2,4d', '$d', '1!d',
    '/fox/,/dog/d', '-n 3,5p', '-n $=', '=', '2q', '2Q', 'y/abc/xyz/', 's/[0-9]\\+/\\<&\\>/g', 's/^/\\>/', 's/$/\\</',
    's/\\w\\+/[&]/', 's/\\bfoo\\b/X/g', 's/\\([a-z]*\\)\\s\\([a-z]*\\)/\\2-\\1/', '-n /[0-9]/p', '/^$/d', 's/\\s\\+$//',
    's/o*/X/g', 's/x*/-/g', 'N;P;D', '$!N;s/\\n/+/', '1!G;h;$!d', 'G', 'n;d', '2{p;p}', '-n /x=/{s/;/\\n/g;p}',
    '/fox/{s/quick/slow/;s/brown/red/}', '/fox/!d', '2aHELLO', '2iINS', '2cCHG', '2,3cCHG', '/fox/aAFTER', '$aEND',
    'l', '-n l', 's/.*/\\U&/', 's/\\(.\\)\\(.*\\)/\\u\\1\\L\\2/', '/FOX/Id', 's/FOX/dog/I', 's/\\(o\\)\\1/DOUBLE/',
    's/o/0/;ta;s/$/!/;:a', 's/o/0/;Ta;s/$/!/;:a', '-n /a/,+1p', '2~2d', '0,/o/s//0/', '-n 2,/o/p', '/fox/,+1d', '3,~4d',
    '-E -f #5:S2.TEXT', '-E s/o{2}/OO/', '-E s/([a-z]+)\\ ([a-z]+)/\\2/', 's/.*/"&"/', 's/a\\|e/_/g', '-n /^.\\{5\\}$/p',
    's/[[:digit:]]\\+/N/g', 's/[^a-z ]//g', 's/[aeiou]/\\n/2', 'x;G', 'h;G;G', '$!d', '1d;$d', '/^l/,/^e/s/e/E/',
    '-n /fox/{=;p}', 'y/ /_/', 's/\\(ab\\)*c/X/', 's/b\\?c/Y/', 's/./&&/3', '-n s/o/0/gp', '-n /o/{/e/p}', '/jumps/r#5:B.TEXT',
    '-e s/a/b/ -e s/o/0/', '-s -n $p #5:B.TEXT', '-n $p #5:B.TEXT', '-f #5:S1.TEXT', '-n -f #5:S1.TEXT',
    '-s 1d #5:B.TEXT', 's#o#0#g', 's,o,0,', 's/o/\\&/', 's/o/[\\&]/g', 's/\\(l\\)\\1/L\\1L/', '/\\(o\\).*\\1/d',
    'N;N;s/\\n/,/g', ':a;N;$!ba;s/\\n/,/g', '$!N;/^\\(.*\\)\\n\\1$/!P;D', 's/[^,]*,//2', 's/a*/x/2', '-E s/(.)(.)/\\2\\1/',
    '-E s/o+/0/g', '-E s/^(.)(.*)$/\\2\\1/', '-E s/[a-z]{3,}/W/g', '-E -f #5:S3.TEXT', '-f #5:S4.TEXT', '-n -f #5:S4.TEXT',
    '-f #5:S5.TEXT', '2,4!d', '2,3{s/o/0/;s/e/3/}', '/x=/s/[0-9]*/#/g', '/^$/!s/^/-/', '$!N;$!D', '-n /fox/,/dog/{p}',
    '1,3y/abc/ABC/', '/o/s/o/0/3', 's/\\(.\\)\\(.\\)\\(.\\)/\\3\\2\\1/g', '-n $!p', '2h;3x;4G', 'k', 's/a', 'y/ab/x/', '/x/,', '{p', 'bnolabel', 's/\\(a/x/',
]


def make_text(text):
    pages, page = [], b''
    for line in text.split('\n')[:-1]:
        b = line.encode('latin1') + b'\r'
        if len(page) + len(b) > 1023:
            pages.append(page)
            page = b''
        page += b
    pages.append(page)
    return bytes(1024) + b''.join(p + bytes(1024 - len(p)) for p in pages)


def host(args):
    """GNU sed's output for the same arguments: the files are the ones in the arguments, or A.TEXT's text"""
    w = args.split(' ') if args else []
    tmp = '/tmp/_sedtest'
    os.makedirs(tmp, exist_ok=True)
    for k, v in FILES.items():
        open(os.path.join(tmp, k + '.TEXT'), 'w').write(v)
    w = [x.replace('#5:', tmp + '/') for x in w]
    if not any(x.startswith(tmp) and not x.endswith(('S1.TEXT', 'S2.TEXT', 'S3.TEXT', 'S4.TEXT', 'S5.TEXT')) for x in w):
        w.append(tmp + '/A.TEXT')
    if '/jumps/r' + FB.replace('#5:', '') in ' '.join(w):
        pass
    r = subprocess.run(['sed'] + w, capture_output=True, text=True, cwd=tmp)
    return r.stdout.replace(tmp + '/', ''), r.returncode


def session(cases, z, start):
    """one P-System session (a volume holds 77 files): the outputs of the cases, read back"""
    ps = PSystem()
    sed = os.path.join(ROOT, 'build', 'SED_T.CODE')
    code = compile_modules([os.path.join(ROOT, 'ports/sed/sed.c'), os.path.join(ROOT, 'ports/sed/sedre.c')], sed, z)
    ps.put('SED.CODE', open(code, 'rb').read())
    from tcrun import compile_c
    base, shell = compile_c(os.path.join(ROOT, 'examples', 'shell.c'), ps.dir, z)
    ps.put('SHELL.CODE', open(shell, 'rb').read())
    v = ucsdvol.Volume(ps.src)
    for k, t in FILES.items():
        v.write(k + '.TEXT', make_text(t), 3)
    v.save()
    script = ['WAIT "Command:"', 'TYPE "X"', 'WAIT "Execute what file?"', 'TYPE "#5:SHELL\\r"', 'WAIT "shell> "',
              'TYPE "cd #5\\r"', 'WAIT "shell> "']
    for i, c in enumerate(cases):
        a = c.replace('\\', '\\\\').replace('"', '\\"')
        if not any(x.startswith('#5:') and not x.endswith(('S1.TEXT', 'S2.TEXT', 'S3.TEXT', 'S4.TEXT', 'S5.TEXT')) for x in c.split(' ')):
            a += ' ' + FA
        script += ['TYPE "sed %s >#5:O%d.TEXT\\r"' % (a, i), 'WAIT "shell> "']
    script += ['TYPE "bye\\r"', 'WAIT "Command:"']
    ok, tr, info = ps.run_script('\n'.join(script) + '\n', 3000)
    img = ucsdvol.Volume(os.path.join(ps.out, 'VERIFY_SOURCE.BLK'))
    outs = []
    for i in range(len(cases)):
        name = 'O%d.TEXT' % i
        outs.append(ucsdvol.ucsd_to_text(img.read(name)[0]) if img.find(name) else '')
    return ok, outs, ps.dir


def extras(z):
    """-i with a backup, w FILE, the exit status of q and of a missing file"""
    ps = PSystem()
    sed = os.path.join(ROOT, 'build', 'SED_T.CODE')
    code = compile_modules([os.path.join(ROOT, 'ports/sed/sed.c'), os.path.join(ROOT, 'ports/sed/sedre.c')], sed, z)
    ps.put('SED.CODE', open(code, 'rb').read())
    from tcrun import compile_c
    base, shell = compile_c(os.path.join(ROOT, 'examples', 'shell.c'), ps.dir, z)
    ps.put('SHELL.CODE', open(shell, 'rb').read())
    v = ucsdvol.Volume(ps.src)
    for k, t in FILES.items():
        v.write(k + '.TEXT', make_text(t), 3)
    v.write('C.TEXT', make_text(A), 3)
    v.save()
    cmds = ['sed -i.BAK s/o/0/ #5:C.TEXT', 'sed -n /fox/w#5:W.TEXT #5:A.TEXT', 'sed 5q7 #5:A.TEXT', 'sed -n p #5:NOSUCH.TEXT',
            'sed -n 2p #5:A.TEXT #5:NOSUCH.TEXT', 'sed -n $= #5:A.TEXT #5:B.TEXT']
    script = ['WAIT "Command:"', 'TYPE "X"', 'WAIT "Execute what file?"', 'TYPE "#5:SHELL\\r"', 'WAIT "shell> "',
              'TYPE "cd #5\\r"', 'WAIT "shell> "']
    for c in cmds:
        script += ['TYPE "%s\\r"' % c, 'WAIT "shell> "']
    script += ['TYPE "bye\\r"', 'WAIT "Command:"']
    ok, tr, info = ps.run_script('\n'.join(script) + '\n', 3000)
    tr = tr.replace('\r', '\n')
    img = ucsdvol.Volume(os.path.join(ps.out, 'VERIFY_SOURCE.BLK'))

    def text(n):
        return ucsdvol.ucsd_to_text(img.read(n)[0]) if img.find(n) else None

    def part(cmd):
        i = tr.find('shell> ' + cmd + '\n')
        return '' if i < 0 else tr[i:tr.find('shell> ', i + 7)]
    return [('a session completed', ok),
            ('sed -i.BAK: the file edited', text('C.TEXT') == host('s/o/0/')[0]),
            ('sed -i.BAK: the backup is the original', text('C.BAK.TEXT') == A),
            ('w FILE', text('W.TEXT') == 'the quick brown fox\n'),
            ('5q7: the exit status', '[exit status 7]' in part(cmds[2])),
            ('a missing file: said, status 2', 'NOSUCH.TEXT: cannot open it' in part(cmds[3]) and '[exit status 2]' in part(cmds[3])),
            ('a missing file after a good one: the good one is still read', 'the quick brown fox' in part(cmds[4])),
            ('two files: $= counts them as one', part(cmds[5]).count('\n13\n') == 1 or '\n13' in part(cmds[5]))]


def main():
    z = os.environ.get('PSYS_MODE') == 'z80'
    checks = []
    for start in range(0, len(CASES), 40):
        chunk = CASES[start:start + 40]
        ok, outs, work = session(chunk, z, start)
        checks.append(('a session of %d commands completed' % len(chunk), ok))
        for c, got in zip(chunk, outs):
            want, rc = host(c)
            good = got.rstrip('\n') == want.rstrip('\n')
            checks.append(('sed %s' % c, good))
            if not good:
                print('--- sed %s\n got:  %r\n want: %r' % (c, got[:300], want[:300]))
    checks += extras(z)
    bad = [n for n, good in checks if not good]
    for n, good in checks:
        print('%-5s %s' % ('ok' if good else 'FAIL', n))
    print('sed test: %s' % ('FAILED' if bad else 'PASSED'))
    return 1 if bad else 0


if __name__ == '__main__':
    sys.exit(main())
