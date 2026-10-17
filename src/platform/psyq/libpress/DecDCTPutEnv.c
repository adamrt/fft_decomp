#include "psx/libpress.h"

/* Install the quantization tables and upload the cached IDCT table; env->dct is ignored. */
DECDCTENV* DecDCTPutEnv(DECDCTENV* env) {
    u32* src;
    u32* dst;
    s32 i;

    /* The cached tables are copied in aligned 32-bit units. */
    src = (u32*)env->iq_y;
    dst = (u32*)g_psyq_press_quant_table.iq_y;
    i = sizeof(g_psyq_press_quant_table.iq_y) / sizeof(u32) - 1;
    do {
        *dst++ = *src++;
    } while (--i != -1);

    dst = (u32*)g_psyq_press_quant_table.iq_c;
    src = (u32*)env->iq_c;
    i = sizeof(g_psyq_press_quant_table.iq_c) / sizeof(u32) - 1;
    do {
        *dst++ = *src++;
    } while (--i != -1);

    MDEC_in(&g_psyq_press_quant_table.command,
        (sizeof(g_psyq_press_quant_table.iq_y) + sizeof(g_psyq_press_quant_table.iq_c)) / sizeof(u32));
    MDEC_in(&g_psyq_press_scale_table.command, sizeof(g_psyq_press_scale_table.dct) / sizeof(u32));
    return env;
}
