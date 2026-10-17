/* SCUS_942.21 0x8001aef4..0x8001af63. */
#include "psx/libspu.h"

void SpuGetVoiceEnvelopeAttr(s32 v_num, s32* key_stat, s16* envx) {
    s16 envelope = _spu_RXX->raw[v_num << 3 | 6];
    *envx = envelope;
    /* Only the low 16 bits of the s32 key_stat output are written. */
    if (_spu_keystat & SPU_KEYCH(v_num)) {
        *(s16*)key_stat = envelope > 0 ? 1 : 3;
    } else {
        *(s16*)key_stat = envelope > 0 ? 2 : 0;
    }
}
