/* SCUS_942.21 0x80019b5c..0x80019b7f. */
#include "psx/libspu.h"

u32 SpuSetNoiseVoice(s32 on_off, u32 voice_bit) {
    return _SpuSetAnyVoice(on_off, voice_bit, PSYQ_SPU_REG_NOISE_LOW, PSYQ_SPU_REG_NOISE_HIGH);
}
