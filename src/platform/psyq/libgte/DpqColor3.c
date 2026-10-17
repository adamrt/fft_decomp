#include "psx/gte_inline.h"
#include "psx/libgte.h"

/* main 0x8001d228–0x8001d264: DPCT takes the color code from c2. */
void DpqColor3(CVECTOR* c0, CVECTOR* c1, CVECTOR* c2, s32 depth, CVECTOR* out0, CVECTOR* out1, CVECTOR* out2) {
    PSYQ_GTE_DPCT(c0, c1, c2, depth, out0, out1, out2);
}
