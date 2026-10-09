#!/usr/bin/env python3
"""pexectest.py -- test pexec() (psys.h) and the operating system's part of
it (BIGGY 1.10; the $ part BIGGY 1.11) with the mini-shell examples/shell.c:

  X MEMFREE                  free memory of a program started by the OS
  X SHELL, then in the shell
    MEMFREE                  the same program started by pexec: exactly as
                             much free memory (nothing of the shell is left),
                             exit status 7 back in the shell
    mem                      the shell's own free memory (less: its code)
    NOSUCH                   "no such program"
    CRASH                    an execution error: the system re-initializes,
                             the shell comes back with status -2
    *SYSTEM.FILER.           a Pascal program (status 0)
    ARGS ...                 examples/args.c: main(argc, argv) gets the
                             command line (none, ADD 2 3, MUL 6 7, REPEAT,
                             ECHO with extra blanks, unknown, 81 characters)
    args add 2 3             no volume: found on the one disk that has it
    WHERE ECHO               WHERE (a copy of ARGS) on five disks (#5, #9,
                             #10, #11, #12): the shell lists them, the key
                             4 (no RETURN) runs #11's (argv[0] says so),
                             another key runs nothing
    cd, dir #10              the prefix (the boot volume); unit 10's files
    cd #5, dir *.code        the prefix becomes WORK:; its code files
    DIR disk11:w?ere.=       a volume and a pattern (? and = wildcards)
    type notes.text          a text file on the prefix volume
    type #5:args.code        not a text file
    type tmp?.text           two files, each under its name
    type nosuch.c            no such file
    delete tmp*.text, N      lists them, asks: N keeps them
    DELETE work:tmp=.text, Y deletes them
    del keep.text            no wildcards: deleted without asking
    dir *.text               NOTES.TEXT is the only text file left
    whereis where.code       on all five disks
    WHEREIS #10:*            a volume given: there only
    whereis nosuch.x         on no disk
    volumes                  every disk: unit, name, files, blocks used
    copy hello.c #10:        to another disk: a text file (.C) stays text
    copy *:system.co* #10:    its date and kind stay
    COPY hello.c #10         again: the old copy replaced
    copy hello.c hello2.c    a new name on the same disk
    move hello2.c #11:       to another disk: copied, then deleted here
    move #11:... #11:...     on its own disk: renamed
    rename #10:hello.c bye.c, rename notes.text args.code (there already)
    copy *.code #12:x.code   several files to one name: refused
    type #10:bye.c, dir #10:, dir #11:, dir *.c   the result
    dir *:system.co*          the system volume's SYSTEM.COMPILER (dated)
    dir nosuch:, cd 13       no such disk
    bye                      back to the Command: prompt
  X ARGS                     no volume: found on the prefix (WORK:, set by
                             cd #5), started by the OS: no arguments
  ?, $                       the Command: prompt's $ starts the shell
                             (*SYSTEM.SHELL), MEMFREE from it, then the
                             command line (keys as emulator 2.00 sends them):
    Up Up Enter              #4:SYSTEM.CMDS outlasts the shell: cd 13 again
    ...ECHO abc Left Left Delete X   edited in the middle: aXc
    Up Enter                 the history after pexec restarted the shell
    XRGS ECHO ow Home Insert A End " z"   type over, then at the end
    junk ESC ...ECHO esc     ESC clears the line
    Up Up Down Enter         back and forth: ECHO esc again
    ...ECHO bsx Backspace    bs
    bye

Uses the mode of PSYS_MODE (native or z80) like the other tools.
"""
import os, sys, re
HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
sys.path.insert(0, HERE)
from psys import PSystem
import ucsdvol
from tcrun import compile_c

CRASH = '''#include <stdio.h>
int zero;
int main(void) { printf("crash: dividing by zero\\n"); return 5 / zero; }
'''

