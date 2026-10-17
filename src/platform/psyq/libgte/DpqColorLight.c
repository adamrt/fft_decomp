#include "psx/gte_inline.h"
#include "psx/libgte.h"

/* main 0x8001d200–0x8001d228: DCPL combines IR vector, RGB and depth. */
void DpqColorLight(VECTOR* input, CVECTOR* color, s32 depth, CVECTOR* output) {
    PSYQ_GTE_DCPL(input, color, depth, output);
}
