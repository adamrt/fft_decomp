/* SCUS_942.21 0x8001b428..0x8001b4af. */
#include "psx/libspu.h"

void SpuSetVoiceVolume(s32 v_num, s16 vol_l, s16 vol_r) {
    /* Adding the index before the register base preserves the retail address operands. */
    vol_l &= 0x7fff;
    vol_r &= 0x7fff;
    (v_num + _spu_RXX->rxx.voices)->volume_left = vol_l;
    (v_num + _spu_RXX->rxx.voices)->volume_right = vol_r;
    PSYQ_SPU_REGISTER_DELAY();
}
