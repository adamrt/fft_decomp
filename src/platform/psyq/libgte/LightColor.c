#include "psx/gte_inline.h"
#include "psx/libgte.h"

/* main 0x8001d1d8–0x8001d200: MVMVA color matrix, IR input, background. */
void LightColor(VECTOR* input, VECTOR* output) {
    PSYQ_GTE_IR_VECTOR(
        input, output, PSYQ_GTE_CMD_LIGHT_COLOR, PSYQ_GTE_DATA_IR1, PSYQ_GTE_DATA_IR2, PSYQ_GTE_DATA_IR3);
}
