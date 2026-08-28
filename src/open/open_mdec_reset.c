#include "fft/open.h"
#include "psx/types.h"

/* OPEN.BIN MDEC reset - drop the decoder back to a known state.  Mode 0 also
 * re-uploads the cached quantisation and IDCT tables. */
void open_mdec_reset(s32 mode) {
    switch (mode) {
    case 0:
        *g_open_mdec_status_reg = 0x80000000;
        *g_open_dma_mdec_in_chcr = 0;
        *g_open_dma_mdec_out_chcr = 0;
        *g_open_mdec_status_reg = 0x60000000;
        open_bin_mdec_in(g_open_mdec_quant_table_command, 0x20);
        open_bin_mdec_in(g_open_mdec_scale_table_command, 0x20);
        break;
    case 1:
        *g_open_mdec_status_reg = 0x80000000;
        *g_open_dma_mdec_in_chcr = 0;
        *g_open_dma_mdec_out_chcr = 0;
        *g_open_dma_mdec_out_chcr;
        *g_open_mdec_status_reg = 0x60000000;
        break;
    default:
        printf(g_open_text_mdec_reset_bad_option_format, mode);
        break;
    }
}
