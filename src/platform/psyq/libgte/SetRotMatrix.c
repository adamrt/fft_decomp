#include "psx/gte_inline.h"
#include "psx/libgte.h"

/* main 0x8001d0a8–0x8001d0d8. */
void SetRotMatrix(MATRIX* matrix) {
    PSYQ_GTE_LOAD_MATRIX(matrix, PSYQ_GTE_CTRL_R11_R12, PSYQ_GTE_CTRL_R13_R21, PSYQ_GTE_CTRL_R22_R23,
        PSYQ_GTE_CTRL_R31_R32, PSYQ_GTE_CTRL_R33);
}
