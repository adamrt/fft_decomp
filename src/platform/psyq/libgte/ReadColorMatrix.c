#include "psx/gte_inline.h"
#include "psx/libgte.h"

/* main 0x8001ccd4–0x8001cd1c: read color/far color. */
void ReadColorMatrix(MATRIX* output) {
    PSYQ_GTE_READ_MATRIX(output, PSYQ_GTE_CTRL_LR1_LR2, PSYQ_GTE_CTRL_LR3_LG1, PSYQ_GTE_CTRL_LG2_LG3,
        PSYQ_GTE_CTRL_LB1_LB2, PSYQ_GTE_CTRL_LB3, PSYQ_GTE_CTRL_RFC, PSYQ_GTE_CTRL_GFC, PSYQ_GTE_CTRL_BFC);
}
