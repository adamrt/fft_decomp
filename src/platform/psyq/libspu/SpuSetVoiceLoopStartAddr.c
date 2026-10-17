/* SCUS_942.21 0x8001b720..0x8001b79b. */
#include "psx/libspu.h"

void SpuSetVoiceLoopStartAddr(s32 v_num, u32 loop_start_addr) {
    _spu_FsetRXXa((v_num << 3) | 7, loop_start_addr);
    PSYQ_SPU_REGISTER_DELAY();
}
