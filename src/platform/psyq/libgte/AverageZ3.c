#include "psx/gte_inline.h"
#include "psx/libgte.h"

/* main 0x8001d2d8–0x8001d2f8: AVSZ3 returns OTZ. */
s32 AverageZ3(s32 z0, s32 z1, s32 z2) {
    s32 result;
    PSYQ_GTE_AVSZ3(z0, z1, z2, result);
    return result;
}
