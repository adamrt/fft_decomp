#include "psx/gte_inline.h"
#include "psx/libgte.h"

/* main 0x8001cc44–0x8001cc8c: read rotation/translation. */
void ReadRotMatrix(MATRIX* output) {
    PSYQ_GTE_READ_MATRIX(output, PSYQ_GTE_CTRL_R11_R12, PSYQ_GTE_CTRL_R13_R21, PSYQ_GTE_CTRL_R22_R23,
        PSYQ_GTE_CTRL_R31_R32, PSYQ_GTE_CTRL_R33, PSYQ_GTE_CTRL_TRX, PSYQ_GTE_CTRL_TRY, PSYQ_GTE_CTRL_TRZ);
}
