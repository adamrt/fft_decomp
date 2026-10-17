/* SCUS_942.21 0x80019b80..0x80019d87. */
#include "psx/libspu.h"

/* Set (SPU_ON) or clear (SPU_OFF) voice bits in a pair of 24-voice SPU mask registers, or in their
 * queued copy in _spu_RQ when event queueing is on; returns the resulting mask (any other on_off
 * only reads it). */
u32 _SpuSetAnyVoice(s32 on_off, u32 voice_bit, s32 reg_index_low, s32 reg_index_high) {
    u32 current;
    u8 high_voices;
    if (_spu_env & SPU_ENV_EVENT_QUEUEING) {
        high_voices = _spu_RQ[reg_index_high - PSYQ_SPU_REG_KEY_ON_LOW];
        current = (high_voices << 16) | _spu_RQ[reg_index_low - PSYQ_SPU_REG_KEY_ON_LOW];
    } else {
        high_voices = _spu_RXX->raw[reg_index_high];
        current = (high_voices << 16) | _spu_RXX->raw[reg_index_low];
    }
    switch (on_off) {
    case SPU_ON:
        if (_spu_env & SPU_ENV_EVENT_QUEUEING) {
            _spu_RQ[reg_index_low - PSYQ_SPU_REG_KEY_ON_LOW] |= voice_bit;
            _spu_RQ[reg_index_high - PSYQ_SPU_REG_KEY_ON_LOW] |= (voice_bit >> 16) & 0xff;
            _spu_RQmask |= 1 << ((reg_index_low - PSYQ_SPU_REG_KEY_OFF_LOW) >> 1);
        } else {
            _spu_RXX->raw[reg_index_low] |= voice_bit;
            _spu_RXX->raw[reg_index_high] |= (voice_bit >> 16) & 0xff;
        }
        current |= voice_bit & SPU_ALLCH;
        break;
    case SPU_OFF:
        if (_spu_env & SPU_ENV_EVENT_QUEUEING) {
            _spu_RQ[reg_index_low - PSYQ_SPU_REG_KEY_ON_LOW] &= ~voice_bit;
            _spu_RQ[reg_index_high - PSYQ_SPU_REG_KEY_ON_LOW] &= ~((voice_bit >> 16) & 0xff);
            _spu_RQmask |= 1 << ((reg_index_low - PSYQ_SPU_REG_KEY_OFF_LOW) >> 1);
        } else {
            _spu_RXX->raw[reg_index_low] &= ~voice_bit;
            _spu_RXX->raw[reg_index_high] &= ~((voice_bit >> 16) & 0xff);
        }
        current &= ~(voice_bit & SPU_ALLCH);
        break;
    }
    return current & SPU_ALLCH;
}
