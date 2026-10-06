/* pp.c -- pass 1: the preprocessor.
 *
 * Reads the source and its #include files and writes one expanded line
 * per source line to the intermediate file.  A line "#<n> <file>" in the
 * output resynchronises the compiler's line numbers (after an include, or
 * after lines were joined).
 *
 * Supported: #include "f" / <f> (nested MAXINCL deep), #define (object and
 * function-like, # and ##), #undef, #if/#ifdef/#ifndef/#elif/#else/#endif
 * with defined(), #error, #line is ignored, #pragma is passed through.
 */
#include "tc.h"
#pragma segment PP

struct Macro {
    char *name;
    int nparams;            /* -1: object-like */
    char *body;             /* parameters coded as \001 then (index + 1) */
    struct Macro *next;
};

#define MHASH 128
static struct Macro **mtab;

struct Incl {
    FILE *fp;
    char *name;
    int line;
    int sys;                    /* a <system> header (or included from one) */
};
static struct Incl *istack;
static int idepth;
static FILE *ppout;
static int incomment;
/* the tables below are allocated per run (in the pass's memory), not
   global: globals take memory from every pass */
static int *ifstate;            /* [MAXIF] 0 = skipping, 1 = active, 2 = done (a branch was taken) */
static int *ifparent;           /* [MAXIF] */
static int iflevel;
static int active;
static struct Macro **expanding;       /* [32] */
static int nexpanding;
static int outline;             /* line number the compiler will assume for the next output line */
static char *outfile;
static char *incdir;
static char *openedpath;        /* [INCPATH] */

static char *line;              /* both MAXEXP bytes, allocated per run */
static char *ebuf;

static int isid1(int c)
{
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
}

static int isidc(int c)
{
    return isid1(c) || (c >= '0' && c <= '9');
}

static struct Macro *mlookup(char *name, int n)
{
    struct Macro *m;
    int h;
    int i;
    h = 0;
    for (i = 0; i < n; i++)
        h = (h * 3 + name[i]) & 1023;
    for (m = mtab[h & (MHASH - 1)]; m; m = m->next)
        if ((int)strlen(m->name) == n && strncmp(m->name, name, n) == 0)
            return m;
    return 0;
}

static void mundef(char *name, int n)
{
    struct Macro *m;
    struct Macro **pp;
    int h;
    int i;
    h = 0;
    for (i = 0; i < n; i++)
        h = (h * 3 + name[i]) & 1023;
    pp = &mtab[h & (MHASH - 1)];
    for (m = *pp; m; m = m->next) {
        if ((int)strlen(m->name) == n && strncmp(m->name, name, n) == 0) {
            *pp = m->next;
            return;
        }
        pp = &m->next;
    }
}

static struct Macro *mdefine(char *name, int n, int nparams, char *body)
{
    struct Macro *m;
    int h;
    int i;
    mundef(name, n);
    m = (struct Macro *)palloc(sizeof(struct Macro));
    m->name = palloc(n + 1);
    memcpy(m->name, name, n);
    m->name[n] = 0;
    m->nparams = nparams;
    m->body = pstrdup(body);
    h = 0;
    for (i = 0; i < n; i++)
        h = (h * 3 + name[i]) & 1023;
    m->next = mtab[h & (MHASH - 1)];
    mtab[h & (MHASH - 1)] = m;
    return m;
}

/* skip a string or character literal starting at s (s[0] is the quote) */
static char *skiplit(char *s)
{
    int q;
    q = *s++;
    while (*s && *s != q) {
        if (*s == '\\' && s[1])
            s++;
        s++;
    }
    if (*s)
        s++;
    return s;
}

/* ---- reading source lines ---- */

#ifdef __TINYC__
#define INCPATH 30          /* VOLNAME:NAME.TEXT -- see openinc */
#else
#define INCPATH 200
#endif

