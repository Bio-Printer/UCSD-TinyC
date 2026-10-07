#!/usr/bin/env python3
"""mkvolume.py -- build the two Tiny-C volumes (volumes/*.zip):

  TINY-C:   everything needed to use Tiny-C: CC.CODE, TCLIB.OBJ,
            TCMSGS.TEXT, the headers, README.TEXT, FILES.TEXT (the name
            TINY-C: is built into the compiler: it finds TCLIB.OBJ,
            TCMSGS.TEXT and headers there)
  TCSRC:    everything needed to rebuild it: the compiler's and the
            library's sources, BUILD.TEXT (@BUILD), LIBS.TEXT (@LIBS),
            README.TEXT, FILES.TEXT
  TCEXTRA:  the demo programs (DEMOS), the mini-shell and its examples
            (examples/), CMPCODE: sources and ready-to-run code files,
            README.TEXT, DEMOS.TEXT, FILES.TEXT
  TCTESTS:  the test programs (tests/ that are not demos): sources and
            ready-to-run code files, README.TEXT, TESTS.TEXT, FILES.TEXT
  (Four volumes: a UCSD directory holds 77 files, and @BUILD, @LIBS,
  @DEMOS and @TESTS leave a NAME.OBJ for every module or program.)
  TOOLS:    tools written in Tiny-C, ready to run (VI.CODE), README.TEXT,
            FILES.TEXT
  TOOLSRC:  their sources (VI.H, VIMAIN.C, ...), README.TEXT, FILES.TEXT

FILES.TEXT lists every file on its volume with its size and what it is;
the same listing is written next to the zips (volumes/NAME.txt).
"""
import os, sys, zipfile
HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
sys.path.insert(0, HERE)
import ucsdvol
from tcrun import build_lib, compile_c, compile_modules, INC
from buildtc import build, MODULES

SRC = os.path.join(ROOT, 'tinyc')
LIB = os.path.join(SRC, 'lib')
TESTS = os.path.join(ROOT, 'tests')
OUT = os.path.join(ROOT, 'volumes')
TMP = os.path.join(ROOT, 'build', 'volume_tmp')
LIBMODS = sorted(f[:-2] for f in os.listdir(LIB) if f.endswith('.c'))

README_TC = """TINY-C for UCSD Pascal II.0                        volume TINY-C:

Use: set the prefix to TINY-C: (F(iler, P(refix), then X(ecute CC and
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
The same commands can be arguments instead (from the shell: $ at the
Command: prompt): CC @BUILD @LIBS, CC /Z HANOI SIEVE.  Options and the
word after them make one command; they run in turn, no prompt, and the
first that fails stops CC.
X(ecute NAME runs a program.  WHICH DISK: a source, a #include file or
an @FILE named with a volume (TOOLSRC:VI.H, #5:X.C) is taken from there
only.  Named without one, CC looks on every disk: on one disk only, that
one; on several, the one on the disk of the file that names it (the
including file, the @FILE), else the one on the prefix volume, else it
lists the volumes and stops (name one).  Objects, code and temporary
files (TCTEMP.TEXT, TCTEMP.IR) go to the prefix volume; TCLIB.OBJ and
TCMSGS.TEXT are found on TINY-C: when they are not on the prefix volume
or the boot volume.

THE SOURCES of the compiler and the library, and the batch files that
rebuild them (@BUILD, @LIBS), are on TCSRC: (see its README.TEXT).

THE DEMO PROGRAMS are on TCEXTRA: (DEMOS.TEXT, @DEMOS), the test
programs on TCTESTS: (TESTS.TEXT, @TESTS).

RUNNING PROGRAMS FROM A PROGRAM: PSYS.H's pexec("NAME") runs NAME.CODE
and then starts the calling program again from the beginning, with
pexec_returned() and pexec_status() telling it so; nothing of the caller
stays in memory meanwhile.  pexec("NAME ARG1 ARG2") passes the words to
NAME's main(int argc, char **argv) (80 characters at most).  It needs the operating system of BIGGY 1.10
or later.  SHELL.C on TCEXTRA: is a small shell built on it.

SETJMP.H: setjmp(env) and longjmp(env, val) -- back to the setjmp from
any depth, through any segments (their code is given back as their
returns would have).  The function that called setjmp must not have
returned.

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

README_SRC = """TINY-C SOURCES                                    volume TCSRC:

