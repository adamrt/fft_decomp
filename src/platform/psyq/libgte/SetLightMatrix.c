#include "psx/gte_inline.h"
#include "psx/libgte.h"

/* main 0x8001d0d8–0x8001d108. */
void SetLightMatrix(MATRIX* matrix) {
    PSYQ_GTE_LOAD_MATRIX(matrix, PSYQ_GTE_CTRL_L11_L12, PSYQ_GTE_CTRL_L13_L21, PSYQ_GTE_CTRL_L22_L23,
        PSYQ_GTE_CTRL_L31_L32, PSYQ_GTE_CTRL_L33);
}
