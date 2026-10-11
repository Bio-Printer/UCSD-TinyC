#!/usr/bin/env python3
"""breaktest.py -- the break key (emulator 2.05): BIGGY:'s SYSTEM.MISCINFO has ` as CRTINFO.BREAK
(file offset 84), and a key typed while a program runs without reading keys becomes the run-time
error "User break": a Tiny-C program that loops forever is stopped and the system goes on.
Both modes (PSYS_MODE=native|z80 for one).  Needs build/BIGGY.BLK (tools/mkbiggy.py) and build/run_verify from emulator 2.05 or later."""
import os, shutil, subprocess, sys, tempfile
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from psys import PSystem, BUILD
from tcrun import compile_c

C = r'''#include <stdio.h>
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


def run(mode):
    ps = PSystem()
    src = os.path.join(ps.dir, 'LOOP.C')
    open(src, 'w').write(C)
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


def main():
    modes = [os.environ['PSYS_MODE']] if os.environ.get('PSYS_MODE') else ['native', 'z80']
    ok = all([run(m) for m in modes])
    print('break key test: %s' % ('PASSED' if ok else 'FAILED'))
    return 0 if ok else 1


if __name__ == '__main__':
    sys.exit(main())
