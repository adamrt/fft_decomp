/* SCUS_942.21 0x80020650..0x8002074f. */
#include "psx/libcd.h"

/* Copy words 32-bit words of the current sector from the CD data buffer by DMA, blocking until the
 * transfer ends; always returns 0. */
s32 CD_getsector(void* destination, s32 words) {
    *g_psyq_cd_index_status_register = 0;
    *g_psyq_cd_request_interrupt_register = 0x80;
    *g_psyq_cd_access_delay_register = 0x20943;
    *g_psyq_cd_common_delay_register = 0x1323;
    *g_psyq_cd_dma_priority_register |= 0x8000;
    *g_psyq_cd_dma_address_register = (u32)destination;
    *g_psyq_cd_dma_block_register = words | 0x10000;
    while (!(*g_psyq_cd_index_status_register & 0x40)) { }
    *g_psyq_cd_dma_control_register = 0x11000000;
    while (*g_psyq_cd_dma_control_register & 0x01000000) { }
    *g_psyq_cd_common_delay_register = 0x1325;
    return 0;
}
