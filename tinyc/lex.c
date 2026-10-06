/* lex.c -- pass 2 tokenizer: reads the preprocessor's output.
 *
 * Lines "#<n> <file>" set the position; "#pragma ..." lines are handed to
 * pragma().  Numbers are converted with 16-bit-safe arithmetic so the host
 * and P-System builds of the compiler agree bit for bit.
 */
#include "tc.h"
#pragma segment PARSE

int tok;                        /* current token */
int insys;                      /* reading a <system> header */
int tokval;                     /* T_NUM: low 16 bits */
int tokval2;                    /* T_NUM: high 16 bits (long constants) */
int toklong;                    /* T_NUM: 1 = long, and bit 2 = unsigned */
char *tokname;                  /* T_ID [MAXNAME], allocated by lexinit */
char *tokstr;                   /* T_STR (xalloc'd) */
int toklen;                     /* T_STR length including the final NUL */
unsigned char tokreal[4];       /* T_FNUM image (float) */
char *toknum;                   /* T_FNUM text "DIGITSeEXP" (for a double), allocated by lexinit */

static FILE *lexin;
static int ch;                  /* lookahead character */
static int atbol;

/* one token of pushback, used for declarator lookahead */
static int ptok;
static int pval;
static int pval2;
static int plong;
static char *pname;             /* [MAXNAME] */
static int havepeek;
#define MAXSTR 260               /* LPA holds at most 255 bytes */
static char *strbufs[2];
static int strslot;

void pragma(char *s);

static char *kwtab[] = {
    "auto", "break", "case", "char", "const", "continue", "default", "do",
    "double", "else", "enum", "extern", "float", "for", "goto", "if",
    "int", "long", "register", "return", "short", "signed", "sizeof", "static",
    "struct", "switch", "typedef", "union", "unsigned", "void", "volatile", "while"
};

static void nextch(void)
{
    ch = getc(lexin);
}

void lexinit(FILE *fp)
{
    strbufs[0] = malloc(MAXSTR);
    strbufs[1] = malloc(MAXSTR);
    tokname = malloc(MAXNAME);
    pname = malloc(MAXNAME);
    toknum = malloc(40);
    if (!strbufs[0] || !strbufs[1] || !tokname || !pname || !toknum)
        fatal(2 /* out of memory */, 0);
    lexin = fp;
    atbol = 1;
    havepeek = 0;
    insys = 0;
    strslot = 0;
    nextch();
}

static void lexdirective(void)
{
    char buf[MAXLINE];
    int n;
    int v;
    char *s;
    n = 0;
    nextch();
    while (ch != '\n' && ch != EOF) {
        if (n < MAXLINE - 1)
            buf[n++] = ch;
        nextch();
    }
    buf[n] = 0;
    if (buf[0] >= '0' && buf[0] <= '9') {
        v = 0;
        s = buf;
        while (*s >= '0' && *s <= '9')
            v = v * 10 + *s++ - '0';
        while (*s == ' ')
            s++;
        curline = v - 1;         /* the newline that follows counts */
        insys = *s == '!';
        if (insys)
            s++;
        if (!curfile || strcmp(curfile, s) != 0)
            curfile = pstrdup(s);
    } else if (strncmp(buf, "pragma", 6) == 0)
        pragma(buf + 6);
}

static int escape(void)
{
    int v;
    int n;
    nextch();
    v = ch;
    switch (ch) {
    case 'n': v = 10; break;
    case 't': v = 9; break;
    case 'r': v = 13; break;
    case 'b': v = 8; break;
    case 'f': v = 12; break;
    case 'v': v = 11; break;
    case 'a': v = 7; break;
    case 'x':
        v = 0;
        nextch();
        for (;;) {
            if (ch >= '0' && ch <= '9')
                v = v * 16 + ch - '0';
            else if (ch >= 'a' && ch <= 'f')
                v = v * 16 + ch - 'a' + 10;
            else if (ch >= 'A' && ch <= 'F')
                v = v * 16 + ch - 'A' + 10;
            else
                return v & 255;
            v = v & 4095;
            nextch();
        }
    default:
        if (ch >= '0' && ch <= '7') {
            v = 0;
            n = 0;
            while (ch >= '0' && ch <= '7' && n < 3) {
                v = v * 8 + ch - '0';
                n++;
                nextch();
            }
            return v & 255;
        }
    }
    nextch();
    return v;
}

/* ---- numbers ---- */

/* acc = acc * base + d on a 4-byte little-endian accumulator; returns overflow */
static int mac(unsigned char *acc, int base, int d)
{
    int i;
    int v;
    for (i = 0; i < 4; i++) {
        v = acc[i] * base + d;
        acc[i] = v & 255;
        d = (v >> 8) & 255;
    }
    return d != 0;
}

