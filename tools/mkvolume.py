#!/usr/bin/env python3
"""mkvolume.py -- build the Tiny-C volumes (volumes/*.zip):

  TINY-C:   Tiny-C: CC.CODE, TCMSGS.TEXT, README.TEXT, FILES.TEXT and the
            sub-volumes INCLUDE (the headers), LIB (TCLIB.OBJ) and SRC (the
            sources: CC/ with BUILD.TEXT, LIB/ with LIBS.TEXT); ALL.TEXT at
            the top rebuilds everything.
            The name TINY-C: is built into the compiler: it finds
            TCMSGS.TEXT, TINY-C:/LIB/TCLIB.OBJ and TINY-C:/INCLUDE there.
  TCEXTRA:  the demo programs (DEMOS), the mini-shell and its examples
            (examples/), CMPCODE: sources and ready-to-run code files,
            README.TEXT, DEMOS.TEXT, FILES.TEXT
  TCTESTS:  the test programs (tests/ that are not demos): sources and
            ready-to-run code files, README.TEXT, TESTS.TEXT, FILES.TEXT
  TOOLS:    the tools written in Tiny-C, ready to run (VI.CODE ...),
            MAN.TEXT, README.TEXT, FILES.TEXT and the sub-volume SRC: their
            sources, a sub-volume each (SRC/VI, SRC/GREP ...), TOOLS.TEXT.
  (A UCSD directory holds 77 files: the demos and tests are volumes of
  their own, and @DEMOS and @TESTS leave a NAME.OBJ for every program.)

FILES.TEXT lists every file on its volume, those in its sub-volumes too
(INCLUDE/STDIO.H), with its size and what it is; the same listing is
written next to the zips (volumes/NAME.txt).
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

WHAT IS WHERE (FILES.TEXT lists every file):
    CC.CODE          the compiler
    TCMSGS.TEXT      its messages
    ALL.TEXT         CC @TINY-C:ALL rebuilds the compiler, the library and
                     every tool at once
    INCLUDE/         the headers: STDIO.H, STRING.H, PSYS.H ...
    LIB/             the C library, TCLIB.OBJ
    OBJ/             empty: the objects a rebuild makes (BUILD's and LIBS's -O)
    SRC/             the sources, and the batch files that rebuild them:
      SRC/CC/          the compiler's modules, BUILD.TEXT
      SRC/LIB/         the library's modules, LIBS.TEXT
The tools (VI, GREP, ...) are on TOOLS:, their sources in TOOLS:/SRC; the
demos on TCEXTRA:, the test programs on TCTESTS:.  INCLUDE, LIB and SRC are
sub-volumes (the shell's MKDIR, CD, DIR: DIR TINY-C:/INCLUDE, CD
TINY-C:/SRC/CC); the Filer sees each as one data file.

Use: from the shell (S at the Command: prompt) type CC NAME, or
X(ecute TINY-C:CC and answer "Compile what file?" with
    NAME                 compile NAME.C (or NAME.TEXT), link with
                         TCLIB.OBJ -> NAME.CODE  (NAME.C works too)
    -C NAME              compile only -> NAME.OBJ
    -Z NAME, -Z -C NAME  the same, with calls through function pointers made
                         for the Z80 interpreter (see FUNCTION POINTERS)
    -L OUT=A,B,...       link A.OBJ, B.OBJ, ... and TCLIB.OBJ -> OUT.CODE
    -J LIB=A,B,...       join A.OBJ, B.OBJ, ... into the library LIB.OBJ
    -O DIR               the objects of the commands after it go into DIR
                         (TINY-C:/OBJ), and -L and -J look there first; until
                         the next -O
Options start with - (a path may start with /); -c and -C are the same.
    @FILE                run the commands in FILE.TEXT, one per line
                         (blank lines and lines starting with ; skipped;
                         a line @OTHER runs OTHER.TEXT, then goes on)
The same commands can be arguments instead: CC @TINY-C:ALL, CC -Z
HANOI SIEVE.  Options and the word after them make one command; they run
in turn, no prompt, and the first that fails stops CC.
X(ecute NAME runs a program.  WHICH DISK: a source, a #include file or
an @FILE named with a volume (TOOLS:/SRC/VI/VI.H, #5:X.C) is taken from
there only.  Named without one, CC looks on every disk: on one disk only,
that one; on several, the one on the disk of the file that names it (the
including file, the @FILE), else the one on the prefix volume, else it
lists the volumes and stops (name one).  HEADERS: <x.h> is taken from
TINY-C:/INCLUDE first, "x.h" from the including file's sub-volume first,
then as above, then from TINY-C:/INCLUDE.  SUB-VOLUMES: a path (CC A/X,
CC #5:/A/B/X, #include "A/X.H", -L OUT=A/X,A/Y) is taken from there;
after the shell's CD into a sub-volume CC looks in it first; the names in
an @FILE in a sub-volume (SRC/CC/BUILD's MAIN, TOOLS:/SRC/TOOLS's VI/VIMAIN)
are looked for beside it first.  An object (NAME.OBJ) goes next to its
source, and -L and -J find objects the same way as sources; the code file
goes where NAME says (its volume or path, else the shell's path or the
prefix volume) -- the shell runs a code file in a sub-volume where it is;
the temporary files (TCTEMP.TEXT, TCTEMP.IR) go to the prefix volume, or to
the volume that holds the shell's path; TCLIB.OBJ and TCMSGS.TEXT are
taken from the prefix volume or the boot volume when they are there, else
from TINY-C:/LIB and TINY-C:.

REBUILDING (see SRC/README.TEXT):  CC @TINY-C:/SRC/CC/BUILD makes
TINY-C:CC2.CODE, CC @TINY-C:/SRC/LIB/LIBS TINY-C:/LIB/TCLIB2.OBJ, CC
@TINY-C:ALL both and every tool.

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
interpreter does not.  -Z compiles the older sequence that works in both,
so TCLIB.OBJ and the programs on TCEXTRA: are built with -Z (LIBS.TEXT and
DEMOS.TEXT say so).  Build with -Z anything that must run in Z80 mode,
the library included; without -Z a program that calls through a function
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

README_SRC = """TINY-C SOURCES                              TINY-C:/SRC

