/* Resident coordinate setters used by battle movement and rendering. Retail
 * calls these out-of-line functions even though PsyQ provides the same stores. */

#include "fft/vector.h"
#include "psx/libgpu.h"

/* Store x, y and z in an SVECTOR, truncated to 16 bits. */
void vector_short_set(SVECTOR* vector, s32 x, s32 y, s32 z) {
    setVector(vector, x, y, z);
}

/* Store x, y and z in a VECTOR. */
void vector_long_set(VECTOR* vector, s32 x, s32 y, s32 z) {
    setVector(vector, x, y, z);
}
