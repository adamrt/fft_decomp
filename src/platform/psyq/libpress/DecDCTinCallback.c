#include "psx/libetc.h"
#include "psx/libpress.h"
#include "psx/types.h"

/* Install the MDEC input DMA completion callback and return the previous one. (DecDCToutCallback
 * at 0x800736f8: the same for the output channel.). */
s32 DecDCTinCallback(psyq_interrupt_callback_t callback) {
    /* Returns the previous callback address as a signed 32-bit value. */
    return (s32)DMACallback(PSYQ_DMA_MDEC_IN_CHANNEL, callback);
}