The sources of the Tiny-C compiler (CC/: 14 modules, TC.H, PARSE.H,
GEN.H) and of its library (LIB/: %d modules, LIBINT.H), and the batch
files that rebuild them; the tools' sources are in TOOLS:/SRC.

REBUILDING THE COMPILER:  from the shell, CC @TINY-C:/SRC/CC/BUILD (or
X(ecute TINY-C:CC, answer @TINY-C:/SRC/CC/BUILD).  BUILD.TEXT compiles the
compiler's modules and links TINY-C:CC2.CODE (next to the compiler),
identical to TINY-C:CC.CODE apart from its name (check it with
TCEXTRA:CMPCODE).  To use it, change its name to CC.CODE with the Filer.

REBUILDING THE LIBRARY:  CC @TINY-C:/SRC/LIB/LIBS.  LIBS.TEXT compiles the
library modules and joins them (-J) into TINY-C:/LIB/TCLIB2.OBJ, identical
to TINY-C:/LIB/TCLIB.OBJ (check it with CMPCODE).  To use it, RENAME
TINY-C:/LIB/TCLIB2.OBJ TCLIB.OBJ from the shell (after deleting the old
one).

EVERYTHING AT ONCE:  CC @TINY-C:ALL (TOOLS: on line too): BUILD, LIBS,
then TOOLS:/SRC/TOOLS (every tool linked onto TOOLS:).

The names in a batch file are looked for beside it first, so these work
from anywhere.  Their first command, -O TINY-C:/OBJ, puts every NAME.OBJ
into TINY-C:/OBJ, not among the sources (DELETE TINY-C:/OBJ/*.OBJ from
the shell to clear it).
CC/OBJFORMAT.TEXT describes what is in a .OBJ file and how the linker
reads it.
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

SHELL is a small shell (its prompt is where names without a volume are:
BIGGY:> , or BIGGY:/A/B>  after a CD into a sub-volume; PWD says it too):
type a program's name (as for X(ecute), with
arguments if you like, and it runs it, then comes back with its exit
status.  A name without a volume (ARGS rather than TCEXTRA:ARGS) is
looked for on the path (CD into a sub-volume) first, then on every disk
on line; when several have it, SHELL lists them and you choose with one
key (no RETURN).  A path (A/PROG, #5:/A/PROG) runs the code file in that
sub-volume, where it is (no copy: the OS loads its segments from there);
X(ecute at the Command: prompt takes such paths too (BIGGY 1.26).
Its own commands:
  CD #5 (or CD 5, CD VOL, CD *)  the prefix becomes unit 5's volume, as
        the Filer's Prefix: later commands find their files there; CD
        alone shows it.
  CD PATH   a sub-volume (CD #5:/A, CD A/B, CD .., CD VOL: leaves it):
        names without a volume are in it, for the shell and its programs.
        CD A: the sub-volume A where you are, else the volume A.
  MKDIR NAME [BLOCKS]   a sub-volume: a data file NAME (100 blocks unless
        BLOCKS) whose blocks 2..5 are a directory of its own (77 files).
        Its files are VOL:NAME/FILE or VOL:/NAME/FILE, and sub-volumes
        nest (VOL:/A/B/FILE); a \\ is a / (VOL:\\A\\FILE, as Windows
        writes paths); . is here, .. the sub-volume that holds it (COPY
        X.C .., DIR .., COPY #9:*.C with no DEST: here).  Programs open such paths like any name;
        DIR, TYPE, DELETE, WHEREIS, COPY, MOVE and RENAME take them too
        (DIR #5:/A/*.C, COPY X.C A, MOVE A/*.C #9:); not yet the
        wildcards of GREP, LC and WC.
        DIR A lists what is in A; DIR marks sub-volumes "Sub-volume".
  RMDIR NAME   a sub-volume goes; with files in it, after a Y (not one
        the shell's path is in).  DELETE leaves sub-volumes alone.
  DIR [-E] [-S | -N] [VOL: or #5:][PATTERN]   the files of the prefix
        volume (or VOL:, or unit 5) whose names match: * or = any
        characters, ? any one (DIR *.C, DIR #9:, DIR TOOLS:/SRC/VI/*.C); ESC
        stops a long list.  -E: each file's first block and bytes in its
        last block too, and the unused areas between the files; -S sorts
        the files by size (the largest first), -N by name (else they are in
        the order of their blocks); -ES both.
  TYPE [VOL: or #5:]NAME         a text file on the console; with
        wildcards each file that matches, under its name.
  DELETE (or DEL) [VOL: or #5:]PATTERN   deletes the files that match;
        with wildcards it lists them and asks first (Y deletes).
  WHEREIS [VOL: or #5:]PATTERN   the files that match on every disk on
        line and in their sub-volumes (WHEREIS STDIO.H, WHEREIS *.C,
        WHEREIS ALL*), or on the volume or in the sub-volume named.
  VOLUMES (or VOLS)              every disk on line: unit, volume,
        files, blocks used of its size; (boot) and (prefix) marked.
  COPY SOURCE DEST    SOURCE: [VOL: or #5: or a path]PATTERN; DEST: a
        volume or sub-volume (#9:, TOOLS:, A/, #5:/A/B/, or A when A is
        a sub-volume) keeps the names, or for one file a new name
        ([VOL:]NAME; no volume: the path or the prefix).  The copy keeps
        the kind (a .C stays a text file), the date and the length; a
        file of that name there is replaced.  A sub-volume copies whole.
  MOVE SOURCE DEST    as COPY, then the original is deleted; in its own
        directory only its name changes.
  RENAME (or REN) [VOL: or #5: or a path]NAME NEWNAME   where it is.
THE COMMAND LINE: Up and Down bring back the last 10 commands (kept in
#4:SYSTEM.CMDS, so they are there the next time too); Left, Right, Home
and End move in the line; Insert switches between inserting and typing
over; Delete deletes the character at the cursor, Backspace the one
before it; ESC clears the line.  Home, End, Insert and Delete need
emulator 2.00 or later.
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
name in block 0 aside): after CC @TINY-C:/SRC/CC/BUILD, compare
TINY-C:CC.CODE with TINY-C:CC2.CODE, after CC @TINY-C:/SRC/LIB/LIBS
TINY-C:/LIB/TCLIB.OBJ with TINY-C:/LIB/TCLIB2.OBJ; it prints IDENTICAL.

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

# the tools (TOOLS:, their sources in TOOLS:/SRC): name, its modules (compiled one by one, then
# linked), its headers, what it is
VI_MODULES = ['ports/vi/%s.c' % m for m in
              ('vimain', 'viscreen', 'vitext', 'vicolon', 'vicmd', 'vipage', 'vipage2', 'viucsd')]
TOOLS = [
    ('VI', VI_MODULES, ['ports/vi/vi.h', 'ports/vi/vipage.h'], 'vi, the screen editor: VI NAME.C from the shell'),
    ('GREP', ['ports/grep/grep.c'], [], 'grep: GREP [-i] [-n] PATTERN FILES from the shell'),
    ('CAT', ['ports/cat/cat.c'], [], 'cat: CAT [-AbeEnstTuv] [FILE ...] from the shell'),
    ('HEXDUMP', ['ports/hexdump/hexdump.c'], [], 'hexdump: HEXDUMP [-i] [FILE ...] from the shell'),
    ('OBJCOPY', ['ports/objcopy/objcopy.c'], [], 'objcopy: OBJCOPY [-I ihex|hexdump] [INFILE [OUTFILE]]: a hex dump back into a file'),
    ('MORE', ['ports/more/more.c'], [], 'more: MORE [-N] [-s] [-i] [+N] [FILE ...] a screen at a time'),
    ('LESS', ['ports/less/less.c'], [], 'less: LESS [-NSiF] [+N] [FILE ...] forwards and backwards'),
    ('SED', ['ports/sed/sed.c', 'ports/sed/sedre.c'], ['ports/sed/sed.h'], 'sed: SED [-nEsur] [-i[SUF]] [-e S] [-f F] [S] [FILE ...] a stream editor'),
    ('DIFF', ['ports/diff/diff.c'], [], 'diff: DIFF [-iwbBqs] [-u|-U N|-c|-C N] FILE1 FILE2'),
    ('WC', ['ports/wc/wc.c'], [], 'wc: WC [-clmwL] [FILE ...] lines, words and bytes'),
    ('LC', ['ports/lc/lc.c'], [], 'lc: LC [-1xFsrSC] [VOL: | #n:][PATTERN ...] the file names in columns'),
    ('KRUNCH', ['ports/krunch/krunch.c'], [], 'krunch: KRUNCH [-y] [VOL: | #n | PATH] the files moved down, the free blocks in one area'),
    ('GROW', ['ports/grow/grow.c'], [], 'grow: GROW NAME +N | -N | N a sub-volume bigger (or smaller)'),
    ('SHRINK', ['ports/shrink/shrink.c'], [], 'shrink: SHRINK NAME [-N | +N | N] a sub-volume smaller (no size: no free blocks left)'),
    ('PATCH', ['ports/patch/patch.c'], [], 'patch: PATCH [-RsbClN] [-F N] [-pN] [-o OUT] [-i PATCHFILE] [FILE]'),
    ('MAN', ['ports/man/man.c'], [], 'man: MAN [-k WORD] [NAME ...] the manual pages (MAN.TEXT)'),
    ('BINDERC', ['ports/binder/binderc.c'], [], 'BINDERC [FILE]: the Binder (puts your GOTOXY into SYSTEM.PASCAL)'),
]

README_TOOLS = """TOOLS                                               volume TOOLS:

Tools written in Tiny-C, ready to run.  Their sources are in TOOLS:/SRC, a
sub-volume each (TOOLS:/SRC/VI, TOOLS:/SRC/GREP ...; SRC/README.TEXT); the
objects a rebuild makes go into TOOLS:/OBJ (empty until then).

VI      the screen editor vi (the BusyBox "tiny vi"):  from the shell,
        VI NAME.C  (the name as typed; .C, .H and .TEXT files are text
        files); from X(ecute TOOLS:VI it asks for the file.  Most of vi's
        commands: moving, i a o O, x dd dw cw D C J p P y yy, . u U,
        marks, / ? n N, :w :q :wq ZZ :s :set :r, ...  The cursor keys
        are the P-System's (they take ^T ^R ^Q ^U); ^L redraws.  Page Up,
        Page Down, Home, End, Insert and Delete work with emulator 2.00
        or later, the mouse wheel (3 lines a notch) with 2.01 or later;
        with 2.02 or later the cursor is a block while vi runs (UCSD-C
        emulator/KEYS.md).
        Big files: vi keeps a window of the file in memory (about 4 KB in
        Z80 mode, 12 KB in P-Code mode, 30 KB with the Harvard layout) and
        the rest in VI.SWAP, a temporary file on the prefix volume (the
        file's size and 32 KB; deleted when vi ends; on the prefix volume
        even after a CD into a sub-volume -- a file in one is found once, at
        the start, and read and saved by where it is, its window 1 KB
        smaller: in Z80 mode about 2.4 KB).  Moving, searching (/ ? n N go on through the whole
        file), G and :N move the window; files up to about 185 KB.  A
        command on more lines than fit in the window at once (500dd,
        :1,$s/a/b/ on a big file) is refused with a message.
        Where it was left: on leaving a saved or unchanged text file vi keeps
        the cursor's line and column in the file's 1 KB header page (6 bytes
        the Editor never uses); the next VI of the file starts there, that line
        in the middle of the screen.

GREP    GREP [-i] [-n] PATTERN [VOL: or #5:]FILES ...  from the shell: the
        lines of text files that match, as VOL:NAME:LINE: text.  Case is
        ignored unless -i (the other way round from Unix).  FILES takes
        * = ? wildcards and is looked for on every disk unless a volume
        is named.  PATTERN: * any characters (as in file names), . any
        one, [abc] [^abc] [a-z] a class, x+ x? x once or more / or not,
        ^ $ the line's start and end, \\c c itself (\\*: a star), \\s a
        blank (the shell splits words at blanks).
        GREP printf *.C, GREP -i ^int #9:*.H, GREP vi*pageup *.C

BINDERC CAT     CAT [-AbeEnstTuv] [FILE ...]  from the shell: the files one after the
        other on the standard output (no FILE or -: the standard input; the
        shell's >FILE, <FILE and | work).  The options of cat(1), not the long
        ones: -A -vET, -b number non-empty lines, -e -vE, -E $ at each line's
        end, -n number all lines, -s one empty line for several, -t -vT, -T
        tabs as ^I, -u ignored, -v control characters as ^X and bytes of 128
        and up as M-x.  -bn, -vET: joined; -- ends the options.  .TEXT, .C and
        .H files are text, any other file is read as the bytes it holds.

HEXDUMP HEXDUMP [-i] [FILE ...]  from the shell: a hex dump of the files (one
        stream; no FILE or -: the standard input):  the offset in hex, 16
        bytes in hex with a blank between them (two more blanks after the
        8th), then the bytes as characters (. for the unprintable ones):
        00000000  48 65 6C 6C 6F 20 77 6F   72 6C 64 0A 00 01 02 03  Hello world.....
        -i: Intel Hex instead, a record of 16 bytes a line (:LLAAAATTDD..CC,
        extended linear address records every 64 KB, the end record
        :00000001FF).  HEXDUMP -i A.CODE >A.HEX.  .TEXT, .C and .H files are
        text (a line is a 0A), any other file the bytes it holds.

OBJCOPY OBJCOPY [-I ihex|hexdump] [-O binary] [INFILE [OUTFILE]]  from the
        shell: a listing made by HEXDUMP (either form, or the same style by
        another program) back into the bytes: the offsets place them (a gap is
        filled with 0 bytes), Intel Hex checked by its checksums; without -I
        the first character decides (':' is Intel Hex).  No INFILE or -: the
        standard input; no OUTFILE: the standard output.  HEXDUMP A.CODE
        >A.HEX, OBJCOPY A.HEX B.CODE; HEXDUMP -I A.CODE | OBJCOPY - B.CODE.
        A file that is not .TEXT/.C/.H is whole blocks (the last one is
        filled up with 0 bytes: UCSD files do not keep a length).

MORE    MORE [-N] [-s] [-i] [+N] [FILE ...]  from the shell: a screen at a
        time (the standard input when there is no FILE: DIR | MORE).  At the
        --More--(NN%) prompt: SPACE the next screen, RETURN a line, d half a
        screen, /text search (n again), = the line number, h help, q quit;
        -N lines to a screen, -s squeeze empty lines, -i ignore case in
        searches, +N start at line N.  Long lines wrap.

LESS    LESS [-NSiF] [+N] [FILE ...]  from the shell: a screen at a time,
        forwards and backwards (the standard input when it was redirected:
        DIR | LESS).  SPACE/f PgDn and b PgUp a screen, RETURN/j and k a line
        (the mouse wheel 3), d u half a screen, g G the first and last line
        (Ng: line N; N%), /text ?text search forward and back (n N again),
        :n :p :x the next, previous and first file, = where you are, h help,
        q quit.  -N line numbers, -S chop long lines (else they wrap), -i
        ignore case, -F quit if the file fits on the screen, +N start at line
        N.  Any size of file: it reads as far as the keys need and keeps a
        sparse index of line starts (needs emulator 2.04).

SED     SED [-nEsur] [-i[SUFFIX]] [-e SCRIPT] [-f FILE] [SCRIPT] [FILE ...]
        from the shell: a stream editor: the commands of a script (s y a i c
        d p q n N h H g G x b t = l r w ...) run on each line of the files,
        with addresses (N $ /re/ ranges first~step), basic or extended (-E)
        regular expressions.  SED -n /x/p A.C; SED s/a/b/g A.TEXT; -i edits in
        place.  (Write \\< \\> \\| for the shell's special characters, or use -f.)

DIFF    DIFF [-iwbBqs] [-u|-U N|-c|-C N] FILE1 FILE2  from the shell: what
        differs between two text files: normal, unified or context form;
        exit status 0 same, 1 different, 2 trouble.  Any size of file.

WC      WC [-clmwL] [FILE ...]  from the shell: the lines, words and bytes of
        the files (-L the longest line) and a total; wild cards in the names
        (WC *.C); DIR | WC -l.

LC      LC [-1xFsrSC] [VOL: | #n: | PATH][PATTERN ...]  from the shell: the
        file names of a disk (or a sub-volume) in columns as wide as the console (down the columns, -x
        across), -F marks CODE files, -s shows the sizes; one name to a line
        when the output goes to a file or a pipe.

KRUNCH  KRUNCH [-y] [VOL: | #n | PATH]  from the shell: moves the files of
        a disk (or of a sub-volume: KRUNCH #5:/A) down so that its unused
        blocks become one area at its end (the Filer's K(runch); asks first
        unless -y).  Running code files (SYSTEM.PASCAL, the shell) may move:
        KRUNCH tells the OS.

GROW    GROW NAME SIZE, SHRINK NAME [SIZE]  from the shell: the sub-volume NAME
SHRINK  (MKDIR makes one) becomes SIZE blocks: +N (N more), -N (N fewer) or
        N; SHRINK NAME alone leaves it no free blocks.  Growing takes the blocks after it, or moves it to a free area,
        krunching the volume that holds it when it must; shrinking krunches
        the sub-volume first only when its files reach past the new end.

PATCH   PATCH [-RsbClN] [-F N] [-pN] [-o OUT] [-i PATCHFILE] [FILE]  from the
        shell: applies a normal, unified or context diff (DIFF's, or the
        host's) to a file, finds hunks that moved and with 2 lines of fuzz,
        writes the hunks that do not fit to NAME.REJ.TEXT; -R undoes a patch.

MAN     MAN [-l] [-k WORD] [NAME ...]  from the shell: the manual page of a
        program on this volume (what it is for, every option and key); MAN
        alone lists them, -k WORD finds the pages with WORD in them.  The text
        is the one file MAN.TEXT next to MAN.CODE (add a page there for each
        new tool: MAN VI | LESS).

BINDERC [FILE]  (or X(ecute TOOLS:BINDERC): the Pascal System's
        Binder, BINDER.CODE, rewritten in Tiny-C (its Pascal source is lost;
        it makes the same SYSTEM.PASCAL).  Puts the GOTOXY(X,Y: INTEGER)
        procedure of your own, the first procedure of the code file FILE
        (name as typed, else with .CODE), into SYSTEM.PASCAL of the prefix
        volume, which is replaced.  Run SETUP too.  Back up the boot disk.

FILES.TEXT lists every file on this volume.
"""

README_TOOLS_SRC = """TOOL SOURCES                                TOOLS:/SRC

The sources of the tools on TOOLS:, a sub-volume each: SRC/VI holds vi's
modules, SRC/GREP GREP.C, and so on.

REBUILDING THEM:  from the shell, CC @TOOLS:/SRC/TOOLS (or X(ecute
TINY-C:CC, answer @TOOLS:/SRC/TOOLS).  TOOLS.TEXT compiles every tool's
modules (VI/VIMAIN: found beside TOOLS.TEXT; -O TOOLS:/OBJ puts each
NAME.OBJ into TOOLS:/OBJ) and links the tool straight onto TOOLS:
(TOOLS:NAME.CODE, replacing the one there).

EVERYTHING AT ONCE:  CC @TINY-C:ALL: the compiler (TINY-C:CC2.CODE),
the library (TINY-C:/LIB/TCLIB2.OBJ), then TOOLS; a batch file can name
other batch files (@NAME).

A NEW TOOL: MKDIR TOOLS:/SRC/NAME, its sources there, its lines in
TOOLS.TEXT (-Z -C NAME/NAME, -L TOOLS:NAME=NAME/NAME) and a page in
TOOLS:MAN.TEXT.

VI      vi: the BusyBox "tiny vi" (GPL v2 or later, see VI.H), edited
        for Tiny-C, in modules: VI.H (what they share), VIMAIN.C (start,
        main loop), VISCREEN.C (the screen), VITEXT.C (moving, changing
        text), VICOLON.C (the : commands), VICMD.C (the vi commands),
        VIPAGE.C and VIPAGE.H (the window into big files), VIUCSD.C (the
        P-System: keys, screen, files).  @TOOLS compiles each (-Z -C) and
        links them (-L VI=...), in P-Code or Z80 mode.

GREP.C  grep (a regular expression, wildcards, every disk).

CAT.C   cat (the options of cat(1)).

SED.C, SEDRE.C, SED.H  sed (the stream editor, its regular expressions).

DIFF.C  diff (two files compared, three output forms).

WC.C    wc (lines, words, bytes).

LC.C    lc (the file names of a disk in columns).

KRUNCH.C  krunch (the files of a disk moved down, the free blocks in one area).

GROW.C, SHRINK.C  grow, shrink (a sub-volume's size: the library's svresize).

PATCH.C patch (a diff applied to a file).

MAN.C   man (the manual pages program; MAN.TEXT on TOOLS: is its data).

HEXDUMP.C  hexdump (a hex dump, Intel Hex with -i).

OBJCOPY.C  objcopy (a hex dump or Intel Hex back into a file).

MORE.C, LESS.C  the pagers.

FILES.TEXT lists every file on this volume.
"""

# the demo programs (TCEXTRA:, with examples/ and CMPCODE); the other
# tests/ programs go on TCTESTS:
DEMOS = ('boxes', 'calc', 'demo', 'guess', 'hanoi', 'pi', 'queens', 'sieve')

LIBMODS_ = sorted(f[:-2] for f in os.listdir(os.path.join(ROOT, 'tinyc', 'lib')) if f.endswith('.c'))
LIBS = """; LIBS -- rebuild the C library: CC @TINY-C:/SRC/LIB/LIBS from the shell
; (or X(ecute TINY-C:CC and answer @TINY-C:/SRC/LIB/LIBS).  Compiles every library
; module here (-O: each NAME.OBJ into TINY-C:/OBJ), then joins them into
; TINY-C:/LIB/TCLIB2.OBJ.
-O TINY-C:/OBJ
""" + ''.join('-Z -C %s\n' % m.upper() for m in LIBMODS_) + \
    '-J TINY-C:/LIB/TCLIB2=%s\n' % ','.join(m.upper() for m in LIBMODS_)

BUILD = """; BUILD -- rebuild the Tiny-C compiler: CC @TINY-C:/SRC/CC/BUILD from the shell
; (or X(ecute TINY-C:CC and answer @TINY-C:/SRC/CC/BUILD).  Compiles every module
; here (-O: each NAME.OBJ into TINY-C:/OBJ), then links them into TINY-C:CC2.CODE.
-O TINY-C:/OBJ
""" + ''.join('-C %s\n' % m.upper() for m in MODULES) + \
    '-L TINY-C:CC2=%s\n' % ','.join(m.upper() for m in MODULES)


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
    'dirs.h': 'disks, directories and sub-volumes: volunit, pathdir, wildmatch ...',
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
    'redirect.c': 'freopen; >NAME and <NAME on the command line',
    'strtold.c': 'strtold, atold (<stdlib.h>, P-Code mode)',
    'dirs.c': 'volunit, pathdir, wildmatch, opens by a place ...: disks, directories, sub-volumes (<dirs.h>)',
    'subvol.c': 'svcanon, dirfree, dirkrunch, svresize: KRUNCH, GROW, SHRINK (<dirs.h>)',
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


ROOM = 40                               # a sub-volume's least room to spare, in blocks


class Dir:
    """A volume being made (blocks given) or a sub-volume in it (blocks None: what it holds, and
    room for a quarter as much again as its own files and ROOM blocks more, for edits; GROW
    makes it bigger; the objects a rebuild makes go to OBJ, objdir).  Its files with what they are; sub-volumes are made first, then written into it
    as data files (ucsdvol.Volume.new: the shell's MKDIR)."""
    def __init__(self, name, blocks=None):
        self.name = name.upper()
        self.blocks = blocks
        self.items = []                 # [NAME, bytes or Dir, kind, what]
        self.v = None

    def text(self, name, text, desc):
        self.items.append([name.upper(), ucsdvol.text_to_ucsd(text), 3, desc])

    def textfile(self, name, path, desc=None):
        self.text(name, open(path).read(), desc or describe(path))

    def binary(self, name, data, desc):
        self.items.append([name.upper(), data, 2 if name.upper().endswith('.CODE') else 5, desc])

    def sub(self, name, desc, blocks=None):
        d = Dir(name, blocks)
        self.items.append([d.name, d, 5, desc])
        return d

    def made(self):
        """the ucsdvol.Volume with everything written (once)"""
        if not self.v:
            parts = [(n, r.made().image() if isinstance(r, Dir) else r, k) for n, r, k, d in self.items]
            need = 6 + sum(max(1, (len(r) + 511) // 512) for n, r, k in parts)
            own = sum(max(1, (len(r) + 511) // 512) for n, r, k, d in self.items if not isinstance(r, Dir))
            v = ucsdvol.Volume.new(self.name, self.blocks or need + own // 4 + ROOM)
            for n, r, k in parts:
                v.write(n, r, k)
            self.v = v
        return self.v

    def listing(self, pre=''):
        lines = []
        for n, r, k, d in self.items:
            if isinstance(r, Dir):
                lines.append('  %-24s %6d  %s' % (pre + n + '/', r.made().eov, 'sub-volume: ' + d))
                lines += r.listing(pre + n + '/')
            else:
                lines.append('  %-24s %6d  %s' % (pre + n, max(1, (len(r) + 511) // 512), d))
        return lines

    def finish(self):
        """FILES.TEXT (every file, the sub-volumes' too: written once to learn its own size, then
        with it), the volume saved as build/NAME.BLK, volumes/NAME.zip and NAME.txt"""
        for d in [r for n, r, k, d in self.items if isinstance(r, Dir)]:
            d.made()
        self.items.append(['FILES.TEXT', b'', 3, 'this list'])
        for _ in range(2):
            listing = '\n'.join(['%s:  every file, the sub-volumes\' too (A/NAME: NAME in the sub-volume A)' % self.name, '',
                                 '  %-24s %6s  %s' % ('FILE', 'BLOCKS', 'WHAT'), ''] + self.listing()) + '\n'
            self.items[-1][1] = ucsdvol.text_to_ucsd(listing)
        path = os.path.join(ROOT, 'build', self.name + '.BLK')
        open(path, 'wb').write(self.made().image())
        os.makedirs(OUT, exist_ok=True)
        open(os.path.join(OUT, self.name + '.txt'), 'w').write(listing)
        with zipfile.ZipFile(os.path.join(OUT, self.name + '.zip'), 'w', zipfile.ZIP_DEFLATED) as z:
            z.write(path, self.name + '.BLK')
        print('%s: %d files' % (self.name, len(self.listing())))


Vol = Dir


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
    """TINY-C: the compiler, its headers and library, and their sources"""
    t = Dir('TINY-C', 4000)
    code, log = build()
    t.binary('CC.CODE', open(code, 'rb').read(), 'the Tiny-C compiler: CC from the shell, X(ecute TINY-C:CC')
    t.textfile('TCMSGS.TEXT', os.path.join(INC, 'tcmsgs.txt'), "the compiler's messages (line n = message n)")
    t.text('README.TEXT', README_TC, 'how to use Tiny-C, and what is where')
    t.text('ALL.TEXT', ALL, 'CC @TINY-C:ALL: rebuilds the compiler, the library and every tool')
    inc = t.sub('INCLUDE', 'the headers (#include <stdio.h> finds them here)')
    for f in sorted(os.listdir(INC)):
        if f.endswith('.h'):
            inc.textfile(f.upper(), os.path.join(INC, f), describe(os.path.join(INC, f)))
    tclib = open(build_lib(True), 'rb').read()
    lib = t.sub('LIB', 'the C library', 6 + 2 * ((len(tclib) + 511) // 512) + ROOM)  # (and TCLIB2.OBJ, LIBS)
    lib.binary('TCLIB.OBJ', tclib, 'the C library (built with -Z), linked into every program')
    src = t.sub('SRC', 'the sources of the compiler and the library, and how to rebuild them')
    src.text('README.TEXT', README_SRC, 'how to rebuild Tiny-C and the tools')
    cc = src.sub('CC', 'the compiler: its modules and BUILD.TEXT')
    cc.text('BUILD.TEXT', BUILD, 'CC @TINY-C:/SRC/CC/BUILD: rebuilds the compiler -> TINY-C:CC2.CODE')
    cc.textfile('OBJFORMAT.TEXT', os.path.join(SRC, 'OBJFORMAT.TEXT'), 'the .OBJ file format: what CC writes, how the linker reads it')
    for m in MODULES:
        cc.textfile(m.upper() + '.C', os.path.join(SRC, m + '.c'))
    for h in ('tc.h', 'parse.h', 'gen.h'):
        cc.textfile(h.upper(), os.path.join(SRC, h))
    lb = src.sub('LIB', 'the library: its modules and LIBS.TEXT')
    lb.text('LIBS.TEXT', LIBS, 'CC @TINY-C:/SRC/LIB/LIBS: rebuilds the library -> TINY-C:/LIB/TCLIB2.OBJ')
    for m in LIBMODS:
        lb.textfile(m.upper() + '.C', os.path.join(LIB, m + '.c'))
    lb.textfile('LIBINT.H', os.path.join(LIB, 'libint.h'))
    objdir(t, [os.path.join(os.path.dirname(code), m + '.obj') for m in MODULES] +
           [os.path.join(ROOT, 'build', 'libz', m + '.obj') for m in LIBMODS],
           "empty: the objects BUILD and LIBS make (-O TINY-C:/OBJ)")
    t.finish()


def objdir(vol, objs, desc):
    """the sub-volume OBJ, empty: room for the objects a rebuild makes (the host's, objs) twice
    over -- a new one is written before the old one goes -- and ROOM blocks more"""
    need = sum((os.path.getsize(o) + 511) // 512 for o in objs)
    vol.sub('OBJ', desc, 6 + 2 * need + ROOM)


def programs(vol, batch, progs, desc):
    """sources, ready-to-run code files and the @BATCH file that rebuilds them"""
    names = [os.path.splitext(os.path.basename(p))[0].upper()[:10] for p in progs]
    vol.text(batch + '.TEXT', '; %s -- compile and link every program on %s:\n'
             '; prefix %s:, X(ecute TINY-C:CC, answer @%s\n' % (batch, vol.name, vol.name, batch) +
             ''.join('-Z %s\n' % n for n in names), desc)
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


ALL = """; ALL -- rebuild the compiler, the library and every tool (TOOLS: on line too):
; CC @TINY-C:ALL from the shell, or X(ecute TINY-C:CC and answer @TINY-C:ALL
@TINY-C:/SRC/CC/BUILD
@TINY-C:/SRC/LIB/LIBS
@TOOLS:/SRC/TOOLS
"""


def tool_batch(name, mods):
    """TOOLS.TEXT's lines for a tool: each module in its sub-volume compiled (-Z -C), then linked"""
    names = ['%s/%s' % (name, os.path.splitext(os.path.basename(m))[0].upper()) for m in mods]
    return ''.join('-Z -C %s\n' % n for n in names) + '-L TOOLS:%s=%s\n' % (name, ','.join(names))


def tools():
    """TOOLS: the tools ready to run, their manual pages, and their sources (a sub-volume each)"""
    t = Dir('TOOLS', 4000)
    t.text('README.TEXT', README_TOOLS, 'what is on this volume')
    os.makedirs(TMP, exist_ok=True)
    for name, mods, hdrs, desc in TOOLS:
        code = compile_modules([os.path.join(ROOT, m) for m in mods], os.path.join(TMP, name + '.CODE'), True)
        t.binary(name + '.CODE', open(code, 'rb').read(), desc)
    # the manual pages, one file next to MAN.CODE: every program on this volume needs a page
    man = open(os.path.join(ROOT, 'ports', 'man', 'MAN.TEXT')).read()
    pages = {n for line in man.split('\n') if line.startswith('%% ') for n in line[3:].upper().split()}
    missing = [name for name, mods, hdrs, desc in TOOLS if name not in pages]
    if missing:
        raise SystemExit('ports/man/MAN.TEXT has no page (%%%% NAME) for: %s' % ', '.join(missing))
    t.text('MAN.TEXT', man, 'the manual pages MAN shows (one %% NAME entry a program: edit and add here)')
    s = t.sub('SRC', "the tools' sources (a sub-volume each) and TOOLS.TEXT")
    s.text('README.TEXT', README_TOOLS_SRC, "how to rebuild the tools")
    s.text('TOOLS.TEXT', '; TOOLS -- compile and link every tool onto TOOLS:\n'
           '; CC @TOOLS:/SRC/TOOLS from the shell, or X(ecute TINY-C:CC and answer @TOOLS:/SRC/TOOLS\n'
           '; (-O: the objects into TOOLS:/OBJ)\n-O TOOLS:/OBJ\n' +
           ''.join(tool_batch(name, mods) for name, mods, hdrs, desc in TOOLS),
           'CC @TOOLS:/SRC/TOOLS: rebuilds every tool')
    for name, mods, hdrs, desc in TOOLS:
        d = s.sub(name, name.lower() + "'s source")
        for f in mods + hdrs:
            d.textfile(os.path.basename(f).upper(), os.path.join(ROOT, f))
    objdir(t, [os.path.join(TMP, os.path.splitext(os.path.basename(m))[0] + '.obj')
               for name, mods, hdrs, desc in TOOLS for m in mods],
           "empty: the objects TOOLS.TEXT makes (-O TOOLS:/OBJ)")
    t.finish()


def main():
    tiny_c()
    extras()
    tests()
    tools()
    old = os.path.join(ROOT, 'TinyC_Volume.zip')
    if os.path.exists(old):
        os.remove(old)


if __name__ == '__main__':
    main()
