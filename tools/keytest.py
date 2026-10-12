#!/usr/bin/env python3
"""keytest.py -- the emulator's key features, in both modes (PSYS_MODE=native|z80 for one):

  type-ahead (2.04, PX_KEYS_AHEAD): a program sets SYSCOM->expansion[1] = 25609, the emulator answers 25610 and
      keeps the number of keys not yet read in expansion[2]; three keys typed at once: 2 waiting after the first
      is read, 1 after the second.
  break key (2.05): BIGGY:'s SYSTEM.MISCINFO has ` as CRTINFO.BREAK; a key typed while a program runs without
      reading keys becomes the run-time error "User break": a program that loops forever is stopped.
  flow control (2.06): a program prints lines without reading; ^S (CRTINFO.STOP) stops the output, ^Q starts it
      again, ` ends the program.

Needs build/BIGGY.BLK (tools/mkbiggy.py) and build/run_verify from emulator 2.06 or later."""
import os, shutil, subprocess, sys, tempfile
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from psys import PSystem, BUILD
from tcrun import compile_c

C_KB = r'''#include <stdio.h>
#include <conio.h>
#include <psys.h>
int main()
{
    int k0, k1, k2;
    SYSCOM->expansion[1] = PX_KEYS_AHEAD;
    printf("READY\n");
    k0 = SYSCOM->expansion[1];
    getch();
    k1 = SYSCOM->expansion[2];
    getch();
    k2 = SYSCOM->expansion[2];
    printf("RESULT %d %d %d\n", k0, k1, k2);
    getch();
    return 0;
}
'''


def run_typeahead(mode):
    ps = PSystem()
    src = os.path.join(ps.dir, 'KB.C')
    open(src, 'w').write(C_KB)
    base, code = compile_c(src, ps.dir, mode == 'z80')
    ps.put('KB.CODE', open(code, 'rb').read())
    script = ['WAIT "Command:"', 'TYPE "X"', 'WAIT "Execute what file?"', 'TYPE "#5:KB\\r"',
              'WAIT "READY"', 'TYPE "abc"', 'WAIT "RESULT 25610 2 1"']
    sp = os.path.join(ps.dir, 'run.script')
    open(sp, 'w').write('\n'.join(script) + '\n')
    r = subprocess.run([os.path.join(BUILD, 'run_verify'), os.path.join(BUILD, 'data'), ps.src, ps.spare, sp,
                        mode, ps.out, '', '600'], capture_output=True, text=True, env=dict(ps.env(), VERIFY_BURST='1'))
    ok = 'VERIFY SCRIPT COMPLETED' in r.stdout
    print('%-6s keys waiting 2, 1 after each read: %s' % (mode, 'ok' if ok else 'FAILED\n' + r.stdout[-600:]))
    return ok


C_LOOP = r'''#include <stdio.h>
#include <conio.h>
int main()
{
    long i;
    printf("GO\n");
    getch();
    printf("RUN\n");
    for (i = 0;; i++)
        ;
    return 0;
}
'''


def run_break(mode):
    ps = PSystem()
    src = os.path.join(ps.dir, 'LOOP.C')
    open(src, 'w').write(C_LOOP)
    base, code = compile_c(src, ps.dir, mode == 'z80')
    ps.put('LOOP.CODE', open(code, 'rb').read())
    script = ['WAIT "Command:"', 'TYPE "X"', 'WAIT "Execute what file?"', 'TYPE "#5:LOOP\\r"', 'WAIT "GO"',
              'TYPE "x"', 'WAIT "RUN"', 'TYPENOW "`"', 'WAIT "interrupted by user"', 'TYPE " "', 'WAIT "Command:"']
    sp = os.path.join(ps.dir, 'run.script')
    open(sp, 'w').write('\n'.join(script) + '\n')
    data = tempfile.mkdtemp(prefix='breakdata_')       # build/data with BIGGY (as mkbiggy.py made it) as the boot disk
    for f in os.listdir(os.path.join(BUILD, 'data')):
        shutil.copy(os.path.join(BUILD, 'data', f), data)
    shutil.copy(os.path.join(BUILD, 'BIGGY.BLK'), os.path.join(data, 'Big_Disk.BLK'))
    r = subprocess.run([os.path.join(BUILD, 'run_verify'), data, ps.src, ps.spare, sp,
                        mode, ps.out, '', '60'], capture_output=True, text=True, env=dict(ps.env(), VERIFY_BREAKQUIET='0'))
    ok = 'VERIFY SCRIPT COMPLETED' in r.stdout
    print('%-6s ` stops a program that loops forever: %s' % (mode, 'ok' if ok else 'FAILED\n' + r.stdout[-600:]))
    return ok


C_FLOW = r'''#include <stdio.h>
int main()
{
    long i;
    int k;
    for (i = 1;; i++) {
        printf("n %ld\n", i);
        for (k = 0; k < 300; k++)
            ;
    }
    return 0;
}
'''


def run_flow(mode):
    ps = PSystem()
    src = os.path.join(ps.dir, 'FLOW.C')
    open(src, 'w').write(C_FLOW)
    base, code = compile_c(src, ps.dir, mode == 'z80')
    ps.put('FLOW.CODE', open(code, 'rb').read())
    script = ['WAIT "Command:"', 'TYPE "X"', 'WAIT "Execute what file?"', 'TYPE "#5:FLOW\\r"', 'WAIT "n 50"',
              'TYPENOW "\\x13"', 'QUIET "500"', 'TYPENOW "\\x11"', 'WAIT "n 400"',
              'TYPENOW "\\x13"', 'QUIET "300"', 'TYPENOW "\\x11"', 'WAIT "n 800"',
              'TYPENOW "`"', 'WAIT "interrupted by user"', 'TYPE " "', 'WAIT "Command:"']
    sp = os.path.join(ps.dir, 'run.script')
    open(sp, 'w').write('\n'.join(script) + '\n')
    data = tempfile.mkdtemp(prefix='flowdata_')
    for f in os.listdir(os.path.join(BUILD, 'data')):
        shutil.copy(os.path.join(BUILD, 'data', f), data)
    shutil.copy(os.path.join(BUILD, 'BIGGY.BLK'), os.path.join(data, 'Big_Disk.BLK'))
    r = subprocess.run([os.path.join(BUILD, 'run_verify'), data, ps.src, ps.spare, sp, mode, ps.out, '', '120'],
                       capture_output=True, text=True, env=dict(ps.env(), VERIFY_BREAKQUIET='0'))
    ok = 'VERIFY SCRIPT COMPLETED' in r.stdout
    if not ok: print(r.stdout[-1500:], r.stderr[-500:])
    print('%-6s ^S stops the output, ^Q starts it, ` ends the program: %s' % (mode, 'ok' if ok else 'FAILED\n' + r.stdout[-600:]))
    return ok


def main():
    modes = [os.environ['PSYS_MODE']] if os.environ.get('PSYS_MODE') else ['native', 'z80']
    ok = True
    for m in modes:
        for run in (run_typeahead, run_break, run_flow):
            ok = run(m) and ok
    print('key test: %s' % ('PASSED' if ok else 'FAILED'))
    return 0 if ok else 1


if __name__ == '__main__':
    sys.exit(main())
