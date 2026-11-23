/* Resident coordinate setters used by battle movement and rendering. Retail
 * calls these out-of-line functions even though PsyQ provides the same stores. */

#include "fft/sys/math.h"
#include "psx/libgpu.h"

/* Store x, y and z in an SVECTOR, truncated to 16 bits. */
void vector_set_short(SVECTOR* vector, s32 x, s32 y, s32 z) {
    setVector(vector, x, y, z);
}

/* Store x, y and z in a VECTOR. */
void vector_set_long(VECTOR* vector, s32 x, s32 y, s32 z) {
    setVector(vector, x, y, z);
}
