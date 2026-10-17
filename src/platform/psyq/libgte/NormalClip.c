#include "psx/gte_inline.h"
#include "psx/libgte.h"

/* main 0x8001d5a8–0x8001d5cc. */
s32 NormalClip(s32 a, s32 b, s32 c) {
    s32 result;
    PSYQ_GTE_NORMALCLIP(a, b, c, result);
    return result;
}
