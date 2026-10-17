/* SCUS_942.21 0x8001afc4..0x8001b017. */
#include "psx/libspu.h"

u32 SpuSetTransferStartAddr(u32 addr) {
    if (addr < 0x1010 || addr > 0x7fff8) {
        return 0;
    }
    _spu_tsa = _spu_FsetRXXa(PSYQ_SPU_ADDRESS_AS_UNITS, addr);
    return _spu_tsa;
}
