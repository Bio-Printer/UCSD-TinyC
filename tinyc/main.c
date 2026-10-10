/* main.c -- the Tiny-C driver: preprocess, compile, generate, link.
 *
 * Host:      tc [-c] [-z] [-I dir] [-L lib.obj] [-o out] file.c ... file.obj ...
 *            Each .c becomes a .obj (temporaries .i and .ir beside it);
 *            without -c everything is linked with the library (-L, or
 *            tclib.obj in the include directory) into a .code file.
 *            -z: calls through function pointers for the Z80 interpreter
 *            (see below); for Z80 mode every object linked, the library too,
 *            must be built with it.
 * P-System:  X(ecute CC, then answer "Compile what file?" with
 *              NAME             compile NAME.C (or NAME.TEXT), link with TCLIB.OBJ -> NAME.CODE
 *              /C NAME          compile only -> NAME.OBJ
 *              /L OUT=A,B,...   link A.OBJ, B.OBJ ... and TCLIB.OBJ -> OUT.CODE
 *              /J OUT=A,B,...   join A.OBJ, B.OBJ ... into the library OUT.OBJ
 *              /Z NAME, /Z /C NAME   as above with -z (see below)
 *              @FILE            run the commands in FILE.TEXT, one per line
 *                               (blank lines and lines starting ';' skipped),
 *                               stopping at the first that fails
 *            or give the commands as arguments (from the shell, pexec):
 *              CC @BUILD @LIBS, CC /Z HANOI SIEVE: options and the word after
 *              them make one command; they run in turn, no prompt
 *
 * Calls through function pointers: by default they use CSP 138 (CALLI), which
 * the native P-Code engine implements and the Z80 interpreter does not. -z (/Z)
 * generates the older sequence that stores the function value into the operands
 * of a CXP at run time, for programs that must run in Z80 mode (and, as the
 * library is linked into every program, a library built the same way).
 */
#include "tc.h"
#ifdef __TINYC__
#include <psys.h>
#endif
#pragma segment MAIN

#define MAXFILES 24

static void basename8(char *path, char *out)
{
    char *s;
    char *b;
    int n;
    b = path;
    for (s = path; *s; s++)
        if (*s == '/' || *s == '\\' || *s == ':')
            b = s + 1;
    n = 0;
    while (b[n] && b[n] != '.' && n < 8) {
        out[n] = b[n] >= 'a' && b[n] <= 'z' ? b[n] - 32 : b[n];
        n++;
    }
    out[n] = 0;
}

/* every pass gives its heap back: on the P-System memory is released
   to where it was before the pass (the stdio files are all closed).
   Its "words free" is the least free memory during the pass (memleast,
   psys.h: emulator 1.99), else the free memory at its end (MEMAVAIL). */
static void passbegin(int xsize)
{
    curfile = 0;
#ifdef __TINYC__
    __heapsave();
    memleast_start();
#endif
    xsetsize(xsize);
}

static void passend(void)
{
#ifdef __TINYC__
    int least;
    least = memleast();
    memleast_stop();
    say("  (");
    sayn(least >= 0 ? least : __cspi(40));
    say(" words free) ");
    __heaprestore();
#endif
    resetpools();
}

/* one source file to one object file */
static int compileone(char *src, char *tmpi, char *tmpr, char *obj)
{
    char mod[10];
    basename8(src, mod);
    say("Preprocessing ");
    say(src);
    say(" ");
    passbegin(8000);
    if (!preprocess(src, tmpi))
        return 0;
    passend();
    say("  Compiling ");
    passbegin(1600);
    if (!compile(tmpi, tmpr, mod) || !compileend())
        return 0;
    passend();
    say("Generating code ");
    say(obj);
    say(" ");
    passbegin(1900);
    if (!gencode(tmpr, obj))
        return 0;
    passend();
    say("\n");
    return 1;
}

/* The linker's commands are in its segment: it is in memory while it
   links, not while a source compiles (this is 600 bytes of every pass). */
#pragma segment LINK
static int linkall(char **objs, int n, char *out)
{
    char prog[10];
    int r;
    basename8(out, prog);
    say("Linking ");
    say(out);
    say("\n");
    passbegin(1000);
    r = link(objs, n, out, prog);
    passend();
    say("\n");
    return r;
}

#pragma segment MAIN

/* A file opened outside a pass is opened inside a heap mark of its own:
   fclose would put its buffer (about 300 words) on the free list below
   the next pass's mark, where it would stay unused through that pass. */
