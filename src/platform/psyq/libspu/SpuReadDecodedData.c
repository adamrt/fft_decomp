/* SCUS_942.21 0x8001ac7c..0x8001acef. */
#include "psx/libspu.h"

s32 SpuReadDecodedData(SpuDecodedData* d_data, s32 flag) {
    s16* destination = d_data->cd_left;
    u16 address;
    u32 blocks;
    u32 status;
    switch (flag) {
    case SPU_CDONLY:
        address = 0;
        blocks = 0x20;
        break;
    case SPU_VOICEONLY:
        destination = d_data->voice1;
        address = 0x100;
        blocks = 0x20;
        break;
    default:
        address = 0;
        blocks = 0x40;
        break;
    }
    _spu_Fr_(destination, address, blocks);
    status = _spu_RXX->rxx.status & 0x800;
    return status != 0;
}
