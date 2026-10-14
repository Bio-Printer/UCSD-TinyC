#!/usr/bin/env python3
"""subvolxfertest.py -- the emulator's Options > Import File and Export File into and out of a
sub-volume (emulator 2.07: PSystemEngine GetSubVolumes, ImportFileToVolume and ExportFileFromVolume
with a sub-volume's first block), on the P-System:

  build/run_verify_new   run_verify built from UCSD-Pascal_Windows_Emulator
                         (2.07 or later; g++ as tools/setup.sh does it)

The shell makes #5:A and #5:/A/B (MKDIR); then, while it runs, a Windows text file is imported
into #5:/A/B (VERIFY_IMPORT=5/A/B:...), and the shell TYPEs it; DIR #5:A/B/ >#5:A/OUT.TEXT, and
when the script has ended OUT.TEXT is exported from #5:/A (VERIFY_EXPORT=5/A:OUT.TEXT:...): plain
text with CR LF line ends.  GetSubVolumes lists WORK:, A and A/B.
PSYS_MODE as the other tools (native default)."""
import os, re, sys, subprocess, tempfile
HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
sys.path.insert(0, HERE)
from psys import PSystem, BUILD, shell_script, shell_transcript
from tcrun import compile_c

TEXT = 'Imported from Windows\r\n  into a sub-volume,\r\n\tthird line\r\n'


def main():
    z = os.environ.get('PSYS_MODE') == 'z80'
    new = os.path.join(BUILD, 'run_verify_new')
    ps = PSystem()
    b, code = compile_c(os.path.join(ROOT, 'examples/shell.c'), ps.dir)
    ps.put('SHELL.CODE', open(code, 'rb').read())
    host = tempfile.mkdtemp(prefix='svxfer_')
    src = os.path.join(host, 'hello.text')
    open(src, 'wb').write(TEXT.encode())
    out = os.path.join(host, 'OUT.TEXT')
    s = ['WAIT "Command:"', 'TYPE "X"', 'WAIT "Execute what file?"', 'TYPE "#5:SHELL\\r"', 'WAIT "shell> "',
         'TYPE "mkdir #5:A 200\\r"', 'WAIT "a sub-volume"', 'WAIT "shell> "',
         'TYPE "mkdir #5:/A/B 60\\r"', 'WAIT "a sub-volume"', 'WAIT "shell> "']
    step = len(s)                       # the import: here, between two commands
    s += ['TYPE "type #5:A/B/HELLO.TEXT\\r"', 'WAIT "third line"', 'WAIT "shell> "',
          'TYPE "dir #5:A/B/ >#5:A/OUT.TEXT\\r"', 'WAIT "shell> "',
          'TYPE "bye\\r"', 'WAIT "Command:"']
    sp = os.path.join(ps.dir, 'run.script')
    open(sp, 'w').write(shell_script('\n'.join(s) + '\n'))
    env = dict(ps.env(), VERIFY_IMPORT='5/A/B:' + src, VERIFY_IMPORT_STEP=str(step),
               VERIFY_EXPORT='5/A:OUT.TEXT:' + out, VERIFY_SUBVOLS='1')
    r = subprocess.run([new, os.path.join(BUILD, 'data'), ps.src, ps.spare, sp, os.environ.get('PSYS_MODE', 'native'),
                        ps.out, '', '2400' if z else '400'], capture_output=True, text=True, env=env)
    tr = shell_transcript(open(os.path.join(ps.out, 'transcript.txt'), encoding='latin1').read()).replace('\r', '\n') \
        if os.path.exists(os.path.join(ps.out, 'transcript.txt')) else ''
    subs = re.findall(r'subvolume #5:(\S*) at block (\d+)', r.stderr)
    got = open(out, 'rb').read().decode('latin1') if os.path.exists(out) else ''
    typed = tr[tr.find('type #5:A/B/HELLO.TEXT'):]
    checks = [('the script ran to its end', 'VERIFY SCRIPT COMPLETED' in r.stdout),
              ('import into #5:/A/B: OK', 'import 5/A/B:%s: OK' % src in r.stderr),
              ('TYPE #5:A/B/HELLO.TEXT: the Windows lines (the tab and indent as blanks)',
               'Imported from Windows' in typed and '  into a sub-volume,' in typed and 'third line' in typed),
              ('GetSubVolumes: WORK:, A, A/B (%s)' % ' '.join(p for p, _ in subs), [p for p, _ in subs] == ['/', '/A', '/A/B']),
              ('export of #5:/A OUT.TEXT: OK', 'export 5/A:OUT.TEXT:%s: OK' % out in r.stderr),
              ('OUT.TEXT on Windows: the DIR listing, CR LF, no header page',
               got.startswith('WORK:/A/B/ (#5)\r\n') and 'HELLO.TEXT' in got and '\0' not in got)]
    bad = 0
    for what, good in checks:
        print('%-4s  %s' % ('ok' if good else 'BAD', what))
        bad += not good
    if bad:
        print(tr[-2000:])
        print(r.stdout[-800:], r.stderr[-800:])
    print('sub-volume import/export test (%s): %s   (work: %s)' % ('z80' if z else 'native', 'FAILED' if bad else 'PASSED', ps.dir))
    return 1 if bad else 0


if __name__ == '__main__':
    sys.exit(main())