/* ---- decimal to real conversion with big integers (bytes, little endian) ----
   (a segment of its own: loaded only while a floating constant is converted) */
#pragma segment REALLIT
#define BIGN 48

static int bitlen(unsigned char *a)
{
    int i;
    int b;
    for (i = BIGN - 1; i >= 0; i--)
        if (a[i]) {
            b = 8;
            while (!(a[i] & (1 << (b - 1))))
                b--;
            return i * 8 + b;
        }
    return 0;
}

static int getbit(unsigned char *a, int n)
{
    if (n < 0 || n >= BIGN * 8)
        return 0;
    return (a[n >> 3] >> (n & 7)) & 1;
}

static void bigmul(unsigned char *a, int m)
{
    int i;
    int c;
    int v;
    c = 0;
    for (i = 0; i < BIGN; i++) {
        v = a[i] * m + c;
        a[i] = v & 255;
        c = (v >> 8) & 255;
    }
    if (c)
        error(27 /* floating constant too large */, 0);
}

static void bigshl(unsigned char *a)
{
    int i;
    int c;
    int v;
    c = 0;
    for (i = 0; i < BIGN; i++) {
        v = a[i] * 2 + c;
        a[i] = v & 255;
        c = v >> 8;
    }
}

static int bigcmp(unsigned char *a, unsigned char *b)
{
    int i;
    for (i = BIGN - 1; i >= 0; i--)
        if (a[i] != b[i])
            return a[i] < b[i] ? -1 : 1;
    return 0;
}

static void bigsub(unsigned char *a, unsigned char *b)
{
    int i;
    int bw;
    int v;
    bw = 0;
    for (i = 0; i < BIGN; i++) {
        v = a[i] - b[i] - bw;
        bw = v < 0;
        a[i] = v & 255;
    }
}

/* 24-bit mantissa + exponent: value = digits * 10^exp10 */
static void makereal(unsigned char *digits, int nd, int exp10, unsigned char *out)
{
    unsigned char num[BIGN];
    unsigned char den[BIGN];
    unsigned char q[BIGN];
    int i;
    int k;
    int bl;
    int e2;
    int round;
    int sticky;
    int m1;
    int m2;
    int m3;
    int top;
    memset(num, 0, BIGN);
    memset(den, 0, BIGN);
    for (i = 0; i < nd; i++) {
        int j;
        int c;
        int v;
        bigmul(num, 10);
        c = digits[i];
        for (j = 0; j < BIGN && c; j++) {
            v = num[j] + c;
            num[j] = v & 255;
            c = v >> 8;
        }
    }
    for (i = 0; i < 4; i++)
        out[i] = 0;
    if (bitlen(num) == 0)
        return;
    den[0] = 1;
    k = 0;
    if (exp10 >= 0) {
        while (exp10-- > 0)
            bigmul(num, 10);
    } else {
        while (exp10++ < 0)
            bigmul(den, 10);
    }
    /* scale so that num / den has at least 26 significant bits */
    while (bitlen(num) < bitlen(den) + 27) {
        bigshl(num);
        k++;
    }
    /* q = num / den, remainder -> sticky */
    memset(q, 0, BIGN);
    bl = bitlen(num) - bitlen(den);
    for (i = 0; i < bl; i++)
        bigshl(den);
    for (i = bl; i >= 0; i--) {
        bigshl(q);
        if (bigcmp(num, den) >= 0) {
            bigsub(num, den);
            q[0] = q[0] | 1;
        }
        /* den >>= 1 */
        {
            int j;
            int c;
            c = 0;
            for (j = BIGN - 1; j >= 0; j--) {
                int v;
                v = den[j];
                den[j] = (v >> 1) | (c << 7);
                c = v & 1;
            }
        }
    }
    sticky = bitlen(num) != 0;
    top = bitlen(q);                    /* value = q * 2^-k, q has top bits */
    /* take bits top-1 .. top-24 as the mantissa */
    m1 = 0;
    m2 = 0;
    m3 = 0;
    for (i = 0; i < 8; i++) {
        m1 = m1 * 2 + getbit(q, top - 1 - i);
        m2 = m2 * 2 + getbit(q, top - 9 - i);
        m3 = m3 * 2 + getbit(q, top - 17 - i);
    }
    round = getbit(q, top - 25);
    e2 = top - k;                       /* value = 0.mmm * 2^e2 */
    if (round) {
        m3++;
        if (m3 == 256) {
            m3 = 0;
            m2++;
            if (m2 == 256) {
                m2 = 0;
                m1++;
                if (m1 == 256) {
                    m1 = 128;
                    e2++;
                }
            }
        }
    }
    if (e2 + 128 > 255) {
        error(27 /* floating constant too large */, 0);
        return;
    }
    if (e2 + 128 < 1)
        return;                         /* underflow: 0.0 */
    out[0] = e2 + 128;
    out[1] = m1 & 127;
    out[2] = m2;
    out[3] = m3;
}

