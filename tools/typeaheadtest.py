#!/usr/bin/env python3
"""typeaheadtest.py -- the keys-waiting count of emulator 2.04 (PX_KEYS_AHEAD): a program
sets SYSCOM->expansion[1] = 25609, the emulator answers 25610 and keeps the number of keys
not yet read in expansion[2].  Three keys are typed at once (VERIFY_BURST=1): after the
first is read 2 are waiting, after the second 1.  Both modes (PSYS_MODE=native|z80 for
one).  Needs build/run_verify built from UCSD-Pascal_Windows_Emulator 2.04 or later."""
import os, subprocess, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from psys import PSystem, BUILD
from tcrun import compile_c

C = r'''#include <stdio.h>
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


def run(mode):
    ps = PSystem()
    src = os.path.join(ps.dir, 'KB.C')
    open(src, 'w').write(C)
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


def main():
    modes = [os.environ['PSYS_MODE']] if os.environ.get('PSYS_MODE') else ['native', 'z80']
    ok = all([run(m) for m in modes])
    print('type-ahead test: %s' % ('PASSED' if ok else 'FAILED'))
    return 0 if ok else 1


if __name__ == '__main__':
    sys.exit(main())
