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
 *
 * Calls through function pointers: by default they use CSP 138 (CALLI), which
 * the native P-Code engine implements and the Z80 interpreter does not. -z (/Z)
 * generates the older sequence that stores the function value into the operands
 * of a CXP at run time, for programs that must run in Z80 mode (and, as the
 * library is linked into every program, a library built the same way).
 */
#include "tc.h"
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
   to where it was before the pass (the stdio files are all closed) */
static void passbegin(int xsize)
{
    curfile = 0;
#ifdef __TINYC__
    __heapsave();
#endif
    xsetsize(xsize);
}

static void passend(void)
{
#ifdef __TINYC__
    say("  (");
    sayn(__cspi(40));
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
    passbegin(2000);
    if (!compile(tmpi, tmpr, mod))
        return 0;
    passend();
    say("Generating code ");
    say(obj);
    say(" ");
    passbegin(2400);
    if (!gencode(tmpr, obj))
        return 0;
    passend();
    say("\n");
    return 1;
}

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
    return r;
}

static int exists(char *name)
{
    FILE *f;
    f = fopen(name, "rb");
    if (!f)
        return 0;
    fclose(f);
    return 1;
}

static void upper(char *s)
{
    for (; *s; s++)
        if (*s >= 'a' && *s <= 'z')
            *s = *s - 32;
}

#ifdef __TINYC__
/* one command (see the top of this file); 0 = it failed */
static int command(char *s, char *lib)
{
    char *objs[MAXFILES];
    char src[25];                       /* UCSD file names are short */
    char obj[25];
    char out[25];
    char *t;
    int nobjs;
    int n;
    int conly;
    int joining;
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
    if (s[0] == '/' && (s[1] == 'L' || s[1] == 'J')) {
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
            objs[nobjs] = (char *)malloc(strlen(s) + 5);
            strcpy(objs[nobjs], s);
            strcat(objs[nobjs], ".OBJ");
            nobjs++;
        }
        if (joining) {
            say("Joining ");
            say(out);
            say("\n");
            return join(objs, nobjs, out);
        }
    } else {
        conly = 0;
        if (s[0] == '/' && s[1] == 'C') {
            conly = 1;
            s = s + 2;
            while (*s == ' ')
                s++;
        }
        /* NAME or NAME.C: the source is NAME.C, or NAME.TEXT when there
           is no NAME.C */
        n = strlen(s);
        if (n > 5 && strcmp(s + n - 5, ".TEXT") == 0)
            s[n = n - 5] = 0;
        if (n > 2 && strcmp(s + n - 2, ".C") == 0)
            s[n - 2] = 0;
        strcpy(src, s);
        strcat(src, ".C");
        if (!exists(src)) {
            strcpy(src, s);
            strcat(src, ".TEXT");
        }
        strcpy(obj, s);
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
   the file is closed again. */
static int batch(char *name, char *lib)
{
    FILE *f;
    char path[25];
    char line[200];
    int done;
    int k;
    int n;
    strcpy(path, name);
    n = strlen(path);
    if (n <= 5 || strcmp(path + n - 5, ".TEXT") != 0)
        strcat(path, ".TEXT");
    for (done = 0;; done++) {
        f = fopen(path, "r");
        if (!f) {
            say("cannot open ");
            say(path);
            say("\n");
            return 0;
        }
        for (k = 0; k <= done; k++)
            if (!fgets(line, 180, f))
                break;
        fclose(f);
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
        if (!command(line, lib)) {
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
    char out[30]; char src[30]; char tmpi[30]; char tmpr[30]; char obj[30]; char lib[30]; char line[150];
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
    say("Compile what file? ");
    if (!fgets(line, 80, stdin))
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
    strcpy(lib, "TCLIB.OBJ");
    if (!exists(lib))
        strcpy(lib, "*TCLIB.OBJ");
    if (!exists(lib))
        strcpy(lib, "TINY-C:TCLIB.OBJ");
    /* looked for once: fopen outside a pass would leak its buffer (the
       free list is dropped when a pass gives its memory back) */
    if (!exists(lib))
        lib[0] = 0;
    if (*s == '@')
        n = batch(s + 1, lib);
    else
        n = command(s, lib);
    if (!n)
        return 1;
    say("Done.\n");
    argc = 0;
    argv = 0;
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
