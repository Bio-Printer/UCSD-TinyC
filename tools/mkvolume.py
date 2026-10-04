#!/usr/bin/env python3
"""mkvolume.py -- build the two Tiny-C volumes (volumes/*.zip):

  TINY-C:   everything needed to use Tiny-C and to rebuild it: TINYC.CODE,
            TCLIB.OBJ, TCMSGS.TEXT, the headers, the compiler's and the
            library's sources, BUILD.TEXT (X TINYC, @BUILD rebuilds the
            compiler), README.TEXT, FILES.TEXT
  TCEXTRA:  the test and demo programs (sources and ready-to-run code
            files), CMPCODE, README.TEXT, FILES.TEXT

FILES.TEXT lists every file on its volume with its size and what it is;
the same listing is written next to the zips (volumes/NAME.txt).
"""
import os, sys, zipfile
HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
sys.path.insert(0, HERE)
import ucsdvol
from tcrun import build_lib, compile_c, INC
from buildtc import build, MODULES

SRC = os.path.join(ROOT, 'tinyc')
LIB = os.path.join(SRC, 'lib')
TESTS = os.path.join(ROOT, 'tests')
OUT = os.path.join(ROOT, 'volumes')
TMP = os.path.join(ROOT, 'build', 'volume_tmp')
LIBMODS = sorted(f[:-2] for f in os.listdir(LIB) if f.endswith('.c'))

README_TC = """TINY-C for UCSD Pascal II.0                        volume TINY-C:

Use: set the prefix to TINY-C: (F(iler, P(refix), then X(ecute TINYC and
answer "Compile what file?" with
    NAME                 compile NAME.C (or NAME.TEXT), link with
                         TCLIB.OBJ -> NAME.CODE  (NAME.C works too)
    /C NAME              compile only -> NAME.OBJ
    /Z NAME, /Z /C NAME  the same, with calls through function pointers made
                         for the Z80 interpreter (see FUNCTION POINTERS)
    /L OUT=A,B,...       link A.OBJ, B.OBJ, ... and TCLIB.OBJ -> OUT.CODE
    /J LIB=A,B,...       join A.OBJ, B.OBJ, ... into the library LIB.OBJ
    @FILE                run the commands in FILE.TEXT, one per line
                         (blank lines and lines starting with ; skipped)
X(ecute NAME runs a program.  A program on another volume (e.g. TCEXTRA:)
compiles with that volume as the prefix: headers, TCLIB.OBJ and
TCMSGS.TEXT are found on TINY-C: when they are not on the prefix volume
or the boot volume.  Temporary files: TCTEMP.TEXT, TCTEMP.IR.

REBUILDING THE COMPILER:  X(ecute TINYC, answer  @BUILD
BUILD.TEXT compiles the compiler's 12 modules and links TINYC2.CODE,
which is identical to TINYC.CODE apart from its name (check it with
CMPCODE on TCEXTRA:).  To use it, rename it with the Filer.

REBUILDING THE LIBRARY:  X(ecute TINYC, answer  @LIBS
LIBS.TEXT compiles the 11 library modules and joins them (/J) into
TCLIB2.OBJ, identical to TCLIB.OBJ (check it with CMPCODE).  To use it,
remove TCLIB.OBJ and rename TCLIB2.OBJ to TCLIB.OBJ with the Filer.

THE DEMO PROGRAMS: see DEMOS.TEXT on TCEXTRA: (@DEMOS).

FUNCTION POINTERS: a call through a function pointer (qsort, bsearch and
printf's floating-point formatting use them) is by default compiled to
CSP 138 (CALLI), which the native P-Code engine implements and the Z80
interpreter does not.  /Z compiles the older sequence that works in both,
so TCLIB.OBJ and the programs on TCEXTRA: are built with /Z (LIBS.TEXT and
DEMOS.TEXT say so).  Build with /Z anything that must run in Z80 mode,
the library included; without /Z a program that calls through a function
pointer runs in Z80 mode but gives wrong results.

MEMORY: Tiny-C runs in Z80 mode as well as P-Code mode, and @BUILD
works in both.  (The Z80 interpreter on the boot disk has no SIN, COS,
EXP, ATAN, SQT, LOG or LN: math.h's sqrt, sin ... stop there with
"Unimplemented instruction"; P-Code mode has them.)

SOURCE FILES are NAME.C and headers NAME.H, in UCSD text format (a
.TEXT file's layout, and marked as text in the directory).  Tiny-C also
takes NAME.TEXT when there is no NAME.C, and X.H.TEXT for "x.h".  The
Filer's T(ransfer copies a text file to such a name as it is.

FILES.TEXT lists every file on this volume.
"""

