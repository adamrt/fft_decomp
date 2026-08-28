#include "fft/battle.h"

s32 battle_effect_lerp_linear(s32 start, s32 end, s32 factor) {
    return ((end - start) * factor >> 8) + start;
}
