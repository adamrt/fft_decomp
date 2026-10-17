#include "psx/gte_inline.h"
#include "psx/libgte.h"

/* main 0x8001d418–0x8001d450: short-vector SQR, sf=1. */
SVECTOR* SquareSS12(SVECTOR* input, SVECTOR* output) {
    PSYQ_GTE_SVECTOR_SQUARE(input, output, PSYQ_GTE_CMD_SQUARE_12);
    return output;
}
