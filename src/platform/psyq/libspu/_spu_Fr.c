/* SCUS_942.21 0x8001915c..0x800191c3. */
#include "psx/libspu.h"

/* Start a DMA read of size bytes from SPU RAM at _spu_tsa into addr; returns size without waiting. */
u32 _spu_Fr(void* addr, u32 size) {
    _spu_t(PSYQ_SPU_DMA_SET_ADDRESS, _spu_tsa << _spu_mem_mode_plus);
    _spu_t(PSYQ_SPU_DMA_READ);
    _spu_t(PSYQ_SPU_DMA_START_TRANSFER, addr, size);
    return size;
}