The sources of the Tiny-C compiler (14 modules, TC.H, PARSE.H, GEN.H)
and of its library (%d modules, LIBINT.H), and the batch files that
rebuild them.
Mount TINY-C: as well: the compiler, the headers, TCLIB.OBJ and
TCMSGS.TEXT come from there.

REBUILDING EVERYTHING:  set the prefix to TCSRC:, then from the shell
($ at the Command: prompt) type  CC @BUILD @LIBS.

REBUILDING THE COMPILER:  set the prefix to TCSRC:, X(ecute
TINY-C:CC, answer  @BUILD.  BUILD.TEXT compiles the compiler's modules
and links TCSRC:CC2.CODE, identical to TINY-C:CC.CODE apart from its
name (check it with TCEXTRA:CMPCODE).  To use it, transfer it to
TINY-C:CC.CODE with the Filer.

REBUILDING THE LIBRARY:  set the prefix to TCSRC:, X(ecute TINY-C:CC,
answer  @LIBS.  LIBS.TEXT compiles the library modules and joins them
(/J) into TCSRC:TCLIB2.OBJ, identical to TINY-C:TCLIB.OBJ (check it with
CMPCODE).  To use it, transfer it to TINY-C:TCLIB.OBJ with the Filer.

Each module leaves a NAME.OBJ; remove those with the Filer if you like.

FILES.TEXT lists every file on this volume.
"""

README_SRC = README_SRC % len([f for f in os.listdir(os.path.join(ROOT, 'tinyc', 'lib')) if f.endswith('.c')])

README_EX = """TINY-C EXTRAS                                     volume TCEXTRA:

Demo programs for Tiny-C (the compiler is on TINY-C:, the test programs
on TCTESTS:).  Every program is here as source (NAME.C) and ready to run
(NAME.CODE):
X(ecute TCEXTRA:NAME.  To compile one yourself, set the prefix to
TCEXTRA: and X(ecute TINY-C:CC, answer NAME.

REBUILDING THEM ALL:  set the prefix to TCEXTRA:, X(ecute TINY-C:CC,
answer  @DEMOS.  DEMOS.TEXT compiles and links every program here (each
leaves a NAME.OBJ as well; remove those with the Filer if you like).

SHELL is a small shell: type a program's name (as for X(ecute), with
arguments if you like, and it runs it, then comes back with its exit
status.  A name without a volume (ARGS rather than TCEXTRA:ARGS) is
looked for on every disk on line; when several have it, SHELL lists them
and you choose with one key (no RETURN).  Its own commands:
  CD #5 (or CD 5, CD VOL, CD *)  the prefix becomes unit 5's volume, as
        the Filer's Prefix: later commands find their files there; CD
        alone shows it.
  DIR [VOL: or #5:][PATTERN]     the files of the prefix volume (or VOL:,
        or unit 5) whose names match: * or = any characters, ? any one
        (DIR *.C, DIR #9:, DIR TOOLSRC:VI*.C); ESC stops a long list.
  TYPE [VOL: or #5:]NAME         a text file on the console; with
        wildcards each file that matches, under its name.
  DELETE (or DEL) [VOL: or #5:]PATTERN   deletes the files that match;
        with wildcards it lists them and asks first (Y deletes).
  MEM   the shell's free memory;  BYE leaves it.
MEMFREE shows a program's free memory: run it with X(ecute and from
SHELL, it is the same (pexec leaves nothing of the shell in memory).
ARGS does what its arguments say: from SHELL try ARGS, ARGS ADD 2 3,
ARGS MUL 6 7, ARGS REPEAT 3 HELLO, ARGS ECHO A B C.  They need BIGGY 1.10
or later.
MEMMARK estimates another program's least free memory: MEMMARK FILL
fills the free memory with a pattern, run the program, MEMMARK SCAN
reports how much of the pattern is still intact (from X(ecute it asks
F or S).  Only an estimate (see memgap in PSYS.H); the emulator's
Options > Track Least Free Memory gives the exact figure.

CMPCODE compares two files byte by byte (for .CODE files the program
name in block 0 aside): after @BUILD on TCSRC:, compare
TINY-C:CC.CODE with TCSRC:CC2.CODE, after @LIBS TINY-C:TCLIB.OBJ
with TCSRC:TCLIB2.OBJ; it prints IDENTICAL.

FILES.TEXT lists every file on this volume.
"""

README_TS = """TINY-C TESTS                                      volume TCTESTS:

Test programs for Tiny-C (the compiler is on TINY-C:, the demos on
TCEXTRA:): each exercises a part of the language or the library and
prints what it computed.  Every program is here as source (NAME.C) and
ready to run (NAME.CODE): X(ecute TCTESTS:NAME.

REBUILDING THEM ALL:  set the prefix to TCTESTS:, X(ecute TINY-C:CC,
answer  @TESTS.  TESTS.TEXT compiles and links every program here (each
leaves a NAME.OBJ as well; remove those with the Filer if you like).

FILES.TEXT lists every file on this volume.
"""

# the tools (TOOLS:, TOOLSRC:): name, its modules (compiled one by one, then
# linked), its headers, what it is
VI_MODULES = ['ports/vi/%s.c' % m for m in
              ('vimain', 'viscreen', 'vitext', 'vicolon', 'vicmd', 'vipage', 'viucsd')]
TOOLS = [
    ('VI', VI_MODULES, ['ports/vi/vi.h', 'ports/vi/vipage.h'], 'vi, the screen editor: VI NAME.C from the shell'),
]

README_TOOLS = """TOOLS                                               volume TOOLS:

Tools written in Tiny-C, ready to run.  Their sources are on TOOLSRC:.

VI      the screen editor vi (the BusyBox "tiny vi"):  from the shell,
        VI NAME.C  (the name as typed; .C, .H and .TEXT files are text
        files); from X(ecute TOOLS:VI it asks for the file.  Most of vi's
        commands: moving, i a o O, x dd dw cw D C J p P y yy, . u U,
        marks, / ? n N, :w :q :wq ZZ :s :set :r :e, ...  The cursor keys
        are the P-System's (they take ^T ^R ^Q ^U); ^L redraws.
        Big files: vi keeps a window of the file in memory (about 4 KB in
        Z80 mode, 12 KB in P-Code mode, 30 KB with the Harvard layout) and
        the rest in VI.SWAP, a temporary file on the prefix volume (deleted
        when vi ends).  Moving, searching (/ ? n N go on through the whole
        file), G and :N move the window; files up to about 125 KB.  A
        command on more lines than fit in the window at once (500dd,
        :1,$s/a/b/ on a big file) is refused with a message.

FILES.TEXT lists every file on this volume.
"""

README_TOOLSRC = """TOOL SOURCES                                      volume TOOLSRC:

The sources of the tools on TOOLS:.

REBUILDING THEM:  set the prefix to TOOLSRC:, X(ecute TINY-C:CC, answer
@TOOLS.  TOOLS.TEXT compiles and links every tool (NAME.CODE here, and
a NAME.OBJ); copy the new NAME.CODE to TOOLS: with the Filer.

VI      vi: the BusyBox "tiny vi" (GPL v2 or later, see VI.H), edited
        for Tiny-C, in modules: VI.H (what they share), VIMAIN.C (start,
        main loop), VISCREEN.C (the screen), VITEXT.C (moving, changing
        text), VICOLON.C (the : commands), VICMD.C (the vi commands),
        VIPAGE.C and VIPAGE.H (the window into big files), VIUCSD.C (the
        P-System: keys, screen, files).  @TOOLS compiles each (/Z /C) and
        links them (/L VI=...), in P-Code or Z80 mode.

FILES.TEXT lists every file on this volume.
"""

# the demo programs (TCEXTRA:, with examples/ and CMPCODE); the other
# tests/ programs go on TCTESTS:
DEMOS = ('boxes', 'calc', 'demo', 'guess', 'hanoi', 'pi', 'queens', 'sieve')

LIBMODS_ = sorted(f[:-2] for f in os.listdir(os.path.join(ROOT, 'tinyc', 'lib')) if f.endswith('.c'))
LIBS = """; LIBS -- rebuild the C library: prefix TCSRC:, X(ecute TINY-C:CC, answer @LIBS
; Compiles every library module, then joins them into TCLIB2.OBJ.
""" + ''.join('/Z /C %s\n' % m.upper() for m in LIBMODS_) + \
    '/J TCLIB2=%s\n' % ','.join(m.upper() for m in LIBMODS_)

BUILD = """; BUILD -- rebuild the Tiny-C compiler: prefix TCSRC:, X(ecute TINY-C:CC, answer @BUILD
; Compiles every module, then links them into CC2.CODE.
""" + ''.join('/C %s\n' % m.upper() for m in MODULES) + \
    '/L CC2=%s\n' % ','.join(m.upper() for m in MODULES)


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
    'setjmp.h': 'setjmp, longjmp: back to an earlier point, out of any calls',
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
    'stmt.c': 'parser: statements, functions, declarations',
    'compile.c': "parser: the pass, helpers' and #pragma declarations",
    'ir.c': 'the intermediate file between parser and code generator',
    'gen.c': 'pass 3: P-code generation: emitter, object files',
    'genx.c': 'pass 3: P-code for expressions, calls, switch',
    'link.c': 'the linker: object files -> code file',
    'tc.h': 'shared declarations (every module)',
    'parse.h': "the parser modules' shared declarations",
    'gen.h': "gen.c and genx.c's shared declarations",
    'libint.h': "the library modules' shared declarations",
    'fltfmt.c': "printf's %f %e %g (linked only when floats are used)",
    'memscan.c': 'memfill, memgap (<psys.h>)',
    'pexec.c': 'pexec, main(argc, argv) (<psys.h>)',
    'strtold.c': 'strtold, atold (<stdlib.h>, P-Code mode)',
    'tcrt.c': 'runtime helpers: C division, shifts, unsigned, longs',
    'vi.h': "vi's modules share this: configuration, globals",
    'vipage.h': "vi's window into big files (VI.SWAP)",
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
    t.binary('CC.CODE', open(code, 'rb').read(), 'the Tiny-C compiler: X(ecute CC')
    t.binary('TCLIB.OBJ', open(build_lib(True), 'rb').read(), 'the C library (built with /Z), linked into every program')
    t.textfile('TCMSGS.TEXT', os.path.join(INC, 'tcmsgs.txt'), "the compiler's messages (line n = message n)")
    t.text('README.TEXT', README_TC, 'how to use Tiny-C')
    for f in sorted(os.listdir(INC)):
        if f.endswith('.h'):
            t.textfile(f.upper(), os.path.join(INC, f), 'header: ' + describe(os.path.join(INC, f)))
    t.finish()


def sources():
    t = Vol('TCSRC', 4000)
    t.text('README.TEXT', README_SRC, 'how to rebuild Tiny-C')
    t.text('BUILD.TEXT', BUILD, 'X TINY-C:CC, @BUILD: rebuilds the compiler -> CC2.CODE')
    t.text('LIBS.TEXT', LIBS, 'X TINY-C:CC, @LIBS: rebuilds the library -> TCLIB2.OBJ')
    for m in MODULES:
        p = os.path.join(SRC, m + '.c')
        t.textfile(m.upper() + '.C', p, 'compiler: ' + describe(p))
    for h in ('tc.h', 'parse.h', 'gen.h'):
        p = os.path.join(SRC, h)
        t.textfile(h.upper(), p, 'compiler: ' + describe(p))
    for m in LIBMODS:
        p = os.path.join(LIB, m + '.c')
        t.textfile(m.upper() + '.C', p, 'library: ' + describe(p))
    p = os.path.join(LIB, 'libint.h')
    t.textfile('LIBINT.H', p, 'library: ' + describe(p))
    t.finish()


def programs(vol, batch, progs, desc):
    """sources, ready-to-run code files and the @BATCH file that rebuilds them"""
    names = [os.path.splitext(os.path.basename(p))[0].upper()[:10] for p in progs]
    vol.text(batch + '.TEXT', '; %s -- compile and link every program on %s:\n'
             '; prefix %s:, X(ecute TINY-C:CC, answer @%s\n' % (batch, vol.name, vol.name, batch) +
             ''.join('/Z %s\n' % n for n in names), desc)
    for p in progs:
        name = os.path.splitext(os.path.basename(p))[0].upper()[:10]
        vol.textfile(name + '.C', p, describe(p))
        vol.binary(name + '.CODE', compiled(p), '  (ready to run)')


def extras():
    e = Vol('TCEXTRA', 4000)
    e.text('README.TEXT', README_EX, 'what is on this volume')
    progs = [os.path.join(TESTS, d + '.c') for d in DEMOS]
    progs.append(os.path.join(ROOT, 'verify', 'cmpcode.c'))
    progs += [os.path.join(ROOT, 'examples', f) for f in sorted(os.listdir(os.path.join(ROOT, 'examples'))) if f.endswith('.c')]
    programs(e, 'DEMOS', progs, 'X TINY-C:CC, @DEMOS: rebuilds every program here')
    e.finish()


def tests():
    t = Vol('TCTESTS', 4000)
    t.text('README.TEXT', README_TS, 'what is on this volume')
    progs = [os.path.join(TESTS, f) for f in sorted(os.listdir(TESTS)) if f.endswith('.c') and f[:-2] not in DEMOS]
    programs(t, 'TESTS', progs, 'X TINY-C:CC, @TESTS: rebuilds every program here')
    t.finish()


def tool_batch(name, mods):
    """TOOLS.TEXT's lines for a tool: each module compiled (/Z /C), then linked"""
    names = [os.path.splitext(os.path.basename(m))[0].upper() for m in mods]
    return ''.join('/Z /C %s\n' % n for n in names) + '/L %s=%s\n' % (name, ','.join(names))


def tools():
    t = Vol('TOOLS', 4000)
    t.text('README.TEXT', README_TOOLS, 'what is on this volume')
    os.makedirs(TMP, exist_ok=True)
    for name, mods, hdrs, desc in TOOLS:
        code = compile_modules([os.path.join(ROOT, m) for m in mods], os.path.join(TMP, name + '.CODE'), True)
        t.binary(name + '.CODE', open(code, 'rb').read(), desc)
    t.finish()
    s = Vol('TOOLSRC', 4000)
    s.text('README.TEXT', README_TOOLSRC, 'what is on this volume')
    s.text('TOOLS.TEXT', '; TOOLS -- compile and link every tool on TOOLSRC:\n'
           '; prefix TOOLSRC:, X(ecute TINY-C:CC, answer @TOOLS\n' +
           ''.join(tool_batch(name, mods) for name, mods, hdrs, desc in TOOLS),
           'X TINY-C:CC, @TOOLS: rebuilds every tool here')
    for name, mods, hdrs, desc in TOOLS:
        for f in mods + hdrs:
            p = os.path.join(ROOT, f)
            s.textfile(os.path.basename(f).upper(), p, '%s: %s' % (name.lower(), describe(p)))
    s.finish()


def main():
    tiny_c()
    sources()
    extras()
    tests()
    tools()
    old = os.path.join(ROOT, 'TinyC_Volume.zip')
    if os.path.exists(old):
        os.remove(old)


if __name__ == '__main__':
    main()
