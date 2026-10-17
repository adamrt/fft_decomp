/* SCUS_942.21 0x80019dd8..0x80019e37. */
#include "psx/libspu.h"

u32 SpuRead(u8* addr, u32 size) {
    if (size > 0x7eff0) {
        size = 0x7eff0;
    }
    _spu_Fr(addr, size);
    if (_spu_transferCallback == 0) {
        _spu_inTransfer = 0;
    }
    return size;
}
