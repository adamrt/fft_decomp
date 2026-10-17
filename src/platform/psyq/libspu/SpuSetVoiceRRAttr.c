/* SCUS_942.21 0x8001bab8..0x8001bb5b. */
#include "psx/libspu.h"

void SpuSetVoiceRRAttr(s32 v_num, u16 rr, s32 rr_mode) {
    /* Adding the index before the register base preserves the retail address operands. */
    u32 mode = 0;
    u16 value;
    if (rr_mode != SPU_VOICE_LINEARDecN) {
        mode = rr_mode == SPU_VOICE_EXPDec ? 0x20 : 0;
    }
    value = (v_num + _spu_RXX->rxx.voices)->adsr2;
    value &= 0xffc0;
    value |= rr | mode;
    (v_num + _spu_RXX->rxx.voices)->adsr2 = value;
    PSYQ_SPU_REGISTER_DELAY();
}
