/* SCUS_942.21 0x800190d4..0x8001915b. */
#include "psx/libspu.h"

/* Write size bytes from source to SPU RAM at _spu_tsa, by DMA or, in I/O transfer mode, through
 * the FIFO; returns size without waiting for a DMA to finish. */
u32 _spu_Fw(void* source, u32 size) {
    if (_spu_transMode == SPU_TRANSFER_BY_DMA) {
        _spu_t(PSYQ_SPU_DMA_SET_ADDRESS, _spu_tsa << _spu_mem_mode_plus);
        _spu_t(PSYQ_SPU_DMA_WRITE);
        _spu_t(PSYQ_SPU_DMA_START_TRANSFER, source, size);
    } else {
        _spu_writeByIO(source, size);
    }
    return size;
}
