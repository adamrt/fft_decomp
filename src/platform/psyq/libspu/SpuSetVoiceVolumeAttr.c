/* SCUS_942.21 0x8001b4b0..0x8001b627. */
#include "psx/libspu.h"

void SpuSetVoiceVolumeAttr(s32 v_num, s16 vol_l, s16 vol_r, s16 vol_mode_l, s16 vol_mode_r) {
    u16 left_mode;
    u16 right_mode;
    u16 right_volume;
    s32 register_index = v_num << 3;
    vol_l &= 0x7fff;
    left_mode = 0;
    switch (vol_mode_l) {
    case SPU_VOICE_LINEARIncN:
        left_mode = 0x8000;
        break;
    case SPU_VOICE_LINEARIncR:
        left_mode = 0x9000;
        break;
    case SPU_VOICE_LINEARDecN:
        left_mode = 0xa000;
        break;
    case SPU_VOICE_LINEARDecR:
        left_mode = 0xb000;
        break;
    case SPU_VOICE_EXPIncN:
        left_mode = 0xc000;
        break;
    case SPU_VOICE_EXPIncR:
        left_mode = 0xd000;
        break;
    case SPU_VOICE_EXPDec:
        left_mode = 0xe000;
        break;
    }
    right_volume = vol_r & 0x7fff;
    right_mode = 0;
    _spu_RXX->raw[register_index] = vol_l | left_mode;
    switch (vol_mode_r) {
    case SPU_VOICE_LINEARIncN:
        right_mode = 0x8000;
        break;
    case SPU_VOICE_LINEARIncR:
        right_mode = 0x9000;
        break;
    case SPU_VOICE_LINEARDecN:
        right_mode = 0xa000;
        break;
    case SPU_VOICE_LINEARDecR:
        right_mode = 0xb000;
        break;
    case SPU_VOICE_EXPIncN:
        right_mode = 0xc000;
        break;
    case SPU_VOICE_EXPIncR:
        right_mode = 0xd000;
        break;
    case SPU_VOICE_EXPDec:
        right_mode = 0xe000;
        break;
    }
    _spu_RXX->raw[register_index + 1] = right_volume | right_mode;
    PSYQ_SPU_REGISTER_DELAY();
}