/* The preprocessor's output (pp.c): the real constant at s (a decimal
   number with a '.' or an exponent, as number() reads it) as the token
   `HHHHHHHH?TEXT` in out -- its float image in hex (tokreal), L or S
   (the L suffix), the "DIGITSeEXP" text a double is made from (toknum).
   The compiling pass then needs no conversion (and not this segment,
   which deep in an expression took memory where it is scarcest).
   Returns the characters of s used, 0 when s is not a real constant. */
int realtoken(char *s, char *out)
{
    unsigned char digits[24];
    unsigned char img[4];
    char *p;
    int nd;
    int exp10;
    int esign;
    int ev;
    int islong;
    int i;
    p = s;
    nd = 0;
    exp10 = 0;
    if (p[0] == '0' && (p[1] == 'x' || p[1] == 'X'))
        return 0;
    while (*p >= '0' && *p <= '9') {
        if (nd > 0 || *p != '0') {
            if (nd < 24)
                digits[nd++] = *p - '0';
            else
                exp10++;
        }
        p++;
    }
    if (*p != '.' && *p != 'e' && *p != 'E')
        return 0;
    if (*p == '.') {
        p++;
        while (*p >= '0' && *p <= '9') {
            if (nd > 0 || *p != '0') {
                if (nd < 24) {
                    digits[nd++] = *p - '0';
                    exp10--;
                }
            } else
                exp10--;
            p++;
        }
    }
    if (*p == 'e' || *p == 'E') {
        p++;
        esign = 1;
        if (*p == '-') {
            esign = -1;
            p++;
        } else if (*p == '+')
            p++;
        ev = 0;
        while (*p >= '0' && *p <= '9') {
            if (ev < 1000)
                ev = ev * 10 + *p - '0';
            p++;
        }
        exp10 = exp10 + esign * ev;
    }
    islong = 0;
    while (*p == 'f' || *p == 'F' || *p == 'l' || *p == 'L') {
        if (*p == 'l' || *p == 'L')
            islong = 1;
        p++;
    }
    *out++ = '`';
    if (exp10 + nd > 40 && !islong)
        error(27 /* floating constant too large */, 0);
    {
        int k;
        k = nd;
        if (exp10 + nd < -40 || exp10 + nd > 40)
            k = 0;
        makereal(digits, k, exp10, img);
    }
    for (i = 0; i < 4; i++) {
        *out++ = "0123456789ABCDEF"[img[i] >> 4];
        *out++ = "0123456789ABCDEF"[img[i] & 15];
    }
    *out++ = islong ? 'L' : 'S';
    for (i = 0; i < nd; i++)
        *out++ = '0' + digits[i];
    if (nd == 0)
        *out++ = '0';
    *out++ = 'e';
    itoa10(nd ? exp10 : 0, out);
    out = out + strlen(out);
    *out++ = '`';
    *out = 0;
    return p - s;
}

#pragma segment PARSE

