#include "psx/gte_inline.h"
#include "psx/libgte.h"

/* main 0x8001c574–0x8001c658: current GTE rotation times matrix. */
MATRIX* MulRotMatrix0(MATRIX* matrix, MATRIX* output) {
    /* The destination and returned pointer retain the handwritten a1/v0 transport. */
    register MATRIX* destination __asm__("$5") = output;
    register MATRIX* result __asm__("$2");
    PSYQ_GTE_MATRIX_COLUMNS(matrix);
    PSYQ_GTE_MATRIX_STORE(destination);
    result = output;
    __asm__ volatile("" : : "r"(result));
    return result;
}
