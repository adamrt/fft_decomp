#include "fft/open.h"

/* OPEN.BIN DecDCTPutEnv - install the caller supplied quantisation and IDCT
 * tables into the library's cache and push both of them to the MDEC. */
u32* open_bin_decdctputenv(u32* env) {
    u32* src;
    u32* dst;
    s32 i;

    src = env;
    dst = g_open_mdec_luma_quant_table;
    i = 15;
    do {
        *dst++ = *src++;
    } while (--i != -1);

    dst = g_open_mdec_chroma_quant_table;
    src = env + 16;
    i = 15;
    do {
        *dst++ = *src++;
    } while (--i != -1);

    open_bin_mdec_in(g_open_mdec_quant_table_command, 0x20);
    open_bin_mdec_in(g_open_mdec_scale_table_command, 0x20);
    return env;
}
