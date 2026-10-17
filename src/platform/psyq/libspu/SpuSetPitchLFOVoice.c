/* SCUS_942.21 0x8001b070..0x8001b093. */
#include "psx/libspu.h"

u32 SpuSetPitchLFOVoice(s32 on_off, u32 voice_bit) {
    return _SpuSetAnyVoice(on_off, voice_bit, PSYQ_SPU_REG_PITCH_LFO_LOW, PSYQ_SPU_REG_PITCH_LFO_HIGH);
}
