#include "psx/gte_inline.h"
#include "psx/libgte.h"

/* main 0x8001d2f8–0x8001d31c: AVSZ4 returns OTZ. */
s32 AverageZ4(s32 z0, s32 z1, s32 z2, s32 z3) {
    s32 result;
    PSYQ_GTE_AVSZ4(z0, z1, z2, z3, result);
    return result;
}
