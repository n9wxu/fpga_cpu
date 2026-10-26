#include "t.h"
#include <math.h>

int main(void)
{
    static const float xs[] = {0.0f, 0.1f, 0.5f, 1.0f, 1.5f, 2.0f, 3.0f, 3.14159f, 10.0f, 100.0f, -0.7f, -2.5f, 1e-3f, 12345.6f};
    char nm[16];
    for (unsigned i = 0; i < sizeof xs / sizeof xs[0]; i++) {
        float x = xs[i];
        snprintf(nm, sizeof nm, "sin%u", i);   PF(nm, sinf(x));
        snprintf(nm, sizeof nm, "cos%u", i);   PF(nm, cosf(x));
        snprintf(nm, sizeof nm, "atan%u", i);  PF(nm, atanf(x));
        snprintf(nm, sizeof nm, "exp%u", i);   PF(nm, x < 80 ? expf(x) : 0.0f);
        if (x > 0) {
            snprintf(nm, sizeof nm, "log%u", i);  PF(nm, logf(x));
            snprintf(nm, sizeof nm, "sqrt%u", i); PF(nm, sqrtf(x));
            snprintf(nm, sizeof nm, "pow%u", i);  PF(nm, powf(x, 1.7f));
        }
        if (fabsf(cosf(x)) > 0.01f) {
            snprintf(nm, sizeof nm, "tan%u", i); PF(nm, tanf(x));
        }
        snprintf(nm, sizeof nm, "floor%u", i); PF(nm, floorf(x));
        snprintf(nm, sizeof nm, "ceil%u", i);  PF(nm, ceilf(x));
        snprintf(nm, sizeof nm, "fmod%u", i);  PF(nm, fmodf(x, 0.75f));
    }
    PF("pow2^10", powf(2.0f, 10.0f));
    PF("pow-2^3", powf(-2.0f, 3.0f));
    PF("atan2", atan2f(-1.0f, -1.0f));
    PF("asin", asinf(0.5f));
    PF("acos", acosf(0.5f));
    PF("sinbig", sinf(1000.0f));
    return 0;
}
