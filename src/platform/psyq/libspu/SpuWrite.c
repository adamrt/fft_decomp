/* SCUS_942.21 0x8001af64..0x8001afc3. */
#include "psx/libspu.h"

u32 SpuWrite(u8* addr, u32 size) {
    if (size > 0x7eff0) {
        size = 0x7eff0;
    }
    _spu_Fw(addr, size);
    if (_spu_transferCallback == 0) {
        _spu_inTransfer = 0;
    }
    return size;
}
