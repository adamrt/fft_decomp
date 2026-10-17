/* SCUS_942.21 0x8001b9d4..0x8001bab7. */
#include "psx/libspu.h"

void SpuSetVoiceSRAttr(s32 v_num, u16 sr, s32 sr_mode) {
    s32 index = v_num << 3;
    u32 mode = 0x100;
    u16 value;
    switch (sr_mode) {
    case SPU_VOICE_LINEARIncN:
        mode = 0;
        break;
    case SPU_VOICE_EXPIncN:
        mode = 0x200;
        break;
    case SPU_VOICE_EXPDec:
        mode = 0x300;
        break;
    }
    value = _spu_RXX->raw[index + 5];
    value &= 0x3f;
    value |= (sr | mode) << 6;
    _spu_RXX->raw[index + 5] = value;
    PSYQ_SPU_REGISTER_DELAY();
}