static FILE *openinc(char *name, int sys)
{
    FILE *fp;
    char path[INCPATH];
    char *p;
#ifdef __TINYC__
    /* P-System: file names are upper case; "x.h" is X.H (or X.H.TEXT);
       <x.h> is searched on the default volume, then on the boot volume,
       then on TINY-C:.  A UCSD file name has at most 15 characters, a
       volume name 7: a name with its own volume (or *) is tried only as
       it is, so the longest path is TINY-C: + 15 + .TEXT (28) or
       VOLNAME: + 15 + .TEXT (28); a longer name is no file at all. */
    int i;
    int v;
    int vol;
    char u[24];
    vol = 0;
    for (i = 0; name[i] && i < 23; i++) {
        u[i] = name[i];
        if (u[i] >= 'a' && u[i] <= 'z')
            u[i] = u[i] - 32;
        if (u[i] == ':' || u[i] == '*')
            vol = 1;
    }
    u[i] = 0;
    fp = 0;
    if (name[i] || (!vol && i > 15))
        return 0;                   /* cannot be a file name */
    for (v = 0; !fp && v < (sys && !vol ? 3 : 1); v++) {
        strcpy(path, v == 0 ? "" : (v == 1 ? "*" : "TINY-C:"));
        strcat(path, u);
        fp = fopen(path, "r");
        if (!fp) {
            strcat(path, ".TEXT");
            fp = fopen(path, "r");
        }
    }
    strcpy(openedpath, path);
    p = 0;
    return fp;
#else
    fp = 0;
    if (!sys && idepth > 0) {
        /* the directory of the including file */
        strcpy(path, istack[idepth - 1].name);
        p = strrchr(path, '/');
        if (p) {
            strcpy(p + 1, name);
            fp = fopen(path, "r");
            if (fp)
                strcpy(openedpath, path);
        }
    }
    if (!fp && !sys) {
        fp = fopen(name, "r");
        strcpy(openedpath, name);
    }
    if (!fp && incdir) {
        strcpy(path, incdir);
        p = path + strlen(path);
        if (p > path && p[-1] != '/')
            *p++ = '/';
        strcpy(p, name);
        fp = fopen(path, "r");
        if (fp)
            strcpy(openedpath, path);
    }
    if (!fp && sys) {
        fp = fopen(name, "r");
        strcpy(openedpath, name);
    }
    return fp;
#endif
}

static char *outname;            /* file the compiler will assume */
static int startline;           /* first physical line of the current logical line */
static char *pragbuf;           /* [MAXLINE] */

/* read one physical line into buf; strips the newline. 0 at end of file */
static int rawline(char *buf, int max)
{
    int c;
    int n;
    FILE *fp;
    fp = istack[idepth - 1].fp;
    n = 0;
    c = getc(fp);
    if (c == EOF)
        return 0;
    while (c != EOF && c != '\n') {
        if (c != '\r' && n < max - 1)
            buf[n++] = c;
        c = getc(fp);
    }
    buf[n] = 0;
    istack[idepth - 1].line++;
    return 1;
}

/* remove comments from buf in place (state carried in incomment) */
static void uncomment(char *buf)
{
    char *s;
    char *d;
    char *e;
    s = buf;
    d = buf;
    while (*s) {
        if (incomment) {
            if (s[0] == '*' && s[1] == '/') {
                incomment = 0;
                s += 2;
                *d++ = ' ';
            } else
                s++;
        } else if (*s == '"' || *s == '\'') {
            e = skiplit(s);
            while (s < e)
                *d++ = *s++;
        } else if (s[0] == '/' && s[1] == '*') {
            incomment = 1;
            s += 2;
        } else if (s[0] == '/' && s[1] == '/') {
            break;
        } else
            *d++ = *s++;
    }
    *d = 0;
}

/* paren balance of a line (outside literals) */
static int balance(char *s)
{
    int b;
    b = 0;
    while (*s) {
        if (*s == '"' || *s == '\'') {
            s = skiplit(s);
            continue;
        }
        if (*s == '(')
            b++;
        else if (*s == ')')
            b--;
        s++;
    }
    return b;
}

/* a logical line: continuation lines joined, comments removed; for
   non-ppdirective lines, lines are joined while parentheses are open */
