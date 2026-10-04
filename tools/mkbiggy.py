#!/usr/bin/env python3
"""mkbiggy.py -- volumes/BIGGY.zip: the emulator's boot volume BIGGY:
(data/Big_Disk.BLK in the P-Machine zip) with Tiny-C installed: TINYC.CODE,
TCLIB.OBJ, TCMSGS.TEXT, the headers (NAME.H) and SYSTEM.SHELL (examples/shell.c,
which the Command: prompt's $ runs).  The base disk is the
reference Big_Disk.BLK of UCSD-Pascal-Volumes (Filer and Editor that take
NAME.C / NAME.H workfiles); only files that differ are written, so with a
current base the result is byte-identical to it.  Tiny-C looks for headers,
TCLIB.OBJ and TCMSGS.TEXT on the boot volume (*) when they are not on the
prefix volume, so programs on any volume compile with X *TINYC.
The 8-byte doubles (CSP 100..137) are in the emulator itself (build it
from UCSD-Pascal---P-Machine_work-v1.88.zip); nothing on the disk is needed
for them."""
import os, sys, zipfile, shutil
HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
sys.path.insert(0, HERE)
import ucsdvol
from psys import ensure_setup
from tcrun import build_lib, compile_c, INC
from buildtc import build

def main():
    ensure_setup()
    # the base disk: the reference Big_Disk.BLK of UCSD-Pascal-Volumes, as unpacked from
    # Usefull_System_Disk_Images.zip by setup.sh (build/data/Big_Disk.BLK is the copy the
    # tests boot, with TINYC.CODE etc. of the checkout NOT yet installed)
    src = os.path.join(ROOT, 'build', 'img', 'Big_Disk.BLK')
    out = os.path.join(ROOT, 'build', 'BIGGY.BLK')
    shutil.copy(src, out)
    v = ucsdvol.Volume(out)
    changed = []

    def put(name, data, kind):
        """write NAME only when it differs: an up-to-date base disk (the
        reference Big_Disk.BLK in UCSD-Pascal-Volumes) stays byte-identical"""
        if not v.find(name) or v.read(name)[0] != data:
            v.write(name, data, kind)
            changed.append(name)
    code, log = build()
    put('TINYC.CODE', open(code, 'rb').read(), 2)
    put('TCLIB.OBJ', open(build_lib(True), 'rb').read(), 5)   # -z: works in Z80 mode too
    put('TCMSGS.TEXT', ucsdvol.text_to_ucsd(open(os.path.join(INC, 'tcmsgs.txt')).read()), 3)
    for f in sorted(os.listdir(INC)):
        if f.endswith('.h'):
            put(f.upper(), ucsdvol.text_to_ucsd(open(os.path.join(INC, f)).read()), 3)
    # the Tiny-C shell, which the Command: prompt's $ runs (OS 1.08, BIGGY 1.10)
    base, shell = compile_c(os.path.join(ROOT, 'examples', 'shell.c'), os.path.join(ROOT, 'build'), z80=True)
    put('SYSTEM.SHELL', open(shell, 'rb').read(), 2)
    if changed:
        v.save()
        print('updated on BIGGY:', ', '.join(changed))
    v = ucsdvol.Volume(out)
    lines = ['BIGGY:  %d files  (the boot volume, with Tiny-C added)' % len(v.entries), '']
    for first, last, kind, name, lastbyte, date in v.entries:
        lines.append('  %-15s %6d' % (name, last - first))
    open(os.path.join(ROOT, 'volumes', 'BIGGY.txt'), 'w').write('\n'.join(lines) + '\n')
    with zipfile.ZipFile(os.path.join(ROOT, 'volumes', 'BIGGY.zip'), 'w', zipfile.ZIP_DEFLATED) as z:
        z.write(out, 'Big_Disk.BLK')
    print('volumes/BIGGY.zip: %d files' % len(v.entries))

if __name__ == '__main__':
    main()
