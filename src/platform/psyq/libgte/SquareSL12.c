#include "psx/gte_inline.h"
#include "psx/libgte.h"

/* main 0x8001d488–0x8001d4b8: short input, wide IR output, SQR sf=1. */
VECTOR* SquareSL12(SVECTOR* input, VECTOR* output) {
    PSYQ_GTE_SVECTOR_SQUARE_LONG(input, output, PSYQ_GTE_CMD_SQUARE_12);
    return output;
}
