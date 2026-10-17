/* SCUS_942.21 0x8001acf0..0x8001aef3. */
#include "psx/libspu.h"

void SpuSetKey(s32 on_off, u32 voice_bit) {
    u16 low;
    u16 high;
    voice_bit &= SPU_ALLCH;
    low = voice_bit;
    high = voice_bit >> 16;
    switch (on_off) {
    case SPU_ON:
        if (_spu_env & SPU_ENV_EVENT_QUEUEING) {
            _spu_RQ[0] = low;
            _spu_RQ[1] = high;
            _spu_RQmask |= SPU_EVENT_KEY;
            _spu_RQvoice |= voice_bit;
            if (_spu_RQ[2] & voice_bit) {
                _spu_RQ[2] &= ~voice_bit;
            }
            if (_spu_RQ[3] & high) {
                _spu_RQ[3] &= ~high;
            }
        } else {
            _spu_RXX->rxx.key_on_low = low;
            _spu_RXX->rxx.key_on_high = high;
            _spu_keystat |= voice_bit;
        }
        break;
    case SPU_OFF:
        if (_spu_env & SPU_ENV_EVENT_QUEUEING) {
            _spu_RQ[2] = low;
            _spu_RQ[3] = high;
            _spu_RQmask |= SPU_EVENT_KEY;
            _spu_RQvoice &= ~voice_bit;
            if (_spu_RQ[0] & voice_bit) {
                _spu_RQ[0] &= ~voice_bit;
            }
            if (_spu_RQ[1] & high) {
                _spu_RQ[1] &= ~high;
            }
        } else {
            _spu_RXX->rxx.key_off_low = low;
            _spu_RXX->rxx.key_off_high = high;
            _spu_keystat &= ~voice_bit;
        }
        break;
    }
}
