#include "fft/battle.h"
#include "psx/types.h"

void battle_map_start_darkness_blend(s32 first, s32 second, s16 third, s16 fourth, s16 fifth) {
    battle_map_blend_darkness_color(first, second, third, fourth, fifth);
}
