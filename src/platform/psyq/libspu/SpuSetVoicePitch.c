/* SCUS_942.21 0x8001b628..0x8001b6a3. */
#include "psx/libspu.h"

void SpuSetVoicePitch(s32 v_num, u16 pitch) {
    /* Adding the index before the register base preserves the retail address operands. */
    (v_num + _spu_RXX->rxx.voices)->pitch = pitch;
    PSYQ_SPU_REGISTER_DELAY();
}
