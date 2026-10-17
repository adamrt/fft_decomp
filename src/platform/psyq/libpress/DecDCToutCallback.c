#include "psx/libetc.h"
#include "psx/libpress.h"
#include "psx/types.h"

s32 DecDCToutCallback(psyq_interrupt_callback_t callback) {
    /* Returns the previous callback address as a signed 32-bit value. */
    return (s32)DMACallback(PSYQ_DMA_MDEC_OUT_CHANNEL, callback);
}
