/* SCUS_942.21 0x8001b938..0x8001b9d3. */
#include "psx/libspu.h"

void SpuSetVoiceARAttr(s32 v_num, u16 ar, s32 ar_mode) {
    /* Adding the index before the register base preserves the retail address operands. */
    u32 mode = ar_mode == SPU_VOICE_EXPIncN ? 0x80 : 0;
    u16 value;
    value = (v_num + _spu_RXX->rxx.voices)->adsr1;
    value &= 0xff;
    value |= (ar | mode) << 8;
    (v_num + _spu_RXX->rxx.voices)->adsr1 = value;
    PSYQ_SPU_REGISTER_DELAY();
}