static void number(void)
{
    unsigned char acc[4];
    unsigned char digits[24];
    int nd;
    int base;
    int d;
    int ovf;
    int isfloat;
    int exp10;
    int esign;
    int ev;
    int isunsigned;
    int islong;
    memset(acc, 0, 4);
    ovf = 0;
    base = 10;
    isfloat = 0;
    nd = 0;
    exp10 = 0;
    if (ch == '0') {
        nextch();
        if (ch == 'x' || ch == 'X') {
            base = 16;
            nextch();
        } else
            base = 8;
    }
    for (;;) {
        if (ch >= '0' && ch <= '9')
            d = ch - '0';
        else if (base == 16 && ch >= 'a' && ch <= 'f')
            d = ch - 'a' + 10;
        else if (base == 16 && ch >= 'A' && ch <= 'F')
            d = ch - 'A' + 10;
        else
            break;
        if (base != 16 && (nd > 0 || d > 0)) {
            if (nd < 24)
                digits[nd++] = d;
            else
                exp10++;
        }
        ovf = ovf | mac(acc, base == 8 ? 10 : base, d);
        nextch();
    }
    if (base != 16 && (ch == '.' || ch == 'e' || ch == 'E')) {
        isfloat = 1;
        if (ch == '.') {
            nextch();
            while (ch >= '0' && ch <= '9') {
                if (nd > 0 || ch != '0') {
                    if (nd < 24) {
                        digits[nd++] = ch - '0';
                        exp10--;
                    }
                } else
                    exp10--;
                nextch();
            }
        }
        if (ch == 'e' || ch == 'E') {
            nextch();
            esign = 1;
            if (ch == '-') {
                esign = -1;
                nextch();
            } else if (ch == '+')
                nextch();
            ev = 0;
            while (ch >= '0' && ch <= '9') {
                if (ev < 1000)
                    ev = ev * 10 + ch - '0';
                nextch();
            }
            exp10 = exp10 + esign * ev;
        }
        toklong = 0;                    /* the L suffix: a double constant */
        while (ch == 'f' || ch == 'F' || ch == 'l' || ch == 'L') {
            if (ch == 'l' || ch == 'L')
                toklong = 1;
            nextch();
        }
        {
            /* the text a double's image is made from: "DIGITSeEXP" */
            int i;
            for (i = 0; i < nd; i++)
                toknum[i] = '0' + digits[i];
            if (nd == 0)
                toknum[i++] = '0';
            toknum[i++] = 'e';
            itoa10(nd ? exp10 : 0, toknum + i);
        }
        if (exp10 + nd > 40 && !toklong)
            error(27 /* floating constant too large */, 0);
        if (exp10 + nd < -40 || exp10 + nd > 40)
            nd = 0;
        makereal(digits, nd, exp10, tokreal);
        tok = T_FNUM;
        return;
    }
    if (base == 8) {                    /* redo in octal */
        int i;
        memset(acc, 0, 4);
        ovf = 0;
        for (i = 0; i < nd; i++)
            ovf = ovf | mac(acc, 8, digits[i]);
    }
    isunsigned = 0;
    islong = 0;
    for (;;) {
        if (ch == 'u' || ch == 'U')
            isunsigned = 1;
        else if (ch == 'l' || ch == 'L')
            islong = 1;
        else
            break;
        nextch();
    }
    if (ovf)
        error(28 /* integer constant too large */, 0);
    tokval = W16(acc[0] + acc[1] * 256);
    tokval2 = W16(acc[2] + acc[3] * 256);
    if (acc[2] || acc[3])
        islong = 1;
    if (!islong && (acc[1] & 128)) {
        if (base == 10 && !isunsigned)
            islong = 1;
        else
            isunsigned = 1;
    }
    if (islong && !isunsigned && (acc[3] & 128) && base != 10)
        isunsigned = 1;
    toklong = islong + isunsigned * 2;
    tok = T_NUM;
}

