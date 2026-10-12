/* sed.h -- what SED.C and SEDRE.C share: the regular expression engine's interface */

/* re_compile: pat (BRE; ERE if ere) -> a regular expression number, -1 on a mistake (re_err says);
   re_exec: the first match in s at or after offset from: 1 and the match in re_ms..re_me (offsets) and
   the groups in re_cs[n]..re_ce[n] (-1: did not take part), 0 if none */
int re_compile(char *pat, int ere, int icase);
int re_exec(int re, char *s, int from);
int re_groups(int re);                  /* the number of groups in it */
extern char *re_err;
extern int re_ms, re_me;
extern int re_cs[10], re_ce[10];
