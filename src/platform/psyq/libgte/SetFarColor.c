#include "psx/gte_inline.h"
#include "psx/libgte.h"

/* main 0x8001d188–0x8001d1a8. */
void SetFarColor(s32 r, s32 g, s32 b) {
    r <<= 4;
    g <<= 4;
    b <<= 4;
    PSYQ_GTE_CTC2(PSYQ_GTE_CTRL_RFC, r);
    PSYQ_GTE_CTC2(PSYQ_GTE_CTRL_GFC, g);
    PSYQ_GTE_CTC2(PSYQ_GTE_CTRL_BFC, b);
}
