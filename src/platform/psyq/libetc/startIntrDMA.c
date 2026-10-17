#include "psx/libetc.h"

/* Start the DMA dispatcher: clear the DMA callback slots and the DMA interrupt register and
 * install trapIntrDMA. Returns setIntrDMA for the slot setter table. */
psyq_callback_setter_t startIntrDMA(void) {
    psyq_etc_clear_dma_callbacks(g_psyq_etc_dma_callbacks, PSYQ_ETC_CALLBACK_SLOTS);
    *g_psyq_etc_dma_dicr = 0;
    InterruptCallback(PSYQ_IRQ_DMA, trapIntrDMA);
    return setIntrDMA;
}
