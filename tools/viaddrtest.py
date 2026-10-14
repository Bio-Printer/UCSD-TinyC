#!/usr/bin/env python3
"""viaddrtest.py -- vi's : command addresses written as patterns (:/pat/), on the P-System from the shell:

  :/charlie/d                  the line found is deleted (in the window)
  :/nomatch/d, :1,/nomatch/d   "Pattern not found", nothing deleted
  :/delta/,/echo/s/e/E/        a range of two patterns
  :/alpha/d                    above the cursor: as vi, the search goes on from the file's start
  :/x/d on "x one" (C.C)       the search starts on the next line: "x two" goes, the cursor's
                               own line is found only after going around
  :/line 1400:/d               found beyond vi's window (a 1500-line file)
  :/line 20:/d                 above the cursor and the window: found by going around (the chunks
                               before the window)
Uses the mode of PSYS_MODE (native or z80) like the other tools."""
import os, sys
HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
sys.path.insert(0, HERE)
from psys import PSystem
from tcrun import compile_c, compile_modules
from mkvolume import VI_MODULES
import ucsdvol
ps = PSystem()
code = compile_modules([os.path.join(ROOT, m) for m in VI_MODULES], os.path.join(ps.dir, 'VI.CODE'), True)
ps.put('VI.CODE', open(code, 'rb').read())
b, sc = compile_c(os.path.join(ROOT, 'examples', 'shell.c'), ps.dir); ps.put('SHELL.CODE', open(sc, 'rb').read())
A = ['alpha', 'bravo', 'charlie', 'delta', 'echo']
B = ['line %d: some text to fill the line out a bit more' % i for i in range(1, 1501)]
ps.put('C.C', 'x one\ny\nx two\n'); ps.put('A.C', '\n'.join(A) + '\n'); ps.put('B.C', '\n'.join(B) + '\n')
s=['WAIT "Command:"','TYPE "X"','WAIT "Execute what file?"','TYPE "#5:SHELL\\r"','WAIT "shell> "',
   'TYPE "vi #5:A.C\\r"','WAIT "echo"','TYPE ":/charlie/d\\r"','TYPE ":/nomatch/d\\r"','WAIT "Pattern not found"','TYPE ":1,/nomatch/d\\r"','WAIT "Pattern not found"',
   'TYPE ":/delta/,/echo/s/e/E/\\r"','TYPE ":/alpha/d\\r"','TYPE "ZZ"','WAIT "shell> "',
   'TYPE "vi #5:C.C\\r"','WAIT "x two"','TYPE ":/x/d\\r"','TYPE "ZZ"','WAIT "shell> "',
   'TYPE "vi #5:B.C\\r"','WAIT "B.C"','TYPE ":/line 1400:/d\\r"','TYPE ":/line 20:/d\\r"','TYPE "ZZ"','WAIT "shell> "',
   'TYPE "bye\\r"','WAIT "Command:"']
ok,tr,info=ps.run_script('\n'.join(s)+'\n',900)
img=ucsdvol.Volume(ps.out+'/VERIFY_SOURCE.BLK')
a=ucsdvol.ucsd_to_text(img.read('A.C')[0]).split('\n')[:-1]; bb=ucsdvol.ucsd_to_text(img.read('B.C')[0]).split('\n')[:-1]
c=ucsdvol.ucsd_to_text(img.read('C.C')[0]).split('\n')[:-1]
ea=['bravo','dElta','Echo']; eb=B[:19]+B[20:1399]+B[1400:]
checks = [('the script ran to its end (both "Pattern not found" messages seen)', ok),
          ('A.C: charlie deleted, nothing for /nomatch/, the range changed, alpha found around', a == ea),
          ('C.C: :/x/ on "x one" took "x two" (the next line on, the cursor\'s own last)', c == ['x one', 'y']),
          ('B.C: line 1400 deleted, then line 20 (found around, before the window)', bb == eb)]
bad = 0
for what, good in checks:
    print('%-4s  %s' % ('ok' if good else 'BAD', what))
    bad += not good
if bad:
    print(info[-600:])
print('vi address test (%s): %s   (work: %s)' % (os.environ.get('PSYS_MODE', 'native'), 'FAILED' if bad else 'PASSED', ps.dir))
sys.exit(1 if bad else 0)
