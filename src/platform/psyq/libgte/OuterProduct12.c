#include "psx/gte_inline.h"
#include "psx/libgte.h"

/* main 0x8001d31c–0x8001d374: outer product sf=1; restores matrix diagonal. */
void OuterProduct12(VECTOR* a, VECTOR* b, VECTOR* output) {
    PSYQ_GTE_OUTER_PRODUCT(a, b, output, PSYQ_GTE_CMD_OUTER_PRODUCT_12);
}
