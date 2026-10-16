/* SCUS_942.21 0x800186e4..0x800187db. */
#include "psx/libspu.h"

#include "psx/libetc.h"

/* Shared body of SpuInit (hot 0) and SpuInitHot (hot 1): reset the callbacks and the SPU, set
 * every voice's center note on a cold start, install the DMA handler, turn reverb off and clear
 * the allocator and key-queue state. */
void _SpuInit(s32 hot) {
    s32 i;
    u16 sample_note;
    ResetCallback();
    _spu_init(hot);
    sample_note = 0xc000;
    if (!hot) {
        for (i = 23; i >= 0; i--) {
            _spu_voice_centerNote[i] = sample_note;
        }
    }
    SpuStart();
    _spu_rev_flag = SPU_OFF;
    _spu_rev_reserve_wa = SPU_OFF;
    _spu_rev_attr.mode = SPU_REV_MODE_OFF;
    _spu_rev_attr.depth.left = 0;
    _spu_rev_attr.depth.right = 0;
    _spu_rev_attr.delay = 0;
    _spu_rev_attr.feedback = 0;
    _spu_rev_offsetaddr = _spu_rev_startaddr[SPU_REV_MODE_OFF];
    _spu_FsetRXX(PSYQ_SPU_REG_REVERB_START, _spu_rev_offsetaddr, PSYQ_SPU_REGISTER_RAW);
    _spu_AllocBlockNum = 0;
    _spu_AllocLastNum = 0;
    _spu_memList = 0;
    _spu_trans_mode = SPU_TRANSFER_BY_DMA;
    _spu_transMode = SPU_TRANSFER_BY_DMA;
    _spu_keystat = 0;
    _spu_RQmask = 0;
    _spu_RQvoice = 0;
    _spu_env = 0;
}
