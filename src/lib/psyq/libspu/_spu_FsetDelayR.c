/* SCUS_942.21 0x8001934c..0x80019377. */
#include "psx/libspu.h"

/* Program the SPU bus delay register for DMA writes into SPU RAM: clear the DMA timing field (bits
 * 24-27) and set bit 29. _spu_t calls it before writes, not reads. */
void _spu_FsetDelayR(void) {
    *g_psyq_spu_delay_register = (*g_psyq_spu_delay_register & 0xf0ffffff) | 0x20000000;
}
