#include "psx/gte_inline.h"
#include "psx/libgte.h"

/* main 0x8001d578–0x8001d5a0; v0 contains the FLAG value. */
void RotTrans(SVECTOR* input, VECTOR* output, long* flag) {
    long result;
    PSYQ_GTE_ROTTRANS(input, output, result);
    *flag = result;
}
