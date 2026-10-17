#include "psx/gte_inline.h"
#include "psx/libgte.h"

/* main 0x8001d450–0x8001d488: short-vector SQR, sf=0. */
SVECTOR* SquareSS0(SVECTOR* input, SVECTOR* output) {
    PSYQ_GTE_SVECTOR_SQUARE(input, output, PSYQ_GTE_CMD_SQUARE_0);
    return output;
}
