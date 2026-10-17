#include "psx/gte_inline.h"
#include "psx/libgte.h"

/* main 0x8001d2b0–0x8001d2d8: square IR vector, sf=0. */
VECTOR* Square0(VECTOR* input, VECTOR* output) {
    register VECTOR* destination __asm__("$5") = output; /* Retail uses a1 until the return delay slot. */
    PSYQ_GTE_IR_VECTOR(
        input, destination, PSYQ_GTE_CMD_SQUARE_0, PSYQ_GTE_DATA_MAC1, PSYQ_GTE_DATA_MAC2, PSYQ_GTE_DATA_MAC3);
    return output;
}
