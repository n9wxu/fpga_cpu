/* fpga_cpu C runtime: <math.h>. float and double are the same 32-bit
 * IEEE type on this target, so the double names are aliases. */
#ifndef _MATH_H
#define _MATH_H

#define HUGE_VALF __builtin_huge_valf()
#define HUGE_VAL  __builtin_huge_valf()
#define INFINITY  __builtin_inff()
#define NAN       __builtin_nanf("")
#define M_PI      3.14159265358979f
#define M_E       2.71828182845905f

#define isnan(x)    __builtin_isnan(x)
#define isinf(x)    __builtin_isinf(x)
#define isfinite(x) __builtin_isfinite(x)
#define signbit(x)  __builtin_signbit(x)

float fabsf(float x);
float floorf(float x);
float ceilf(float x);
float truncf(float x);
float roundf(float x);
float fmodf(float x, float y);
float sqrtf(float x);
float expf(float x);
float logf(float x);
float log10f(float x);
float powf(float x, float y);
float sinf(float x);
float cosf(float x);
float tanf(float x);
float atanf(float x);
float atan2f(float y, float x);
float asinf(float x);
float acosf(float x);
float ldexpf(float x, int e);
float frexpf(float x, int *e);

#define fabs  fabsf
#define floor floorf
#define ceil  ceilf
#define trunc truncf
#define round roundf
#define fmod  fmodf
#define sqrt  sqrtf
#define exp   expf
#define log   logf
#define log10 log10f
#define pow   powf
#define sin   sinf
#define cos   cosf
#define tan   tanf
#define atan  atanf
#define atan2 atan2f
#define asin  asinf
#define acos  acosf
#define ldexp ldexpf
#define frexp frexpf

#endif
