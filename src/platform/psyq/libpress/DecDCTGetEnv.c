#include "psx/libpress.h"
#include "psx/types.h"

/* Copy the three cached MDEC tables out of the library's private storage
 * into the caller supplied environment block. */
DECDCTENV* DecDCTGetEnv(DECDCTENV* env) {
    u32* dst;
    u32* src;
    s32 i;

    /* The cached tables are copied in aligned 32-bit units. */
    dst = (u32*)env->iq_y;
    src = (u32*)g_psyq_press_quant_table.iq_y;
    i = sizeof(g_psyq_press_quant_table.iq_y) / sizeof(u32) - 1;
    do {
        *dst++ = *src++;
    } while (--i != -1);

    dst = (u32*)env->iq_c;
    src = (u32*)g_psyq_press_quant_table.iq_c;
    i = sizeof(g_psyq_press_quant_table.iq_c) / sizeof(u32) - 1;
    do {
        *dst++ = *src++;
    } while (--i != -1);

    dst = (u32*)env->dct;
    src = (u32*)g_psyq_press_scale_table.dct;
    i = sizeof(g_psyq_press_scale_table.dct) / sizeof(u32) - 1;
    do {
        *dst++ = *src++;
    } while (--i != -1);

    return env;
}
