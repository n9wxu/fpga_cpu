/* Host build without the JIT: the hooks the interpreter needs. */
#include "sid6502.h"
void smc_write(uint16_t a) { (void)a; }
void jit_flush(void) {}