README_EX = """TINY-C EXTRAS                                     volume TCEXTRA:

Test and demo programs for Tiny-C (the compiler is on TINY-C:).  Every
program is here as source (NAME.C) and ready to run (NAME.CODE):
X(ecute TCEXTRA:NAME.  To compile one yourself, set the prefix to
TCEXTRA: and X(ecute TINY-C:TINYC, answer NAME.

REBUILDING THEM ALL:  set the prefix to TCEXTRA:, X(ecute TINY-C:TINYC,
answer  @DEMOS.  DEMOS.TEXT compiles and links every program here (each
leaves a NAME.OBJ as well; remove those with the Filer if you like).

CMPCODE compares two files byte by byte (for .CODE files the program
name in block 0 aside): after @BUILD on TINY-C:, compare
TINY-C:TINYC.CODE with TINY-C:TINYC2.CODE, after @LIBS TINY-C:TCLIB.OBJ
with TINY-C:TCLIB2.OBJ; it prints IDENTICAL.

FILES.TEXT lists every file on this volume.
"""

LIBMODS_ = sorted(f[:-2] for f in os.listdir(os.path.join(ROOT, 'tinyc', 'lib')) if f.endswith('.c'))
LIBS = """; LIBS -- rebuild the C library: X(ecute TINYC, answer @LIBS
; Compiles every library module, then joins them into TCLIB2.OBJ.
""" + ''.join('/Z /C %s\n' % m.upper() for m in LIBMODS_) + \
    '/J TCLIB2=%s\n' % ','.join(m.upper() for m in LIBMODS_)

BUILD = """; BUILD -- rebuild the Tiny-C compiler: X(ecute TINYC, answer @BUILD
; Compiles every module, then links them into TINYC2.CODE.
""" + ''.join('/C %s\n' % m.upper() for m in MODULES) + \
    '/L TINYC2=%s\n' % ','.join(m.upper() for m in MODULES)


WHAT = {
    'assert.h': 'assert()',
    'conio.h': 'console: getch, putch, cputs, kbhit, gotoxy, clrscr',
    'ctype.h': 'isdigit, isalpha, toupper, ...',
    'fcntl.h': "open()'s flags: O_RDONLY, O_CREAT, ...",
    'float.h': 'FLT_MAX, FLT_EPSILON, ... (32-bit reals)',
    'io.h': 'open, read, write, lseek, close, unlink',
    'limits.h': 'INT_MAX, LONG_MAX, ... (16-bit int, 32-bit long)',
    'math.h': 'sqrt, sin, cos, atan, exp, log, pow, fabs, ...',
    'psys.h': 'SYSCOM: the P-System\'s SYSCOM record (SYSCOM->memtop, ...)',
    'stdarg.h': 'va_list, va_start, va_arg, va_end',
    'stddef.h': 'size_t, NULL, offsetof',
    'stdio.h': 'printf, scanf, FILE, fopen, fgets, fprintf, ...',
    'stdlib.h': 'malloc, free, atoi, rand, qsort, exit, ...',
    'string.h': 'strcpy, strcmp, strlen, memcpy, memset, ...',
    'tcrt.h': 'runtime helpers the compiler calls (reference only)',
    'main.c': 'the driver: commands, @FILE, the passes',
    'util.c': 'messages, output helpers, memory pools',
    'types.c': 'type predicates',
    'pp.c': 'pass 1: the preprocessor',
    'lex.c': 'pass 2: the tokenizer',
    'psym.c': 'parser: types and symbols',
    'expr.c': 'parser: expressions',
    'decl.c': 'parser: declarations and initializers',
    'stmt.c': 'parser: statements, functions, pragmas',
    'ir.c': 'the intermediate file between parser and code generator',
    'gen.c': 'pass 3: P-code generation, object files',
    'link.c': 'the linker: object files -> code file',
    'tc.h': 'shared declarations (every module)',
    'parse.h': "the parser modules' shared declarations",
    'libint.h': "the library modules' shared declarations",
    'fltfmt.c': "printf's %f %e %g (linked only when floats are used)",
    'tcrt.c': 'runtime helpers: C division, shifts, unsigned, longs',
}


def describe(path):
    """WHAT, or the file's first comment line without 'name --', cut at a word"""
    base = os.path.basename(path)
    if base in WHAT:
        return WHAT[base]
    if os.path.dirname(path) == LIB:
        return '<%s.h>' % base[:-2]
    line = open(path).readline().strip()
    line = line.replace('/*', '').replace('*/', '').strip()
    if ' -- ' in line:
        line = line.split(' -- ', 1)[1]
    line = line.rstrip('.,;:')
    if len(line) > 56:
        line = line[:56].rsplit(' ', 1)[0].rstrip('.,;:') + ' ...'
    return line


