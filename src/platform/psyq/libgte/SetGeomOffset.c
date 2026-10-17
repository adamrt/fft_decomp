#include "psx/gte_inline.h"
#include "psx/libgte.h"

/* main 0x8001d1a8–0x8001d1c0. */
void SetGeomOffset(int x, int y) {
    x <<= 16;
    y <<= 16;
    PSYQ_GTE_CTC2(PSYQ_GTE_CTRL_OFX, x);
    PSYQ_GTE_CTC2(PSYQ_GTE_CTRL_OFY, y);
}
