/* SCUS_942.21 0x8001b79c..0x8001b827. */
#include "psx/libspu.h"

void SpuSetVoiceDR(s32 v_num, u16 dr) {
    /* Adding the index before the register base preserves the retail address operands. */
    (v_num + _spu_RXX->rxx.voices)->adsr1 = ((v_num + _spu_RXX->rxx.voices)->adsr1 & 0xff0f) | (dr << 4);
    PSYQ_SPU_REGISTER_DELAY();
}