static int ppgetline(void)
{
    char tmp[MAXLINE];
    int n;
    int joined;
    char *s;
    joined = 0;
    if (!rawline(line, MAXLINE))
        return 0;
    startline = istack[idepth - 1].line;
    n = strlen(line);
    while (n > 0 && line[n - 1] == '\\') {
        line[n - 1] = 0;
        if (!rawline(tmp, MAXLINE))
            break;
        if (n + (int)strlen(tmp) >= MAXEXP - 2)
            fatal(4 /* line too long */, 0);
        strcat(line, tmp);
        n = strlen(line);
        joined = 1;
    }
    uncomment(line);
    s = line;
    while (*s == ' ' || *s == '\t')
        s++;
    if (*s != '#' && active) {
        while (balance(line) > 0 || incomment) {
            if (!rawline(tmp, MAXLINE))
                break;
            uncomment(tmp);
            n = strlen(line);
            if (n + (int)strlen(tmp) >= MAXEXP - 2)
                fatal(4 /* line too long */, 0);
            line[n] = ' ';
            strcpy(line + n + 1, tmp);
            joined = 1;
        }
    }
    return joined ? 2 : 1;
}

/* ---- macro expansion ---- */

static char *outp;
static char *outend;

static void put(char *s, int n)
{
    if (outp + n >= outend)
        fatal(5 /* macro expansion too long */, 0);
    memcpy(outp, s, n);
    outp += n;
}

static int isexpanding(struct Macro *m)
{
    int i;
    for (i = 0; i < nexpanding; i++)
        if (expanding[i] == m)
            return 1;
    return 0;
}

static void expand(char *s);

/* expand text into a fresh buffer and return a permanent-until-next-line copy */
static char *expandcopy(char *s, int n)
{
    char *save;
    char *saveend;
    char *buf;
    char *src;
    int len;
    int xm;
    src = xalloc(n + 1);
    memcpy(src, s, n);
    src[n] = 0;
    buf = xalloc(MAXEXP);
    save = outp;
    xm = xmark();
    saveend = outend;
    outp = buf;
    outend = buf + MAXEXP;
    expand(src);
    *outp = 0;
    len = outp - buf;
    outp = save;
    outend = saveend;
    buf[len] = 0;
    return buf;
}

/* stringify raw argument text */
static void stringify(char *s, int n)
{
    char c;
    int i;
    int inlit;
    put("\"", 1);
    inlit = 0;
    for (i = 0; i < n; i++) {
        c = s[i];
        if (c == '"' || (c == '\\' && inlit))
            put("\\", 1);
        if (c == '"' || c == '\'')
            inlit = !inlit;
        put(&c, 1);
    }
    put("\"", 1);
}

static char *trim(char *s, int *n)
{
    while (*n > 0 && (*s == ' ' || *s == '\t')) {
        s++;
        (*n)--;
    }
    while (*n > 0 && (s[*n - 1] == ' ' || s[*n - 1] == '\t'))
        (*n)--;
    return s;
}

