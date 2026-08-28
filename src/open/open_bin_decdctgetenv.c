#include "fft/open.h"
#include "psx/types.h"

/* OPEN.BIN DecDCTGetEnv - copy the three cached MDEC tables out of the
 * library's private storage into the caller supplied environment block. */
u32* open_bin_decdctgetenv(u32* env) {
    u32* dst;
    u32* src;
    s32 i;

    dst = env;
    src = g_open_mdec_luma_quant_table;
    i = 15;
    do {
        *dst++ = *src++;
    } while (--i != -1);

    dst = env + 16;
    src = g_open_mdec_chroma_quant_table;
    i = 15;
    do {
        *dst++ = *src++;
    } while (--i != -1);

    dst = env + 32;
    src = g_open_mdec_scale_table;
    i = 31;
    do {
        *dst++ = *src++;
    } while (--i != -1);

    return env;
}