static int exists(char *name)
{
    FILE *f;
#ifdef __TINYC__
    __heapsave();
#endif
    f = fopen(name, "rb");
    if (f)
        fclose(f);
#ifdef __TINYC__
    __heaprestore();
#endif
    return f != 0;
}

static void upper(char *s)
{
    for (; *s; s++)
        if (*s >= 'a' && *s <= 'z')
            *s = *s - 32;
}

#ifdef __TINYC__
/* one command (see the top of this file); 0 = it failed */
/* /L OUT=A,B,... or /J LIB=A,B,...: its own function, so that the objects'
   names (on the stack: gone with the command -- on the heap they outlived
   it and @ALL ran out of memory) take no room while a source compiles */
#pragma segment LINK
static int linkcmd(char *s, char *lib, int from)
{
    char *objs[MAXFILES];
    char names[360];
    char src[25];
    char obj[25];
    char out[25];
    char *t;
    int used;
    int nobjs;
    int n;
    int joining;
    nobjs = 0;
    used = 0;
    /* /L OUT=A,B,...  or  /J OUT=A,B,... */
    joining = s[1] == 'J';
    s = s + 2;
    while (*s == ' ')
        s++;
    t = strchr(s, '=');
    if (!t) {
        say("use: /L OUT=A,B,...  or  /J LIB=A,B,...\n");
        return 0;
    }
    *t++ = 0;
    strcpy(out, s);
    strcat(out, joining ? ".OBJ" : ".CODE");
    while (*t) {
        s = t;
        while (*t && *t != ',')
            t++;
        if (*t)
            *t++ = 0;
        while (*s == ' ')
            s++;
        if (!*s)
            continue;
        if (nobjs >= MAXFILES - 1)
            break;
        /* NAME.OBJ: with a volume there, else where findfile says (the
           batch file's disk, where /C put it, or the prefix) */
        strcpy(src, s);
        strcat(src, ".OBJ");
        if (!strchr(s, ':') && s[0] != '*' && strlen(src) <= 15) {
            n = findfile(src, "", from, obj, &n);
            if (n < 0)
                return 0;           /* on several disks: findfile said so */
            if (n > 0)
                strcpy(src, obj);
        }
        n = strlen(src) + 1;
        if (used + n > (int)sizeof names) {
            say("too many objects\n");
            return 0;
        }
        objs[nobjs] = names + used;
        strcpy(objs[nobjs], src);
        used = used + n;
        nobjs++;
    }
    if (joining) {
        say("Joining ");
        say(out);
        say("\n");
#ifdef __TINYC__
        __heapsave();               /* the files' buffers: see exists */
#endif
        n = join(objs, nobjs, out);
#ifdef __TINYC__
        __heaprestore();
#endif
        return n;
    }
    if (lib[0])                         /* found once, at the start (see main) */
        objs[nobjs++] = lib;
    return linkall(objs, nobjs, out);
}

#pragma segment MAIN

/* from: the unit of the @batch file the command is in (0: typed) */
static int command(char *s, char *lib, int from)
{
    char *objs[2];                      /* the object and the library */
    char src[25];                       /* UCSD file names are short */
    char alt[16];
    char obj[25];
    char out[25];
    char *t;
    int nobjs;
    int n;
    int conly;
    nobjs = 0;
    z80calls = 0;                       /* /Z applies to this command only */
    while (*s == ' ')
        s++;
    if (s[0] == '/' && s[1] == 'Z') {
        z80calls = 1;
        s = s + 2;
        while (*s == ' ')
            s++;
    }
    if (s[0] == '/' && (s[1] == 'L' || s[1] == 'J'))
        return linkcmd(s, lib, from);
    {
        conly = 0;
        if (s[0] == '/' && s[1] == 'C') {
            conly = 1;
            s = s + 2;
            while (*s == ' ')
                s++;
        }
        /* NAME or NAME.C: the source is NAME.C, or NAME.TEXT when there
           is no NAME.C.  With a volume (VOL:, #n:, *): there.  Without:
           on whichever disk findfile says (pp.c) -- the batch file's or
           the prefix volume when several have it. */
        n = strlen(s);
        if (n > 5 && strcmp(s + n - 5, ".TEXT") == 0)
            s[n = n - 5] = 0;
        if (n > 2 && strcmp(s + n - 2, ".C") == 0)
            s[n - 2] = 0;
        strcpy(src, s);
        strcat(src, ".C");
        if (strchr(s, ':') || s[0] == '*') {
            if (!exists(src)) {
                strcpy(src, s);
                strcat(src, ".TEXT");
            }
        } else if (n <= 13) {
            alt[0] = 0;
            if (n <= 10) {
                strcpy(alt, s);
                strcat(alt, ".TEXT");
            }
            strcpy(obj, src);
            n = findfile(obj, alt, from, src, &n);
            if (n < 0)
                return 0;               /* on several disks: findfile said so */
        }
        /* NAME.OBJ goes on its source's volume; NAME.CODE on the prefix */
        obj[0] = 0;
        t = strchr(src, ':');
        if (t && !strchr(s, ':') && s[0] != '*') {
            n = t - src + 1;
            memcpy(obj, src, n);
            obj[n] = 0;
        }
        strcat(obj, s);
        strcat(obj, ".OBJ");
        strcpy(out, s);
        strcat(out, ".CODE");
        if (!compileone(src, "TCTEMP.TEXT", "TCTEMP.IR", obj))
            return 0;
        if (conly)
            return 1;
        objs[nobjs++] = obj;
    }
    if (lib[0])                         /* found once, at the start (see main) */
        objs[nobjs++] = lib;
    return linkall(objs, nobjs, out);
}