/* expand function-like macro m; p points after the name. returns end of call, 0 if no '(' */
static char *expandcall(struct Macro *m, char *p)
{
    char *args[32];
    int alen[32];
    char *aexp[32];
    int nargs;
    int depth;
    char *q;
    char *body;
    char *start;
    char *rstart;
    char *res;
    char *saveout;
    char *saveend;
    char *b;
    int idx;
    int n;
    int paste;
    int xm;
    q = p;
    while (*q == ' ' || *q == '\t')
        q++;
    if (*q != '(')
        return 0;
    q++;
    nargs = 0;
    depth = 0;
    start = q;
    for (;;) {
        if (*q == 0)
            fatal(6 /* unterminated macro call */, m->name);
        if (*q == '"' || *q == '\'') {
            q = skiplit(q);
            continue;
        }
        if (*q == '(')
            depth++;
        else if ((*q == ',' || *q == ')') && depth == 0) {
            if (nargs >= 32)
                fatal(7 /* too many macro arguments */, m->name);
            n = q - start;
            args[nargs] = trim(start, &n);
            alen[nargs] = n;
            aexp[nargs] = 0;
            nargs++;
            if (*q == ')') {
                q++;
                break;
            }
            start = q + 1;
        } else if (*q == ')')
            depth--;
        q++;
    }
    if (nargs == 1 && alen[0] == 0 && m->nparams == 0)
        nargs = 0;
    if (nargs != m->nparams)
        error(8 /* wrong number of macro arguments */, m->name);
    /* substitute into a temporary buffer, then rescan it */
    xm = xmark();
    res = xalloc(MAXEXP);
    saveout = outp;
    saveend = outend;
    outp = res;
    outend = res + MAXEXP;
    for (b = m->body; *b; ) {
        if (*b == '#' && b[1] == '#') {
            /* paste: drop trailing blanks already written and leading blanks */
            while (outp > res && (outp[-1] == ' ' || outp[-1] == '\t'))
                outp--;
            b += 2;
            while (*b == ' ' || *b == '\t')
                b++;
            if (*b == 1) {
                idx = b[1] - 1;
                if (idx < nargs)
                    put(args[idx], alen[idx]);
                b += 2;
            }
            continue;
        }
        if (*b == '#' && b[1] != '#') {
            char *t;
            t = b + 1;
            while (*t == ' ')
                t++;
            if (*t == 1) {
                idx = t[1] - 1;
                if (idx < nargs)
                    stringify(args[idx], alen[idx]);
                b = t + 2;
                continue;
            }
        }
        if (*b == 1) {
            idx = b[1] - 1;
            b += 2;
            /* an argument next to ## is pasted unexpanded */
            paste = 0;
            {
                char *t;
                t = b;
                while (*t == ' ' || *t == '\t')
                    t++;
                if (t[0] == '#' && t[1] == '#')
                    paste = 1;
            }
            if (idx < nargs) {
                if (paste)
                    put(args[idx], alen[idx]);
                else {
                    if (!aexp[idx])
                        aexp[idx] = expandcopy(args[idx], alen[idx]);
                    put(aexp[idx], strlen(aexp[idx]));
                }
            }
            continue;
        }
        if (*b == '"' || *b == '\'') {
            char *e;
            e = skiplit(b);
            put(b, e - b);
            b = e;
            continue;
        }
        put(b, 1);
        b++;
    }
    *outp = 0;
    outp = saveout;
    outend = saveend;
    expanding[nexpanding++] = m;
    expand(res);
    nexpanding--;
    xrelease(xm);
    return q;
}

static void expand(char *s)
{
    char *p;
    char *e;
    struct Macro *m;
    char num[8];
    while (*s) {
        if (*s == '"' || *s == '\'') {
            e = skiplit(s);
            put(s, e - s);
            s = e;
        } else if (isid1(*s)) {
            p = s;
            while (isidc(*p))
                p++;
            m = mlookup(s, p - s);
            if (m && !isexpanding(m)) {
                if (m->nparams < 0) {
                    if (nexpanding >= 30)
                        fatal(9 /* macro nesting too deep */, m->name);
                    expanding[nexpanding++] = m;
                    expand(m->body);
                    nexpanding--;
                    s = p;
                    continue;
                }
                e = expandcall(m, p);
                if (e) {
                    s = e;
                    continue;
                }
            }
            if (p - s == 8 && strncmp(s, "__LINE__", 8) == 0) {
                itoa10(istack[idepth - 1].line, num);
                put(num, strlen(num));
            } else if (p - s == 8 && strncmp(s, "__FILE__", 8) == 0) {
                put("\"", 1);
                put(istack[idepth - 1].name, strlen(istack[idepth - 1].name));
                put("\"", 1);
            } else
                put(s, p - s);
            s = p;
        } else if (*s >= '0' && *s <= '9') {
            p = s;
            while (isidc(*p) || *p == '.' || ((*p == '+' || *p == '-') && (p[-1] == 'e' || p[-1] == 'E')))
                p++;
            put(s, p - s);
            s = p;
        } else {
            put(s, 1);
            s++;
        }
    }
}

/* ---- #if expressions ---- */

static char *ep;

static void eskip(void)
{
    while (*ep == ' ' || *ep == '\t')
        ep++;
}

static int ecomma(void);

