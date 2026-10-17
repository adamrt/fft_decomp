/* SCUS_942.21 0x8001b8b0..0x8001b937. */
#include "psx/libspu.h"

void SpuSetVoiceSL(s32 v_num, u16 sl) {
    /* Adding the index before the register base preserves the retail address operands. */
    u16 value;
    value = (v_num + _spu_RXX->rxx.voices)->adsr1;
    value &= 0xfff0;
    value |= sl;
    (v_num + _spu_RXX->rxx.voices)->adsr1 = value;
    PSYQ_SPU_REGISTER_DELAY();
}
