#include "psx/gte_inline.h"
#include "psx/libgte.h"

/* main 0x8001d518–0x8001d56c: RTPT returns last SZ3/4. */
s32 RotTransPers3(SVECTOR* v0, SVECTOR* v1, SVECTOR* v2, s32* xy0, s32* xy1, s32* xy2, s32* p, s32* flag) {
    s32 depth;
    PSYQ_GTE_ROTPERSP3(v0, v1, v2, xy0, xy1, xy2, p, flag, depth);
    return depth >> 2;
}