static int eprimary(void)
{
    int v;
    int base;
    int d;
    eskip();
    if (*ep == '(') {
        ep++;
        v = ecomma();
        eskip();
        if (*ep == ')')
            ep++;
        return v;
    }
    if (*ep == '!') {
        ep++;
        return !eprimary();
    }
    if (*ep == '~') {
        ep++;
        return ~eprimary();
    }
    if (*ep == '-') {
        ep++;
        return -eprimary();
    }
    if (*ep == '+') {
        ep++;
        return eprimary();
    }
    if (*ep == '\'') {
        ep++;
        v = *ep++;
        if (v == '\\') {
            v = *ep++;
            if (v == 'n')
                v = 10;
            else if (v == 't')
                v = 9;
            else if (v == '0')
                v = 0;
        }
        if (*ep == '\'')
            ep++;
        return v;
    }
    if (*ep >= '0' && *ep <= '9') {
        v = 0;
        base = 10;
        if (*ep == '0') {
            base = 8;
            ep++;
            if (*ep == 'x' || *ep == 'X') {
                base = 16;
                ep++;
            }
        }
        for (;;) {
            if (*ep >= '0' && *ep <= '9')
                d = *ep - '0';
            else if (*ep >= 'a' && *ep <= 'f')
                d = *ep - 'a' + 10;
            else if (*ep >= 'A' && *ep <= 'F')
                d = *ep - 'A' + 10;
            else
                break;
            if (d >= base)
                break;
            v = W16(v * base + d);
            ep++;
        }
        while (*ep == 'u' || *ep == 'U' || *ep == 'l' || *ep == 'L')
            ep++;
        return v;
    }
    if (isid1(*ep)) {           /* identifiers left after expansion are 0 */
        while (isidc(*ep))
            ep++;
        return 0;
    }
    error(10 /* bad #if expression */, 0);
    return 0;
}

static int emul(void)
{
    int v;
    int r;
    v = eprimary();
    for (;;) {
        eskip();
        if (*ep == '*') {
            ep++;
            v = W16(v * eprimary());
        } else if (*ep == '/' || *ep == '%') {
            int op;
            op = *ep++;
            r = eprimary();
            if (r == 0)
                error(11 /* division by zero in #if */, 0);
            else if (op == '/')
                v = v / r;
            else
                v = v % r;
        } else
            return v;
    }
}

static int eadd(void)
{
    int v;
    v = emul();
    for (;;) {
        eskip();
        if (*ep == '+') {
            ep++;
            v = W16(v + emul());
        } else if (*ep == '-') {
            ep++;
            v = W16(v - emul());
        } else
            return v;
    }
}

static int eshift(void)
{
    int v;
    v = eadd();
    for (;;) {
        eskip();
        if (ep[0] == '<' && ep[1] == '<') {
            ep += 2;
            v = W16(v << eadd());
        } else if (ep[0] == '>' && ep[1] == '>') {
            ep += 2;
            v = v >> eadd();
        } else
            return v;
    }
}

static int erel(void)
{
    int v;
    v = eshift();
    for (;;) {
        eskip();
        if (ep[0] == '<' && ep[1] == '=') {
            ep += 2;
            v = v <= eshift();
        } else if (ep[0] == '>' && ep[1] == '=') {
            ep += 2;
            v = v >= eshift();
        } else if (ep[0] == '<') {
            ep++;
            v = v < eshift();
        } else if (ep[0] == '>') {
            ep++;
            v = v > eshift();
        } else
            return v;
    }
}

static int eeq(void)
{
    int v;
    v = erel();
    for (;;) {
        eskip();
        if (ep[0] == '=' && ep[1] == '=') {
            ep += 2;
            v = v == erel();
        } else if (ep[0] == '!' && ep[1] == '=') {
            ep += 2;
            v = v != erel();
        } else
            return v;
    }
}

static int eband(void)
{
    int v;
    v = eeq();
    for (;;) {
        eskip();
        if (ep[0] == '&' && ep[1] != '&') {
            ep++;
            v = v & eeq();
        } else
            return v;
    }
}

static int exor(void)
{
    int v;
    v = eband();
    for (;;) {
        eskip();
        if (ep[0] == '^') {
            ep++;
            v = v ^ eband();
        } else
            return v;
    }
}

static int ebor(void)
{
    int v;
    v = exor();
    for (;;) {
        eskip();
        if (ep[0] == '|' && ep[1] != '|') {
            ep++;
            v = v | exor();
        } else
            return v;
    }
}

