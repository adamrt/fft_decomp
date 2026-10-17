#include "psx/gte_inline.h"
#include "psx/libgte.h"

/* Rotate an SVECTOR into saturated IR outputs and return the third argument unchanged. */
u32 psyq_gte_apply_rotation_ir(SVECTOR* input, VECTOR* output, u32 return_value) {
    u32 saved_return = return_value;
    u32 result;
    PSYQ_GTE_APPLY_SHORT(input, output);
    __asm__("" : "=r"(result) : "0"(saved_return));
    return result;
}