class Vol:
    def __init__(self, name, blocks):
        self.name = name
        self.path = os.path.join(ROOT, 'build', name + '.BLK')
        ucsdvol.main(['new', self.path, name, str(blocks)])
        self.v = ucsdvol.Volume(self.path)
        self.desc = {}

    def text(self, name, text, desc):
        self.v.write(name, ucsdvol.text_to_ucsd(text), 3)
        self.desc[name] = desc

    def textfile(self, name, path, desc=None):
        self.text(name, open(path).read(), desc or describe(path))

    def binary(self, name, data, desc):
        self.v.write(name, data, 2 if name.endswith('.CODE') else 5)
        self.desc[name] = desc

    def finish(self):
        # FILES.TEXT: written once to learn its own size, then with it
        self.text('FILES.TEXT', '', 'this list')
        for _ in range(2):
            listing = self.listing()
            self.v.write('FILES.TEXT', ucsdvol.text_to_ucsd(listing), 3)
        self.v.save()
        os.makedirs(OUT, exist_ok=True)
        open(os.path.join(OUT, self.name + '.txt'), 'w').write(listing)
        with zipfile.ZipFile(os.path.join(OUT, self.name + '.zip'), 'w', zipfile.ZIP_DEFLATED) as z:
            z.write(self.path, self.name + '.BLK')
        print('%s: %d files' % (self.name, len(self.v.entries)))

    def listing(self):
        lines = ['%s:  %d files  (a UCSD directory holds 77)' % (self.name, len(self.v.entries)), '',
                 '  %-15s %6s  %s' % ('FILE', 'BLOCKS', 'WHAT'), '']
        for first, last, kind, name, lastbyte, date in self.v.entries:
            lines.append('  %-15s %6d  %s' % (name, last - first, self.desc.get(name, '')))
        return '\n'.join(lines) + '\n'


def compiled(path):
    """compile a program with the host Tiny-C; its .CODE bytes"""
    os.makedirs(TMP, exist_ok=True)
    base, code = compile_c(path, TMP, z80=True)      # -z: the volumes work in Z80 mode too
    d, n = os.path.split(os.path.splitext(path)[0])
    for ext in ('.i', '.ir', '.obj'):                  # compile_c's temporaries
        if os.path.exists(os.path.join(d, n + ext)):
            os.remove(os.path.join(d, n + ext))
    return open(code, 'rb').read()


def tiny_c():
    t = Vol('TINY-C', 4000)
    code, log = build()
    t.binary('TINYC.CODE', open(code, 'rb').read(), 'the Tiny-C compiler: X(ecute TINYC')
    t.binary('TCLIB.OBJ', open(build_lib(True), 'rb').read(), 'the C library (built with /Z), linked into every program')
    t.textfile('TCMSGS.TEXT', os.path.join(INC, 'tcmsgs.txt'), "the compiler's messages (line n = message n)")
    t.text('README.TEXT', README_TC, 'how to use and rebuild Tiny-C')
    t.text('BUILD.TEXT', BUILD, 'X TINYC, @BUILD: rebuilds the compiler -> TINYC2.CODE')
    t.text('LIBS.TEXT', LIBS, 'X TINYC, @LIBS: rebuilds the library -> TCLIB2.OBJ')
    for f in sorted(os.listdir(INC)):
        if f.endswith('.h'):
            t.textfile(f.upper(), os.path.join(INC, f), 'header: ' + describe(os.path.join(INC, f)))
    for m in MODULES:
        p = os.path.join(SRC, m + '.c')
        t.textfile(m.upper() + '.C', p, 'compiler: ' + describe(p))
    for h in ('tc.h', 'parse.h'):
        p = os.path.join(SRC, h)
        t.textfile(h.upper(), p, 'compiler: ' + describe(p))
    for m in LIBMODS:
        p = os.path.join(LIB, m + '.c')
        t.textfile(m.upper() + '.C', p, 'library: ' + describe(p))
    p = os.path.join(LIB, 'libint.h')
    t.textfile('LIBINT.H', p, 'library: ' + describe(p))
    t.finish()


def extras():
    e = Vol('TCEXTRA', 4000)
    e.text('README.TEXT', README_EX, 'what is on this volume')
    progs = [os.path.join(TESTS, f) for f in sorted(os.listdir(TESTS)) if f.endswith('.c')]
    progs.append(os.path.join(ROOT, 'verify', 'cmpcode.c'))
    names = [os.path.splitext(os.path.basename(p))[0].upper()[:10] for p in progs]
    e.text('DEMOS.TEXT', '; DEMOS -- compile and link every program on TCEXTRA:\n'
           '; prefix TCEXTRA:, X(ecute TINY-C:TINYC, answer @DEMOS\n' +
           ''.join('/Z %s\n' % n for n in names), 'X TINY-C:TINYC, @DEMOS: rebuilds every program here')
    for p in progs:
        name = os.path.splitext(os.path.basename(p))[0].upper()[:10]
        e.textfile(name + '.C', p, describe(p))
        e.binary(name + '.CODE', compiled(p), '  (ready to run)')
    e.finish()


def main():
    tiny_c()
    extras()
    old = os.path.join(ROOT, 'TinyC_Volume.zip')
    if os.path.exists(old):
        os.remove(old)


if __name__ == '__main__':
    main()
