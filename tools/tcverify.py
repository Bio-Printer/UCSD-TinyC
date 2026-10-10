#!/usr/bin/env python3
"""tcverify.py [z80|native] -- run the Tiny-C Verify pack (verify/TCVERIFY.SCRIPT
on TCVERIFY.BLK as unit #5) exactly as Verify P-System would, and report.
Build the pack first with mkverify.py.  Native mode runs with the Z80
interpreter's memory reclaimed (Tiny-C needs that memory); VERIFY_RECLAIM=0
turns it off.  Z80 mode runs every test: the emulator's Z80-mode coprocessor
(engine 1.93) does the 8-byte doubles (CSP 100+) and the SQT/SIN/COS/...
that the boot disk's Z80 interpreter lacks (FLOATS).  With an older engine,
TCV_Z80_COPROC=0 leaves those tests out (PCODE_ONLY)."""
import os, sys, subprocess, tempfile, shutil, zipfile
HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
sys.path.insert(0, HERE)
import ucsdvol
from psys import ensure_setup, BUILD

PCODE_ONLY = () if os.environ.get('TCV_Z80_COPROC', '1') != '0' else ('DOUBLES', 'FLOATS', 'PI')


def z80script(path, out):
    """the script without the '# ---- test NAME' sections of PCODE_ONLY"""
    keep = []
    skip = False
    for l in open(path).read().split('\n'):
        if l.startswith('# ---- '):
            skip = any(l == '# ---- test ' + n for n in PCODE_ONLY)
        if not skip:
            keep.append(l)
    open(out, 'w').write('\n'.join(keep))
    return out


def main(a):
    mode = a[0] if a else 'native'
    ensure_setup()
    d = tempfile.mkdtemp(prefix='tcverify_')
    for v in ('TINY-C', 'TCSRC', 'TCTESTS', 'TCEXTRA', 'TCVERIF', 'TCEXPCT'):
        shutil.copy(os.path.join(BUILD if v in ('TINY-C', 'TCSRC', 'TCTESTS', 'TCEXTRA') else os.path.join(ROOT, 'build'), v + '.BLK'), d)
    out = os.path.join(d, 'out')
    script = os.path.join(ROOT, 'verify', 'TCVERIFY.SCRIPT')
    if mode == 'z80':
        script = z80script(script, os.path.join(d, 'TCVERIFY.SCRIPT'))
    env = dict(os.environ, VERIFY_UNIT10=os.path.join(d, 'TCTESTS.BLK'), VERIFY_UNIT11=os.path.join(d, 'TCEXTRA.BLK'),
               VERIFY_UNIT12=os.path.join(d, 'TCVERIF.BLK'), VERIFY_UNIT13=os.path.join(d, 'TCEXPCT.BLK'))
    if mode == 'native' and env.get('VERIFY_RECLAIM', '1') == '1':
        env['VERIFY_RECLAIM'] = '1'
    else:
        env.pop('VERIFY_RECLAIM', None)
    r = subprocess.run([os.path.join(BUILD, 'run_verify'), os.path.join(BUILD, 'data'),
                        os.path.join(d, 'TINY-C.BLK'), os.path.join(d, 'TCSRC.BLK'), script,
                        mode, out, '', os.environ.get('TCV_MAX', '7200')], capture_output=True, text=True, env=env)
    ok = 'VERIFY SCRIPT COMPLETED' in r.stdout
    tail = [l for l in r.stderr.split('\n') if l.strip()][-3:]
    print('\n'.join(tail))
    print(r.stdout.strip()[-800:])
    if ok:                              # the results are on the volumes: CC2.CODE is the compiler's own work
        v = ucsdvol.Volume(os.path.join(out, 'VERIFY_SOURCE.BLK')) if os.path.exists(os.path.join(out, 'VERIFY_SOURCE.BLK')) else ucsdvol.Volume(os.path.join(d, 'TINY-C.BLK'))
        print('TINY-C: CC2.CODE %s (the volumes are in %s)' % ('there' if v.find('CC2.CODE') else 'not found', d))
    print('Tiny-C Verify (%s): %s   (work: %s)' % (mode, 'PASSED' if ok else 'FAILED', d))
    return 0 if ok else 1


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
