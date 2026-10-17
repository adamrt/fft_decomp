#include "psx/libpress.h"
#include "psx/types.h"

/* Dump the MDEC status and DMA state to the console, then force the decoder
 * back into its idle configuration. */
s32 timeout(const char* tag) {
    u32 status;

    printf(g_psyq_press_timeout_format, tag);
    status = *g_psyq_press_mdec_status;
    printf(g_psyq_press_timeout_dma_format, (*g_psyq_press_dma_in_chcr >> 24) & 1,
        (*g_psyq_press_dma_out_chcr >> 24) & 1, *g_psyq_press_dma_in_madr, *g_psyq_press_dma_out_madr);
    printf(g_psyq_press_timeout_fifo_format, ~status >> 31, (status >> 30) & 1, (status >> 29) & 1, (status >> 28) & 1,
        (status >> 27) & 1, (status >> 25) & 1, (status >> 23) & 1);
    /* The same sequence as MDEC_reset(1). */
    *g_psyq_press_mdec_status = PSYQ_MDEC_CONTROL_RESET;
    *g_psyq_press_dma_in_chcr = 0;
    *g_psyq_press_dma_out_chcr = 0;
    *g_psyq_press_dma_out_chcr;
    *g_psyq_press_mdec_status = PSYQ_MDEC_CONTROL_DMA_IN | PSYQ_MDEC_CONTROL_DMA_OUT;
    return 0;
}
