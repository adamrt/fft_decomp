#include "psx/libpress.h"

/* Start receiving size 32-bit values of decoded image data into buf (MDEC_out). */
void DecDCTout(u32* buf, s32 size) {
    MDEC_out(buf, size);
}