/* @FILE: the commands in FILE.TEXT.  The file is not kept open (its
   buffer would take memory from every pass): before each command it is
   opened, the lines already done are skipped, the next one is read and
   the file is closed again.  line is the caller's command buffer
   (BATCHLINE bytes), which name may point into: name is copied first,
   and no buffer of its own stays on the stack through every pass.
   A line @OTHER runs OTHER.TEXT, then this file goes on (it is read
   afresh for each line anyway); the sources and objects its commands
   name are looked for on its own disk first. */
#define BATCHLINE 150
static int batch(char *name, char *lib, char *line)
{
    FILE *f;
    char path[25];
    int done;
    int k;
    int n;
    char found[25];
    int from;
    strcpy(path, name);
    n = strlen(path);
    if (n <= 5 || strcmp(path + n - 5, ".TEXT") != 0)
        strcat(path, ".TEXT");
    /* FILE.TEXT: on its volume if it names one, else as findfile says;
       the sources its commands name are looked for on its disk first */
    if (strchr(path, ':') || path[0] == '*')
        from = fileunit(path);
    else {
        from = 0;
        n = findfile(path, "", 0, found, &from);
        if (n < 0)
            return 0;
        if (n > 0)
            strcpy(path, found);
    }
    for (done = 0;; done++) {
        __heapsave();                       /* the file's buffer: see exists */
        f = fopen(path, "r");
        k = 0;
        if (f) {
            for (; k <= done; k++)
                if (!fgets(line, BATCHLINE, f))
                    break;
            fclose(f);
        }
        __heaprestore();
        if (!f) {
            say("cannot open ");
            say(path);
            say("\n");
            return 0;
        }
        if (k <= done)
            return 1;                       /* the end of the file */
        n = strlen(line);
        while (n > 0 && (line[n - 1] == '\n' || line[n - 1] == ' '))
            line[--n] = 0;
        upper(line);
        if (!line[0] || line[0] == ';')
            continue;
        say("> ");
        say(line);
        say("\n");
        if (line[0] == '@') {               /* another batch file, then on with this one */
            if (!batch(line + 1, lib, line)) {
                say("Stopped.\n");
                return 0;
            }
            continue;
        }
        if (!command(line, lib, from)) {
            say("Stopped.\n");
            return 0;
        }
    }
}
#endif

