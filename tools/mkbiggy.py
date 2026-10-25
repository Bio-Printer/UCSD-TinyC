#!/usr/bin/env python3
"""mkbiggy.py -- volumes/BIGGY.zip: the emulator's boot volume BIGGY:
(data/Big_Disk.BLK in the P-Machine zip) with SYSTEM.SHELL, the Tiny-C shell
(examples/shell.c), which the Command: prompt's S runs.  Tiny-C itself --
CC.CODE, TCLIB.OBJ, TCMSGS.TEXT, the headers -- is only on TINY-C: (since
BIGGY 1.15; one copy of each file): X(ecute TINY-C:CC.  The compiler looks
for TCLIB.OBJ, TCMSGS.TEXT and headers on the prefix volume, the boot volume
and TINY-C:; those on the base disk (and an old TINYC.CODE) are removed.
The base disk is the reference Big_Disk.BLK of UCSD-Pascal-Volumes (Filer
and Editor that take NAME.C / NAME.H workfiles); only files that differ are
written, so with a current base the result is byte-identical to it.
BIGGY 1.27: SYSTEM.C, the operating system in C, beside SYSTEM.PASCAL, and
SYSTEM.MICRO with the boot that chooses between them (a key within 2 seconds:
SYSTEM.PASCAL), both from UCSD-SYSTEM-C (../UCSD-SYSTEM-C, or $SYSTEMC: its
build.py makes them).  BIGGY 1.28: SYSTEM.FILER of U134.4_OS_SOURCE_v1.14, whose
K(runch) asks for a reboot when it moved the running system, SYSTEM.C or
SYSTEM.PASCAL (UCSD-SYSTEM-C's tools/filer.py compiles it).  BIGGY 1.29:
SYSTEM.PASCAL of U134.4_OS_SOURCE_v1.15 (UCSD-SYSTEM-C's tools/ospascal.py
compiles it) and a SYSTEM.C to match: a new NAME.BAS (a BASIC source,
UCSD-BASIC) is a text file, as NAME.TEXT is.  BIGGY 1.30: SYSTEM.SHELL with
the library that takes NAME.BAS for a text file (TYPE).
The 8-byte doubles (CSP 100..137) are in the emulator itself (build it
from UCSD-Pascal---P-Machine_work-v1.88.zip); nothing on the disk is needed
for them."""
import os, sys, zipfile, shutil
HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
SYSTEMC = os.environ.get('SYSTEMC', os.path.join(os.path.dirname(ROOT), 'UCSD-SYSTEM-C'))
sys.path.insert(0, HERE)
import ucsdvol
from psys import ensure_setup
from tcrun import compile_c, INC

