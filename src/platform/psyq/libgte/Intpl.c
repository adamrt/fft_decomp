#include "psx/gte_inline.h"
#include "psx/libgte.h"

/* main 0x8001d264–0x8001d288: INTPL emits RGB2 including input code effects. */
void Intpl(VECTOR* input, s32 depth, CVECTOR* output) {
    PSYQ_GTE_INTPLC(input, depth, output);
}
