#include "psx/gte_inline.h"
#include "psx/libgte.h"

/* main 0x8001d4b8–0x8001d4e8: short input, wide IR output, SQR sf=0. */
VECTOR* SquareSL0(SVECTOR* input, VECTOR* output) {
    PSYQ_GTE_SVECTOR_SQUARE_LONG(input, output, PSYQ_GTE_CMD_SQUARE_0);
    return output;
}
