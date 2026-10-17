#include "psx/libpress.h"

/* Set the MDEC output-depth and mask bits of the 32-bit command from mode,
 * then start the input DMA. */
void DecDCTin(u32* buf, s32 mode) {
    if (mode & PSYQ_PRESS_MODE_RGB24) {
        *buf &= ~PSYQ_MDEC_COMMAND_RGB16;
    } else {
        *buf |= PSYQ_MDEC_COMMAND_RGB16;
    }
    if (mode & PSYQ_PRESS_MODE_SET_BIT15) {
        *buf |= PSYQ_MDEC_COMMAND_SET_BIT15;
    } else {
        *buf &= ~PSYQ_MDEC_COMMAND_SET_BIT15;
    }
    MDEC_in(buf, (u16)*buf);
}
