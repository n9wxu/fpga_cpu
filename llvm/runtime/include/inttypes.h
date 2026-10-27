/* fpga_cpu C runtime: <inttypes.h>. The format macros follow clang's
 * predefined ones, so they are right with a 16-bit or a 32-bit int. */
#ifndef _INTTYPES_H
#define _INTTYPES_H
#include <stdint.h>
#define PRId8  __INT8_FMTd__
#define PRId16 __INT16_FMTd__
#define PRId32 __INT32_FMTd__
#define PRId64 __INT64_FMTd__
#define PRIi32 __INT32_FMTi__
#define PRIu8  __UINT8_FMTu__
#define PRIu16 __UINT16_FMTu__
#define PRIu32 __UINT32_FMTu__
#define PRIu64 __UINT64_FMTu__
#define PRIx16 __UINT16_FMTx__
#define PRIx32 __UINT32_FMTx__
#define PRIx64 __UINT64_FMTx__
#define PRIX32 __UINT32_FMTX__
#define PRIdPTR __INTPTR_FMTd__
#define PRIuPTR __UINTPTR_FMTu__
#define PRIxPTR __UINTPTR_FMTx__
#endif
