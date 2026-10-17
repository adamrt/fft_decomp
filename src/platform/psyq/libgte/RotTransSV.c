#include "psx/gte_inline.h"
#include "psx/libgte.h"

/* main 0x8001d3e8–0x8001d418: IR output, including a 32-bit IR3 store. */
void RotTransSV(SVECTOR* input, SVECTOR* output, s32* flag) {
    s32 result;
    PSYQ_GTE_ROTTRANSSV(input, output, result);
    *flag = result;
}
