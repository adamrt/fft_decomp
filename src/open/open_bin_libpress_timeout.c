#include "fft/open.h"
#include "psx/types.h"

/* OPEN.BIN LIBPRESS timeout - dump the MDEC status and DMA state to the
 * console, then force the decoder back into its idle configuration. */
s32 open_bin_libpress_timeout(const char* tag) {
    u32 status;
    s32 result;
    /* Unpinned, the `result = 0` set sinks below the MDEC reset and the reset
     * uses $v0/$v1 instead of the target's $v1/$a0. */
    register u32 decoder_status __asm__("$3");
    register volatile u32* decoder_control __asm__("$4");

    printf(g_open_text_mdec_timeout_format, tag);
    status = *g_open_mdec_status_reg;
    printf(g_open_text_mdec_timeout_dma_format, (*g_open_dma_mdec_in_chcr >> 24) & 1,
        (*g_open_dma_mdec_out_chcr >> 24) & 1, *g_open_dma_mdec_in_madr, *g_open_dma_mdec_out_madr);
    printf(g_open_text_mdec_timeout_fifo_format, ~status >> 31, (status >> 30) & 1, (status >> 29) & 1,
        (status >> 28) & 1, (status >> 27) & 1, (status >> 25) & 1, (status >> 23) & 1);
    *g_open_mdec_status_reg = 0x80000000;
    *g_open_dma_mdec_in_chcr = 0;
    *g_open_dma_mdec_out_chcr = 0;
    result = 0;
    decoder_status = (u32)g_open_dma_mdec_out_chcr;
    decoder_control = g_open_mdec_status_reg;
    decoder_status = *(volatile u32*)decoder_status;
    decoder_status = 0x60000000;
    *decoder_control = decoder_status;
    return result;
}
