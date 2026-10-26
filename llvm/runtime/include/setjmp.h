/* fpga_cpu C runtime: <setjmp.h>. Saves p4, p5, lr and sp. */
#ifndef _SETJMP_H
#define _SETJMP_H

typedef int jmp_buf[8];

int setjmp(jmp_buf env) __attribute__((returns_twice));
void longjmp(jmp_buf env, int val) __attribute__((noreturn));

#endif
