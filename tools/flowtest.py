#!/usr/bin/env python3
"""flowtest.py -- ^S and ^Q on the console (emulator 2.06): a program prints lines without ever reading
a key; ^S (CRTINFO.STOP of BIGGY's SYSTEM.MISCINFO) stops the output (it does not grow for half a second),
^Q starts it again (the lines go on), and the break key ` ends the program.  Both modes
(PSYS_MODE=native|z80 for one).  Needs build/BIGGY.BLK (tools/mkbiggy.py) and build/run_verify from
emulator 2.06 or later."""
import os, shutil, subprocess, sys, tempfile
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from psys import PSystem, BUILD
from tcrun import compile_c

C = r'''#include <stdio.h>
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


def run(mode):
    ps = PSystem()
    src = os.path.join(ps.dir, 'FLOW.C')
    open(src, 'w').write(C)
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
    ok = all([run(m) for m in modes])
    print('flow control test: %s' % ('PASSED' if ok else 'FAILED'))
    return 0 if ok else 1


if __name__ == '__main__':
    sys.exit(main())
