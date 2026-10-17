/* SCUS_942.21 0x80019378..0x800193a3. */
#include "psx/libspu.h"

/* Program the SPU bus delay register for DMA reads from SPU RAM: DMA timing field 2 and bit 29.
 * _spu_t and _spu_Fr_ call it before reads, not writes. */
void _spu_FsetDelayW(void) {
    *g_psyq_spu_delay_register = (*g_psyq_spu_delay_register & 0xf0ffffff) | 0x22000000;
}