static void rawnext(void)
{
    int n;
    int c;
    char *buf;
    int cap;
    for (;;) {
        if (ch == '\n') {
            curline++;
            atbol = 1;
            nextch();
            continue;
        }
        if (ch == ' ' || ch == '\t' || ch == '\r' || ch == 12) {
            nextch();
            continue;
        }
        if (ch == '#' && atbol) {
            lexdirective();
            continue;
        }
        break;
    }
    atbol = 0;
    if (ch == EOF) {
        tok = T_EOF;
        return;
    }
    if ((ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') || ch == '_') {
        int lo;
        int hi;
        int mid;
        int r;
        n = 0;
        while ((ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') || ch == '_' || (ch >= '0' && ch <= '9')) {
            if (n < MAXNAME - 1)
                tokname[n++] = ch;
            nextch();
        }
        tokname[n] = 0;
        lo = 0;
        hi = K_LAST - K_FIRST;
        while (lo <= hi) {
            mid = (lo + hi) / 2;
            r = strcmp(tokname, kwtab[mid]);
            if (r == 0) {
                tok = K_FIRST + mid;
                return;
            }
            if (r < 0)
                hi = mid - 1;
            else
                lo = mid + 1;
        }
        tok = T_ID;
        return;
    }
    if (ch >= '0' && ch <= '9') {
        number();
        return;
    }
    if (ch == '`') {
        /* a real constant the preprocessor converted (realtoken) */
        int i;
        int h;
        for (i = 0; i < 8; i++) {
            nextch();
            h = ch <= '9' ? ch - '0' : ch - 'A' + 10;
            if (i & 1)
                tokreal[i >> 1] = tokreal[i >> 1] | h;
            else
                tokreal[i >> 1] = h << 4;
        }
        nextch();
        toklong = ch == 'L';
        nextch();
        for (i = 0; ch != '`' && ch != '\n' && ch != EOF && i < 39; i++) {
            toknum[i] = ch;
            nextch();
        }
        toknum[i] = 0;
        nextch();
        tok = T_FNUM;
        return;
    }
    if (ch == '\'') {
        nextch();
        if (ch == '\\')
            tokval = escape();
        else {
            tokval = ch;
            nextch();
        }
        if (ch != '\'')
            error(29 /* bad character constant */, 0);
        nextch();
        tokval = tokval & 255;
        if (tokval > 127)
            tokval = tokval - 256;
        tokval2 = 0;
        toklong = 0;
        tok = T_NUM;
        return;
    }
    if (ch == '"') {
        /* two buffers: the current token and one of lookahead */
        strslot = !strslot;
        buf = strbufs[strslot];
        cap = MAXSTR;
        n = 0;
        for (;;) {
            nextch();
            while (ch != '"') {
                if (ch == '\n' || ch == EOF) {
                    error(30 /* unterminated string */, 0);
                    break;
                }
                if (ch == '\\')
                    c = escape();
                else {
                    c = ch;
                    nextch();
                }
                if (n >= cap - 1) {
                    error(31 /* string literal too long */, 0);
                    n = 0;
                }
                buf[n++] = c;
            }
            nextch();
            /* adjacent string literals are concatenated */
            while (ch == ' ' || ch == '\t' || ch == '\n' || ch == '\r') {
                if (ch == '\n')
                    curline++;
                nextch();
            }
            if (ch != '"')
                break;
        }
        buf[n++] = 0;
        tokstr = buf;
        toklen = n;
        tok = T_STR;
        return;
    }
    c = ch;
    nextch();
    switch (c) {
    case '-':
        if (ch == '>') { nextch(); tok = T_ARROW; return; }
        if (ch == '-') { nextch(); tok = T_DEC; return; }
        if (ch == '=') { nextch(); tok = T_SUBA; return; }
        break;
    case '+':
        if (ch == '+') { nextch(); tok = T_INC; return; }
        if (ch == '=') { nextch(); tok = T_ADDA; return; }
        break;
    case '*':
        if (ch == '=') { nextch(); tok = T_MULA; return; }
        break;
    case '/':
        if (ch == '=') { nextch(); tok = T_DIVA; return; }
        break;
    case '%':
        if (ch == '=') { nextch(); tok = T_MODA; return; }
        break;
    case '&':
        if (ch == '&') { nextch(); tok = T_ANDAND; return; }
        if (ch == '=') { nextch(); tok = T_ANDA; return; }
        break;
    case '|':
        if (ch == '|') { nextch(); tok = T_OROR; return; }
        if (ch == '=') { nextch(); tok = T_ORA; return; }
        break;
    case '^':
        if (ch == '=') { nextch(); tok = T_XORA; return; }
        break;
    case '<':
        if (ch == '<') {
            nextch();
            if (ch == '=') { nextch(); tok = T_SHLA; return; }
            tok = T_SHL;
            return;
        }
        if (ch == '=') { nextch(); tok = T_LE; return; }
        break;
    case '>':
        if (ch == '>') {
            nextch();
            if (ch == '=') { nextch(); tok = T_SHRA; return; }
            tok = T_SHR;
            return;
        }
        if (ch == '=') { nextch(); tok = T_GE; return; }
        break;
    case '=':
        if (ch == '=') { nextch(); tok = T_EQ; return; }
        break;
    case '!':
        if (ch == '=') { nextch(); tok = T_NE; return; }
        break;
    case '.':
        if (ch == '.') {
            nextch();
            if (ch == '.') { nextch(); tok = T_ELLIPSIS; return; }
            error(32 /* bad token '..' */, 0);
        }
        break;
    }
    tok = c;
}

void next(void)
{
    if (havepeek) {
        havepeek = 0;
        tok = ptok;
        tokval = pval;
        tokval2 = pval2;
        toklong = plong;
        strcpy(tokname, pname);
        return;
    }
    rawnext();
}

/* look at the token after the current one (identifiers and punctuators only) */
int peek(void)
{
    int t;
    int v;
    int v2;
    int l;
    char name[MAXNAME];
    if (havepeek)
        return ptok;
    t = tok;
    v = tokval;
    v2 = tokval2;
    l = toklong;
    strcpy(name, tokname);
    rawnext();
    ptok = tok;
    pval = tokval;
    pval2 = tokval2;
    plong = toklong;
    strcpy(pname, tokname);
    havepeek = 1;
    tok = t;
    tokval = v;
    tokval2 = v2;
    toklong = l;
    strcpy(tokname, name);
    return ptok;
}

char *peekname(void)
{
    return pname;
}
