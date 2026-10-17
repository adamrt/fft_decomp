/* SCUS_942.21 0x8001b6a4..0x8001b71f. */
#include "psx/libspu.h"

void SpuSetVoiceStartAddr(s32 v_num, u32 start_addr) {
    _spu_FsetRXXa((v_num << 3) | 3, start_addr);
    PSYQ_SPU_REGISTER_DELAY();
}
