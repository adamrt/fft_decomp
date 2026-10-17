#include "psx/gte_inline.h"
#include "psx/libgte.h"

/* main 0x8001d374–0x8001d3cc: outer product sf=0; restores matrix diagonal. */
void OuterProduct0(VECTOR* a, VECTOR* b, VECTOR* output) {
    PSYQ_GTE_OUTER_PRODUCT(a, b, output, PSYQ_GTE_CMD_OUTER_PRODUCT_0);
}
