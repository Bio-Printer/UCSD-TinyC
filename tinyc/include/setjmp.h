/* setjmp.h -- Tiny-C
 * setjmp(env) saves the mark stack control word of its own call -- the
 * caller's frame, segment, return address and stack depth -- in env and
 * returns 0.  longjmp(env, val) returns from that setjmp call again, now
 * with val (1 if val is 0), leaving every function called since.  The
 * function that called setjmp must not have returned yet.  As in any C,
 * use setjmp only as a whole condition (if, while, switch; ! or a compare
 * with a constant), or as a whole statement.  Local variables keep the
 * values they had at the longjmp (they all live in memory here).
 * longjmp gives back the code segments of the functions it leaves, as
 * their returns would have. */
#ifndef __SETJMP_H
#define __SETJMP_H

/* the caller's MP, JTAB, SEGP, the return IPC and SP */
typedef int jmp_buf[5];

int setjmp(jmp_buf env);

void longjmp(jmp_buf env, int val);

#endif
