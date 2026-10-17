#include "psx/gte_inline.h"
#include "psx/libgte.h"

/* main 0x8001d4e8–0x8001d514: RTPS returns SZ3/4. */
s32 RotTransPers(SVECTOR* input, s32* xy, s32* p, s32* flag) {
    s32 depth;
    PSYQ_GTE_ROTPERSP(input, xy, p, flag, depth);
    return depth >> 2;
}