static int eland(void)
{
    int v;
    int r;
    v = ebor();
    for (;;) {
        eskip();
        if (ep[0] == '&' && ep[1] == '&') {
            ep += 2;
            r = ebor();
            v = v && r;
        } else
            return v;
    }
}

static int elor(void)
{
    int v;
    int r;
    v = eland();
    for (;;) {
        eskip();
        if (ep[0] == '|' && ep[1] == '|') {
            ep += 2;
            r = eland();
            v = v || r;
        } else
            return v;
    }
}

static int econd(void)
{
    int v;
    int a;
    int b;
    v = elor();
    eskip();
    if (*ep == '?') {
        ep++;
        a = ecomma();
        eskip();
        if (*ep == ':')
            ep++;
        b = econd();
        return v ? a : b;
    }
    return v;
}

static int ecomma(void)
{
    return econd();
}

/* evaluate #if text: defined() first, then macro expansion */
static int ifexpr(char *s)
{
    char *buf;
    char *d;
    char *p;
    int paren;
    int v;
    buf = xalloc(MAXEXP);
    d = buf;
    while (*s) {
        if (isid1(*s)) {
            p = s;
            while (isidc(*p))
                p++;
            if (p - s == 7 && strncmp(s, "defined", 7) == 0) {
                s = p;
                while (*s == ' ')
                    s++;
                paren = 0;
                if (*s == '(') {
                    paren = 1;
                    s++;
                }
                while (*s == ' ')
                    s++;
                p = s;
                while (isidc(*p))
                    p++;
                *d++ = mlookup(s, p - s) ? '1' : '0';
                s = p;
                while (*s == ' ')
                    s++;
                if (paren && *s == ')')
                    s++;
                continue;
            }
            while (s < p)
                *d++ = *s++;
            continue;
        }
        *d++ = *s++;
    }
    *d = 0;
    outp = ebuf;
    outend = ebuf + MAXEXP;
    expand(buf);
    *outp = 0;
    ep = ebuf;
    v = ecomma();
    return v;
}

/* ---- directives ---- */

static char *word(char *s, char *w, int max)
{
    int n;
    while (*s == ' ' || *s == '\t')
        s++;
    n = 0;
    while (isidc(*s)) {
        if (n < max - 1)
            w[n++] = *s;
        s++;
    }
    w[n] = 0;
    return s;
}

static void dodefine(char *s)
{
    char name[MAXNAME];
    char pnames[32][MAXNAME];
    int np;
    int i;
    int n;
    char *body;
    char *d;
    char *p;
    s = word(s, name, MAXNAME);
    if (!name[0]) {
        error(12 /* bad #define */, 0);
        return;
    }
    np = -1;
    if (*s == '(') {
        np = 0;
        s++;
        for (;;) {
            while (*s == ' ' || *s == '\t')
                s++;
            if (*s == ')') {
                s++;
                break;
            }
            if (np >= 32)
                fatal(13 /* too many macro parameters */, name);
            s = word(s, pnames[np], MAXNAME);
            np++;
            while (*s == ' ' || *s == '\t')
                s++;
            if (*s == ',')
                s++;
            else if (*s == ')') {
                s++;
                break;
            } else {
                error(14 /* bad macro parameter list */, name);
                return;
            }
        }
    }
    while (*s == ' ' || *s == '\t')
        s++;
    body = xalloc(MAXEXP);
    d = body;
    while (*s) {
        if (*s == '"' || *s == '\'') {
            p = skiplit(s);
            while (s < p)
                *d++ = *s++;
            continue;
        }
        if (isid1(*s)) {
            p = s;
            while (isidc(*p))
                p++;
            n = p - s;
            for (i = 0; i < np; i++)
                if ((int)strlen(pnames[i]) == n && strncmp(pnames[i], s, n) == 0)
                    break;
            if (i < np) {
                *d++ = 1;
                *d++ = i + 1;
            } else
                while (s < p)
                    *d++ = *s++;
            s = p;
            continue;
        }
        *d++ = *s++;
    }
    while (d > body && (d[-1] == ' ' || d[-1] == '\t'))
        d--;
    *d = 0;
    mdefine(name, strlen(name), np, body);
}