int main(int argc, char **argv)
{
    char *objs[MAXFILES];
    int nobjs;
#ifdef __TINYC__
    /* UCSD file names are short: small buffers save 1 KB of stack; the
       command line (e.g. /L CC2=MAIN,UTIL,...) needs more */
    char out[30]; char src[30]; char tmpi[30]; char tmpr[30]; char obj[30]; char lib[30]; char line[BATCHLINE];
#else
    char out[200]; char src[200]; char tmpi[200]; char tmpr[200]; char obj[200]; char lib[200]; char line[200];
#endif
    char *s;
    char *t;
    int i;
    int n;
    int conly;
    say("Tiny-C compiler for UCSD Pascal II.0  [0.4]\n");
    nobjs = 0;
    conly = 0;
    out[0] = 0;
    lib[0] = 0;
#ifdef __TINYC__
    strcpy(lib, "TCLIB.OBJ");
    if (!exists(lib))
        strcpy(lib, "*TCLIB.OBJ");
    if (!exists(lib))
        strcpy(lib, "TINY-C:TCLIB.OBJ");
    if (!exists(lib))
        lib[0] = 0;
    if (argc > 1) {
        /* the commands as arguments (from the shell: CC @BUILD @LIBS,
           CC /Z HANOI SIEVE): each is its options (/Z, /C, /L, /J) and the
           word after them; they run in turn, the first that fails stops */
        i = 1;
        while (i < argc) {
            line[0] = 0;
            while (i < argc) {
                if (strlen(line) + strlen(argv[i]) + 2 > sizeof line) {
                    say("command too long\n");
                    return 1;
                }
                strcat(line, argv[i]);
                strcat(line, " ");
                if (argv[i++][0] != '/')
                    break;
            }
            n = strlen(line);
            line[n - 1] = 0;
            upper(line);
            say("> ");
            say(line);
            say("\n");
            if (!(line[0] == '@' ? batch(line + 1, lib, line) : command(line, lib, 0)))
                return 1;
        }
        say("Done.\n");
        return 0;
    }
    say("Compile what file? ");
    if (!fgets(line, BATCHLINE, stdin))
        return 1;
    n = strlen(line);
    while (n > 0 && (line[n - 1] == '\n' || line[n - 1] == ' '))
        line[--n] = 0;
    upper(line);
    s = line;
    while (*s == ' ')
        s++;
    if (!*s)
        return 1;
    if (*s == '@')
        n = batch(s + 1, lib, line);
    else
        n = command(s, lib, 0);
    if (!n)
        return 1;
    say("Done.\n");
    return 0;
#else
    for (i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-o") == 0 && i + 1 < argc)
            strcpy(out, argv[++i]);
        else if (strcmp(argv[i], "-c") == 0)
            conly = 1;
        else if (strcmp(argv[i], "-z") == 0)
            z80calls = 1;
        else if (strcmp(argv[i], "-L") == 0 && i + 1 < argc)
            strcpy(lib, argv[++i]);
        else if (strcmp(argv[i], "-I") == 0 && i + 1 < argc) {
            static char env[300];
            sprintf(env, "TINYC_INCLUDE=%s", argv[++i]);
            putenv(env);
        }
    }
    for (i = 1; i < argc; i++) {
        if (argv[i][0] == '-') {
            if (strcmp(argv[i], "-c") != 0 && strcmp(argv[i], "-z") != 0)
                i++;
            continue;
        }
        n = strlen(argv[i]);
        if (n > 4 && strcmp(argv[i] + n - 4, ".obj") == 0) {
            objs[nobjs++] = argv[i];
            continue;
        }
        strcpy(src, argv[i]);
        if (conly && out[0])
            strcpy(obj, out);
        else {
            strcpy(obj, src);
            s = strrchr(obj, '.');
            if (s)
                *s = 0;
            strcat(obj, ".obj");
        }
        strcpy(tmpi, obj);
        s = strrchr(tmpi, '.');
        if (s)
            *s = 0;
        strcpy(tmpr, tmpi);
        strcat(tmpi, ".i");
        strcat(tmpr, ".ir");
        if (!compileone(src, tmpi, tmpr, obj))
            return 1;
        if (nobjs >= MAXFILES - 1)
            break;
        objs[nobjs] = (char *)malloc(strlen(obj) + 1);
        strcpy(objs[nobjs], obj);
        nobjs++;
    }
    if (nobjs == 0) {
        printf("usage: tc [-c] [-z] [-I dir] [-L lib.obj] [-o out] file.c ... file.obj ...\n");
        return 1;
    }
    if (!lib[0] && getenv("TINYC_INCLUDE")) {
        strcpy(lib, getenv("TINYC_INCLUDE"));
        strcat(lib, "/tclib.obj");
    }
    if (!out[0]) {
        strcpy(out, objs[0]);
        s = strrchr(out, '.');
        if (s)
            *s = 0;
        strcat(out, ".code");
    }
    if (conly) {
        say("Done.\n");
        return 0;
    }
    if (lib[0] && exists(lib))
        objs[nobjs++] = lib;
    if (!linkall(objs, nobjs, out))
        return 1;
    say("Done.\n");
    return 0;
#endif
}
