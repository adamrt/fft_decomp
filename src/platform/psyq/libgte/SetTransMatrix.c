#include "psx/gte_inline.h"
#include "psx/libgte.h"

/* main 0x8001d138–0x8001d158. */
void SetTransMatrix(MATRIX* matrix) {
    PSYQ_GTE_LOAD_TRANSLATION(matrix);
}