SCRIPT = r'''WAIT "Command:"
TYPE "X"
WAIT "Execute what file?"
TYPE "#5:MEMFREE\r"
WAIT "Command:"
TYPE "X"
WAIT "Execute what file?"
TYPE "#5:SHELL\r"
WAIT "shell> "
TYPE "#5:MEMFREE\r"
WAIT "shell> "
TYPE "mem\r"
WAIT "shell> "
TYPE "NOSUCH\r"
WAIT "shell> "
TYPE "#5:CRASH\r"
WAIT "continue"
TYPE " "
WAIT "shell> "
TYPE "*SYSTEM.FILER.\r"
WAIT "Filer:"
TYPE "Q"
WAIT "shell> "
TYPE "#5:ARGS\r"
WAIT "shell> "
TYPE "#5:ARGS ADD 2 3\r"
WAIT "shell> "
TYPE "#5:ARGS mul 6 7\r"
WAIT "shell> "
TYPE "#5:ARGS REPEAT 2 HELLO\r"
WAIT "shell> "
TYPE "  #5:ARGS   ECHO  A   B  \r"
WAIT "shell> "
TYPE "#5:ARGS FOO\r"
WAIT "shell> "
TYPE "#5:ARGS ECHO LLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLL\r"
WAIT "shell> "
TYPE "args add 2 3\r"
WAIT "shell> "
TYPE "WHERE ECHO\r"
WAIT "Which one"
TYPE "4"
WAIT "shell> "
TYPE "WHERE ECHO\r"
WAIT "Which one"
TYPE "x"
WAIT "shell> "
TYPE "cd\r"
WAIT "shell> "
TYPE "dir #10\r"
WAIT "shell> "
TYPE "cd #5\r"
WAIT "shell> "
TYPE "dir *.code\r"
WAIT "shell> "
TYPE "DIR disk11:w?ere.=\r"
WAIT "shell> "
TYPE "type notes.text\r"
WAIT "shell> "
TYPE "type #5:args.code\r"
WAIT "shell> "
TYPE "type tmp?.text\r"
WAIT "shell> "
TYPE "type nosuch.c\r"
WAIT "shell> "
TYPE "delete tmp*.text\r"
WAIT "(Y/N)?"
TYPE "n"
WAIT "shell> "
TYPE "DELETE work:tmp=.text\r"
WAIT "(Y/N)?"
TYPE "Y"
WAIT "shell> "
TYPE "del keep.text\r"
WAIT "shell> "
TYPE "dir *.text\r"
WAIT "shell> "
TYPE "whereis where.code\r"
WAIT "shell> "
TYPE "WHEREIS #10:*\r"
WAIT "shell> "
TYPE "whereis nosuch.x\r"
WAIT "shell> "
TYPE "volumes\r"
WAIT "shell> "
TYPE "copy hello.c #10:\r"
WAIT "shell> "
TYPE "copy *:system.co* #10:\r"
WAIT "shell> "
TYPE "COPY hello.c #10\r"
WAIT "shell> "
TYPE "copy hello.c hello2.c\r"
WAIT "shell> "
TYPE "move hello2.c #11:\r"
WAIT "shell> "
TYPE "move #11:hello2.c #11:hello3.c\r"
WAIT "shell> "
TYPE "rename #10:hello.c bye.c\r"
WAIT "shell> "
TYPE "rename notes.text args.code\r"
WAIT "shell> "
TYPE "copy *.code #12:x.code\r"
WAIT "shell> "
TYPE "type #10:bye.c\r"
WAIT "shell> "
TYPE "dir #10:\r"
WAIT "shell> "
TYPE "dir #11:\r"
WAIT "shell> "
TYPE "dir *.c\r"
WAIT "shell> "
TYPE "dir *:system.co*\r"
WAIT "shell> "
TYPE "dir nosuch:\r"
WAIT "shell> "
TYPE "cd 13\r"
WAIT "shell> "
TYPE "bye\r"
WAIT "Command:"
TYPE "X"
WAIT "Execute what file?"
TYPE "ARGS\r"
WAIT "Command:"
TYPE "?"
WAIT "$(hell"
TYPE "$"
WAIT "shell> "
TYPE "#5:MEMFREE\r"
WAIT "shell> "
TYPE "\x14\x14\r"
WAIT "shell> "
TYPE "#5:ARGS ECHO abc\x11\x11\x87X\r"
WAIT "shell> "
TYPE "\x14\r"
WAIT "shell> "
TYPE "XRGS ECHO ow\x84\x86A\x85 z\r"
WAIT "shell> "
TYPE "junk\x1b#5:ARGS ECHO esc\r"
WAIT "shell> "
TYPE "\x14\x14\x12\r"
WAIT "shell> "
TYPE "#5:ARGS ECHO bsx\x08\r"
WAIT "shell> "
TYPE "bye\r"
WAIT "Command:"
'''