static void doinclude(char *s)
{
    char name[MAXNAME];
    int n;
    int sys;
    int close;
    FILE *fp;
    while (*s == ' ' || *s == '\t')
        s++;
    if (*s != '"' && *s != '<') {
        outp = ebuf;
        outend = ebuf + MAXEXP;
        expand(s);
        *outp = 0;
        s = ebuf;
        while (*s == ' ')
            s++;
    }
    sys = *s == '<';
    close = sys ? '>' : '"';
    if (*s != '"' && *s != '<') {
        error(15 /* bad #include */, 0);
        return;
    }
    s++;
    n = 0;
    while (*s && *s != close && n < MAXNAME - 1)
        name[n++] = *s++;
    name[n] = 0;
    if (idepth >= MAXINCL)
        fatal(16 /* #include nested too deeply */, name);
    fp = openinc(name, sys);
    if (!fp) {
        error(17 /* cannot open include file */, name);
        return;
    }
    istack[idepth].fp = fp;
    istack[idepth].name = pstrdup(openedpath);
    istack[idepth].line = 0;
    istack[idepth].sys = sys || istack[idepth - 1].sys;
    idepth++;
}

static void ppdirective(char *s)
{
    char w[16];
    int v;
    s++;                            /* the '#' */
    s = word(s, w, 16);
    if (strcmp(w, "ifdef") == 0 || strcmp(w, "ifndef") == 0 || strcmp(w, "if") == 0) {
        char name[MAXNAME];
        if (iflevel >= MAXIF)
            fatal(18 /* #if nested too deeply */, 0);
        ifparent[iflevel] = active;
        if (!active)
            v = 0;
        else if (w[2] == 'd') {
            word(s, name, MAXNAME);
            v = mlookup(name, strlen(name)) != 0;
        } else if (w[2] == 'n') {
            word(s, name, MAXNAME);
            v = mlookup(name, strlen(name)) == 0;
        } else
            v = ifexpr(s) != 0;
        ifstate[iflevel] = v ? 1 : 0;
        iflevel++;
        active = active && v;
        return;
    }
    if (strcmp(w, "elif") == 0) {
        if (iflevel == 0) {
            error(19 /* #elif without #if */, 0);
            return;
        }
        if (ifstate[iflevel - 1] != 0 || !ifparent[iflevel - 1]) {
            ifstate[iflevel - 1] = 2;
            active = 0;
        } else {
            v = ifexpr(s) != 0;
            ifstate[iflevel - 1] = v ? 1 : 0;
            active = v;
        }
        return;
    }
    if (strcmp(w, "else") == 0) {
        if (iflevel == 0) {
            error(20 /* #else without #if */, 0);
            return;
        }
        if (ifstate[iflevel - 1] == 0 && ifparent[iflevel - 1]) {
            ifstate[iflevel - 1] = 1;
            active = 1;
        } else {
            ifstate[iflevel - 1] = 2;
            active = 0;
        }
        return;
    }
    if (strcmp(w, "endif") == 0) {
        if (iflevel == 0) {
            error(21 /* #endif without #if */, 0);
            return;
        }
        iflevel--;
        active = ifparent[iflevel];
        return;
    }
    if (!active)
        return;
    if (strcmp(w, "define") == 0)
        dodefine(s);
    else if (strcmp(w, "undef") == 0) {
        char name[MAXNAME];
        word(s, name, MAXNAME);
        mundef(name, strlen(name));
    } else if (strcmp(w, "include") == 0)
        doinclude(s);
    else if (strcmp(w, "error") == 0)
        error(22 /* #error */, s);
    else if (strcmp(w, "pragma") == 0) {
        strcpy(pragbuf, "#pragma");
        strcat(pragbuf, s);
    }
    else if (strcmp(w, "line") == 0 || w[0] == 0)
        ;
    else
        error(23 /* unknown ppdirective */, w);
}

/* the expanded line with every real constant converted (realtoken,
   lex.c), so that the compiling pass needs no conversion; strings,
   character constants and identifiers are left as they are */
