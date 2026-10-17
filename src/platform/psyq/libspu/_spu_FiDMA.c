/* SCUS_942.21 0x80018cb8..0x80018d87. */
#include "psx/libspu.h"

#include "psx/libapi.h"

/* SPU DMA completion handler (installed by SpuStart): after a write it waits a few register
 * cycles, clears the transfer mode, then calls _spu_transferCallback or, with none set, delivers
 * the HwSPU EvSpCOMP event. */
void _spu_FiDMA(void) {
    u32 wait;
    if (g_psyq_spu_dma_direction == 0) {
        _spu_Fw1ts();
        _spu_Fw1ts();
        _spu_Fw1ts();
    }
    _spu_RXX->rxx.control &= 0xffcf;
    wait = 0;
    while (_spu_RXX->rxx.control & 0x30) {
        if (++wait > 3840) {
            break;
        }
    }
    if (_spu_transferCallback != 0) {
        _spu_transferCallback();
    } else {
        DeliverEvent(HwSPU, EvSpCOMP);
    }
}
