/* SCUS_942.21 0x8001aae0..0x8001ac7b. */
#include "psx/libspu.h"

#include "psx/libapi.h"

s32 SpuClearReverbWorkArea(s32 mode) {
    /* Volatile preserves the native stack slot and separate callback test/restore reloads. */
    volatile SpuTransferCallbackProc callback = 0;
    u32 remaining;
    u32 address;
    s32 more;
    s32 count;
    s32 saved_mode;
    s32 changed_mode = 0;
    if ((u32)mode >= PSYQ_SPU_REVERB_MODE_COUNT || _SpuIsInAllocateArea_(_spu_rev_startaddr[mode])) {
        return -1;
    }
    if (mode == SPU_REV_MODE_OFF) {
        remaining = 0x10 << _spu_mem_mode_plus;
        address = 0xfff0 << _spu_mem_mode_plus;
    } else {
        remaining = (0x10000 - _spu_rev_startaddr[mode]) << _spu_mem_mode_plus;
        address = _spu_rev_startaddr[mode] << _spu_mem_mode_plus;
    }
    saved_mode = _spu_transMode;
    if (saved_mode == SPU_TRANSFER_BY_IO) {
        _spu_transMode = SPU_TRANSFER_BY_DMA;
        changed_mode = 1;
    }
    more = 1;
    if (_spu_transferCallback != 0) {
        callback = _spu_transferCallback;
        _spu_transferCallback = 0;
    }
    while (more) {
        if (remaining > sizeof(_spu_zerobuf)) {
            count = sizeof(_spu_zerobuf);
        } else {
            count = remaining;
            more = 0;
        }
        _spu_t(PSYQ_SPU_DMA_SET_ADDRESS, address);
        _spu_t(PSYQ_SPU_DMA_WRITE);
        _spu_t(PSYQ_SPU_DMA_START_TRANSFER, _spu_zerobuf, count);
        WaitEvent(_spu_EVdma);
        remaining -= sizeof(_spu_zerobuf);
        address += sizeof(_spu_zerobuf);
    }
    if (changed_mode) {
        _spu_transMode = saved_mode;
    }
    if (callback != 0) {
        _spu_transferCallback = callback;
    }
    return 0;
}