static void reals(char *in, char *out)
{
    char *o;
    char *p;
    int q;
    int prev;
    int isreal;
    int k;
    o = out;
    q = 0;
    prev = 0;
    while (*in) {
        if (q) {
            *o++ = *in;
            if (*in == '\\' && in[1]) {
                in++;
                *o++ = *in;
            } else if (*in == q)
                q = 0;
            prev = *in++;
            continue;
        }
        if (*in == '"' || *in == '\'') {
            q = *in;
            prev = *in;
            *o++ = *in++;
            continue;
        }
        if (*in >= '0' && *in <= '9' && !isidc(prev) && prev != '.') {
            isreal = 0;
            p = in;
            if (!(p[0] == '0' && (p[1] == 'x' || p[1] == 'X')))
                for (; isidc(*p) || *p == '.' || ((*p == '+' || *p == '-') && (p[-1] == 'e' || p[-1] == 'E')); p++)
                    if (*p == '.' || *p == 'e' || *p == 'E')
                        isreal = 1;
            /* room for the token (at most about 50 characters) and the rest */
            if (isreal && (o - out) + strlen(in) + 60 < MAXEXP) {
                k = realtoken(in, o);
                if (k) {
                    o = o + strlen(o);
                    in = in + k;
                    prev = '0';
                    continue;
                }
            }
        }
        prev = *in;
        *o++ = *in++;
    }
    *o = 0;
}

int preprocess(char *src, char *out)
{
    char num[8];
    int r;
    int m;
    char *s;
    line = malloc(MAXEXP);
    ebuf = malloc(MAXEXP);
    mtab = (struct Macro **)calloc(MHASH, sizeof(struct Macro *));
    istack = (struct Incl *)calloc(MAXINCL, sizeof(struct Incl));
    ifstate = (int *)malloc(MAXIF * sizeof(int));
    ifparent = (int *)malloc(MAXIF * sizeof(int));
    expanding = (struct Macro **)malloc(32 * sizeof(struct Macro *));
    openedpath = malloc(INCPATH);
    pragbuf = malloc(MAXLINE);
    if (!line || !ebuf || !mtab || !istack || !ifstate || !ifparent || !expanding || !openedpath || !pragbuf)
        fatal(2 /* out of memory */, 0);
    ppout = fopen(out, "w");
    if (!ppout)
        fatal(24 /* cannot create */, out);
    incdir = getenv("TINYC_INCLUDE");
    mdefine("__TINYC__", 9, -1, "1");
    mdefine("__UCSD__", 8, -1, "1");
    istack[0].fp = fopen(src, "r");
    if (!istack[0].fp)
        fatal(25 /* cannot open */, src);
    istack[0].name = pstrdup(src);
    istack[0].line = 0;
    istack[0].sys = 0;
    idepth = 1;
    active = 1;
    iflevel = 0;
    outline = 0;
    outname = 0;
    incomment = 0;
    nexpanding = 0;
    for (;;) {
        m = xmark();
        r = ppgetline();
        if (r == 0) {
            fclose(istack[idepth - 1].fp);
            idepth--;
            xrelease(m);
            if (idepth == 0)
                break;
            continue;
        }
        curfile = istack[idepth - 1].name;
        curline = startline;
        if (outline != startline || outname != curfile) {
            /* '!' marks included files: the compiler skips their unused declarations */
            putc('#', ppout);
            fputs(itoa10(startline, num), ppout);
            fputs(idepth > 1 ? " !" : " ", ppout);
            fputs(curfile, ppout);
            putc('\n', ppout);
            outname = curfile;
        }
        outline = startline + 1;
        s = line;
        while (*s == ' ' || *s == '\t')
            s++;
        if (*s == '#') {
            pragbuf[0] = 0;
            ppdirective(s);
            fputs(pragbuf, ppout);
        } else if (active) {
            outp = ebuf;
            outend = ebuf + MAXEXP;
            nexpanding = 0;
            expand(line);
            *outp = 0;
            reals(ebuf, line);          /* line is free again: the converted line */
            fputs(line, ppout);
        }
        fputc('\n', ppout);
        xrelease(m);
    }
    if (iflevel)
        error(26 /* missing #endif */, 0);
    fclose(ppout);
    return nerrors == 0;
}