def main():
    ensure_setup()
    # the base disk: the reference Big_Disk.BLK of UCSD-Pascal-Volumes, as unpacked from
    # Usefull_System_Disk_Images.zip by setup.sh (build/data/Big_Disk.BLK is the copy the
    # tests boot)
    src = os.path.join(ROOT, 'build', 'img', 'Big_Disk.BLK')
    out = os.path.join(ROOT, 'build', 'BIGGY.BLK')
    shutil.copy(src, out)
    v = ucsdvol.Volume(out)
    changed = []

    def put(name, data, kind):
        """write NAME only when it differs: an up-to-date base disk (the
        reference Big_Disk.BLK in UCSD-Pascal-Volumes) stays byte-identical"""
        e = v.find(name)
        if e and v.read(name)[0] == data:
            return
        if e and e[2] == kind and (len(data) + 511) // 512 == e[1] - e[0]:
            # the same size: in its place, so the disk's free areas stay as they were
            # (CC's TCTEMP files need the second largest one while a pipe has the largest)
            v.data[e[0] * 512: e[1] * 512] = data + bytes((e[1] - e[0]) * 512 - len(data))
            v.entries[v.entries.index(e)] = [e[0], e[1], kind, e[3], len(data) - (e[1] - e[0] - 1) * 512, e[5]]
        else:
            v.write(name, data, kind)
        changed.append(name)
    # Tiny-C is on TINY-C: only
    gone = ['TINYC.CODE', 'CC.CODE', 'TCLIB.OBJ', 'TCMSGS.TEXT'] + \
           sorted(f.upper() for f in os.listdir(INC) if f.endswith('.h'))
    for name in gone:
        if v.find(name):
            v.remove(name)
            changed.append('-' + name)
    # the Tiny-C shell, which the Command: prompt's S runs (BIGGY 1.11)
    base, shell = compile_c(os.path.join(ROOT, 'examples', 'shell.c'), os.path.join(ROOT, 'build'), z80=True)
    put('SYSTEM.SHELL', open(shell, 'rb').read(), 2)
    # the break key (emulator 2.05): ` in CRTINFO.BREAK of SYSTEM.MISCINFO (and NEW.MISCINFO, its
    # copy for SETUP): file offset 84 (CRTINFO at 74: height, width, up .. right, eof, flush, brk)
    for name in ('SYSTEM.MISCINFO', 'NEW.MISCINFO'):
        data, kind = v.read(name)
        data = bytearray(data)
        data[84] = ord('`')
        put(name, bytes(data), kind)
    # BIGGY 1.27: SYSTEM.C (the OS in C) and the boot that chooses it or SYSTEM.PASCAL;
    # 1.29: SYSTEM.PASCAL of the OS source v1.15
    for name in ('SYSTEM.PASCAL', 'SYSTEM.C', 'SYSTEM.MICRO', 'SYSTEM.FILER'):
        path = os.path.join(SYSTEMC, name)
        if not os.path.exists(path):
            raise SystemExit('no %s (UCSD-SYSTEM-C: build.py, tools/filer.py, tools/ospascal.py)' % path)
        put(name, open(path, 'rb').read(), 2)
    text = ucsdvol.ucsd_to_text(v.read('VERSION.TEXT')[0])
    if '  SYSTEM.C ' not in text:
        text = text.replace('BIGGY boot volume, revision 1.26  (14-Oct-26)', 'BIGGY boot volume, revision 1.28  (17-Oct-26)')
        old_filer = ('  SYSTEM.FILER   from U134.4_OS_SOURCE_v1.07: G(et/S(ave take .C, .H\n'
                     '                 and NAME. (exact names); V(olumes lists units to 14\n')
        assert old_filer in text
        text = text.replace(old_filer, '  SYSTEM.FILER   from U134.4_OS_SOURCE_v1.14: G(et/S(ave take .C, .H\n'
                            '                 and NAME. (exact names); V(olumes lists units to 14;\n'
                            '                 K(runch asks for a reboot when it moved the running\n'
                            '                 system, SYSTEM.C or SYSTEM.PASCAL (1.28)\n')
        text = text.replace('  SYSTEM.FILER ', '  SYSTEM.C       the same OS written in C (UCSD-SYSTEM-C, SYS-C:): booted\n'
                            '                 unless a key is typed within 2 seconds (1.27)\n'
                            '  SYSTEM.MICRO   the interpreter, with the boot that asks for that key\n'
                            '                 (emulator 2.08 waits; source SYS-C:BOOTC.TEXT)\n'
                            '  SYSTEM.FILER ', 1)
    if 'revision 1.28 ' in text:
        text = text.replace('BIGGY boot volume, revision 1.28  (17-Oct-26)', 'BIGGY boot volume, revision 1.29  (25-Oct-26)')
        old_os = '  SYSTEM.PASCAL  the OS from U134.4_OS_SOURCE_v1.13 (MAXUNIT = 14:\n'
        assert old_os in text
        text = text.replace(old_os, '  SYSTEM.PASCAL  the OS from U134.4_OS_SOURCE_v1.15 (MAXUNIT = 14:\n')
        end = '                 FGOTOXY is MYGOTOXY.TEXT\'s (132 x 45): no BINDER needed\n'
        assert end in text
        text = text.replace(end, end + '                 1.29: a new NAME.BAS (a BASIC source) is a text\n'
                            '                 file, as NAME.TEXT is; SYSTEM.C as well\n')
    if 'revision 1.29 ' in text:
        text = text.replace('BIGGY boot volume, revision 1.29  (25-Oct-26)', 'BIGGY boot volume, revision 1.30  (25-Oct-26)')
        shell = ('                 1.23: built with the library that reads and writes a block at\n'
                 '                 a time (fgets, fread, fwrite to the console)\n')
        assert shell in text
        text = text.replace(shell, shell + '                 1.30: built with the library that takes NAME.BAS (a\n'
                            '                 BASIC source) for a text file, as NAME.TEXT (TYPE)\n')
    put('VERSION.TEXT', ucsdvol.text_to_ucsd(text), 3)
    if changed:
        v.save()
        print('updated on BIGGY:', ', '.join(changed))
    v = ucsdvol.Volume(out)
    lines = ['BIGGY:  %d files  (the boot volume, with the Tiny-C shell)' % len(v.entries), '']
    for first, last, kind, name, lastbyte, date in v.entries:
        lines.append('  %-15s %6d' % (name, last - first))
    open(os.path.join(ROOT, 'volumes', 'BIGGY.txt'), 'w').write('\n'.join(lines) + '\n')
    with zipfile.ZipFile(os.path.join(ROOT, 'volumes', 'BIGGY.zip'), 'w', zipfile.ZIP_DEFLATED) as z:
        z.write(out, 'Big_Disk.BLK')
    print('volumes/BIGGY.zip: %d files' % len(v.entries))

if __name__ == '__main__':
    main()