def main():
    ps = PSystem()
    crash = os.path.join(ps.dir, 'crash.c')
    open(crash, 'w').write(CRASH)
    for src in (os.path.join(ROOT, 'examples', 'shell.c'), os.path.join(ROOT, 'examples', 'memfree.c'),
                os.path.join(ROOT, 'examples', 'args.c'), crash):
        base, code = compile_c(src, ps.dir)
        ps.put(base + '.CODE', open(code, 'rb').read())
    # WHERE (ARGS under another name) on five disks: #5, #9 and three more
    where = open(os.path.join(ps.dir, 'ARGS.CODE'), 'rb').read()
    ps.put('WHERE.CODE', where)
    ps.put('NOTES.TEXT', 'not a code file\nits second line\n')     # dir *.code leaves it out
    for name, text in (('TMP1.TEXT', 'tmp one\n'), ('TMP2.TEXT', 'tmp two\n'), ('KEEP.TEXT', 'keep me\n'),
                       ('HELLO.C', 'int hello;\n/* copied */\n')):
        ps.put(name, text)
    disks = [ps.spare]
    for u in (10, 11, 12):
        path = os.path.join(ps.dir, 'DISK%d.BLK' % u)
        ucsdvol.main(['new', path, 'DISK%d' % u, '400'])
        os.environ['VERIFY_UNIT%d' % u] = path
        disks.append(path)
    for path in disks:
        v = ucsdvol.Volume(path)
        v.write('WHERE.CODE', where, 2)
        v.save()
    ok, tr, info = ps.run_script(SCRIPT, 600)
    tr = tr.replace('\r', '\n')

    def part(cmd):
        """what the shell printed for cmd (up to its next prompt)"""
        i = tr.find('shell> ' + cmd + '\n')
        if i < 0:
            return ''
        i += 7 + len(cmd)
        # (a line ends in CR LF or CR alone: no empty lines)
        return re.sub(r'\n+', '\n', tr[i:tr.find('shell> ', i)])
    free = [int(x) for x in re.findall(r'memfree: (\d+) words free', tr)]
    shell = re.findall(r'shell: (\d+) words free', tr)
    status = re.findall(r'\[exit status (-?\d+)\]', tr)
    print('\n'.join(l for l in tr.split('\n') if re.search(r'free|status|no such|re-init|args:|too long|prefix|files|\(#\d+\)$|  Code|  Text|no disk|^ ?#\d|Unit  Vol|whereis', l)))
    checks = [
        ('the script completed', ok),
        ('memfree ran three times', len(free) == 3),
        ('same free memory from X(ecute and from the shell', len(free) == 3 and len(set(free)) == 1),
        ('the shell has less (its own code)', len(free) == 3 and len(shell) == 1 and int(shell[0]) < free[0]),
        ('exit statuses 7, -2 (execution error), 0 (the Filer), 1 5 42 0 4 2 (ARGS), 5 2 (searched), 7 (from $)',
         status[:12] == ['7', '-2', '0', '1', '5', '42', '0', '4', '2', '5', '2', '7']),
        ('Up Up Enter in a new shell: cd 13 from #4:SYSTEM.CMDS', tr.count('cd: no disk 13 on line') == 2),
        ('Left Left Delete X: aXc, and Up Enter runs it again', tr.count('argv[2] = "aXc"') == 2),
        ('Home Insert (type over) End: ARGS ECHO ow z', 'argv[2] = "ow"' in tr and 'argv[3] = "z"' in tr),
        ('ESC cleared the line (no junk: no such program); Up Up Down: esc again', tr.count('argv[2] = "esc"') == 2
         and 'no such program' not in tr[tr.rfind('$(hell'):]),
        ('Backspace: bs', 'argv[2] = "bs"' in tr),
        ('WHERE on five disks: all listed', all(('%s:WHERE  (#%d:WHERE)' % (v, u)) in tr for v, u in
         (('WORK', 5), ('SPARE', 9), ('DISK10', 10), ('DISK11', 11), ('DISK12', 12)))),
        ('key 4, no RETURN: #11 ran', 'argv[0] = "#11:WHERE"' in tr and tr.count('args: argc 2') == 1),
        ('ARGS with no arguments: argc 1 (shell, then X(ecute)', tr.count('args: no arguments (argc 1)') == 2),
        ('ARGS ADD 2 3, mul 6 7', 'args: 2 + 3 = 5' in tr and 'args: 6 * 7 = 42' in tr),
        ('ARGS REPEAT 2 HELLO', 'args: 1 HELLO\n' in tr and 'args: 2 HELLO\n' in tr),
        ('ARGS ECHO: words, extra blanks dropped', 'args: argc 4' in tr and 'argv[0] = "#5:ARGS"' in tr
         and 'argv[1] = "ECHO"' in tr and 'argv[2] = "A"' in tr and 'argv[3] = "B"' in tr),
        ('ARGS FOO: unknown', "args: don't know FOO with 1 arguments" in tr),
        ('81 characters: too long', 'command line too long' in tr),
        ('the ? prompt offers $(hell', 'H(alt, $(hell' in tr),
        ('NOSUCH: no such program', 'NOSUCH: no such program' in tr),
        ('cd: the prefix is the boot volume, unit 4', re.search(r'prefix is \w+: \(#4\)', tr) is not None),
        ('dir #10: DISK10\'s one file', 'DISK10: (#10)' in tr and re.search(r'WHERE\.CODE +\d+ .*Code', tr) is not None
         and '1 of 1 files' in tr),
        ('cd #5: the prefix is WORK:', 'prefix is WORK: (#5)' in tr),
        ('dir *.code: the 5 code files of WORK:, not the text files', '5 of 10 files' in part('dir *.code')
         and '.TEXT' not in part('dir *.code')
         and all(re.search(r'%s\.CODE +\d+ ' % n, tr) for n in ('SHELL', 'MEMFREE', 'ARGS', 'CRASH'))),
        ('type notes.text: its two lines', 'not a code file\nits second line\n' in part('type notes.text')),
        ('type #5:args.code: not a text file', 'ARGS.CODE: not a text file' in tr),
        ('type tmp?.text: both, under their names', re.search(r'--- WORK:TMP1\.TEXT\ntmp one\n--- WORK:TMP2\.TEXT\ntmp two\n',
                                                         part('type tmp?.text')) is not None),
        ('type nosuch.c: no such file', 'type: no file nosuch.c on WORK:' in tr),
        ('delete tmp*.text, N: listed, asked, kept', 'Delete these 2 files (Y/N)? n' in tr
         and 'deleted' not in part('delete tmp*.text')),
        ('DELETE work:tmp=.text, Y: both deleted', 'WORK:TMP1.TEXT deleted' in tr and 'WORK:TMP2.TEXT deleted' in tr),
        ('del keep.text: deleted, not asked', 'WORK:KEEP.TEXT deleted' in part('del keep.text') and 'Y/N' not in part('del keep.text')),
        ('whereis where.code: on all five disks', '5 files on 5 volumes' in part('whereis where.code')
         and all(re.search(r'#%d +%s: +WHERE\.CODE +\d+ ' % (u, v), part('whereis where.code')) for v, u in
                 (('WORK', 5), ('SPARE', 9), ('DISK10', 10), ('DISK11', 11), ('DISK12', 12)))),
        ('WHEREIS #10:*: DISK10: only', '1 file on 1 volume' in part('WHEREIS #10:*') and 'DISK11' not in part('WHEREIS #10:*')),
        ('whereis nosuch.x: on no disk', 'whereis: no file nosuch.x on any disk on line' in tr),
        ('volumes: units, names, files, blocks; boot and prefix marked',
         re.search(r'#4 +BIGGY: +\d+ +\d+ of +\d+  \(boot\)', part('volumes')) is not None
         and re.search(r'#5 +WORK: +7 +\d+ of +\d+  \(prefix\)', part('volumes')) is not None
         and re.search(r'#10 +DISK10: +1 +\d+ of +400\n', part('volumes')) is not None),
        ('copy hello.c #10:', 'WORK:HELLO.C -> DISK10:HELLO.C\n' in part('copy hello.c #10:')),
        ('copy *:system.co* #10:', 'BIGGY:SYSTEM.COMPILER -> DISK10:SYSTEM.COMPILER' in part('copy *:system.co* #10:')),
        ('COPY hello.c #10: replaced', 'DISK10:HELLO.C (replaced)' in part('COPY hello.c #10')),
        ('copy hello.c hello2.c', 'WORK:HELLO.C -> WORK:HELLO2.C' in part('copy hello.c hello2.c')),
        ('move hello2.c #11:', 'WORK:HELLO2.C -> DISK11:HELLO2.C' in part('move hello2.c #11:')),
        ('move on its own disk: renamed', 'DISK11:HELLO2.C -> DISK11:HELLO3.C' in part('move #11:hello2.c #11:hello3.c')),
        ('rename #10:hello.c bye.c', 'DISK10:HELLO.C -> DISK10:BYE.C' in part('rename #10:hello.c bye.c')),
        ('rename onto an existing name: refused', 'WORK:ARGS.CODE is there already' in part('rename notes.text args.code')),
        ('several files to one name: refused', 'copy: 5 files: to a volume' in part('copy *.code #12:x.code')),
        ('the copy reads the same', 'int hello;\n/* copied */\n' in part('type #10:bye.c')),
        ('dir #10:: BYE.C text, SYSTEM.COMPILER code and dated', re.search(r'BYE\.C +\d+ +Text', part('dir #10:')) is not None
         and re.search(r'SYSTEM\.COMPILER +69 +\d+-Feb-79  Code', part('dir #10:')) is not None and '3 of 3 files' in part('dir #10:')),
        ('dir #11:: HELLO3.C (text), WHERE.CODE', re.search(r'HELLO3\.C +\d+ +Text', part('dir #11:')) is not None
         and '2 of 2 files' in part('dir #11:')),
        ('dir *.c: HELLO.C still on WORK:, HELLO2.C moved away', 'HELLO.C' in part('dir *.c') and 'HELLO2' not in part('dir *.c')
         and '1 of 7 files' in part('dir *.c')),
        ('dir *.text: only NOTES.TEXT left', 'NOTES.TEXT' in part('dir *.text') and '1 of 7 files' in part('dir *.text')),
        ('DIR disk11:w?ere.=: WHERE.CODE', 'DISK11: (#11)' in tr and tr.count('1 of 1 files') == 2),
        ('dir *:system.co*: SYSTEM.COMPILER, dated', re.search(r'SYSTEM\.COMPILER +\d+ +\d+-[A-Z][a-z][a-z]-\d\d  Code', tr) is not None
         and '1 of ' in tr.split('SYSTEM.COMPILER')[-1]),
        ('dir nosuch:, cd 13: no such disk', 'dir: no disk nosuch on line' in tr and 'cd: no disk 13 on line' in tr),
    ]
    bad = [name for name, good in checks if not good]
    if bad:                             # the $ shell's part, as it was on the screen
        print(repr(tr[tr.rfind('$(hell'):]))
    for name, good in checks:
        print('%-5s %s' % ('ok' if good else 'FAIL', name))
    print('pexec test: %s   (work: %s)' % ('FAILED' if bad else 'PASSED', ps.dir))
    return 1 if bad else 0


if __name__ == '__main__':
    sys.exit(main())
