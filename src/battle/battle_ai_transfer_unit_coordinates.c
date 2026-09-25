#include "fft/battle.h"

void battle_ai_transfer_unit_coordinates(s32 unit_idx, battle_ai_coords_t* dst) {
    dst->bytes.x = g_battle_unit_stats[unit_idx].x;
    dst->bytes.y = (u8)g_battle_unit_stats[unit_idx].position.raw;
    dst->bytes.elevation = (u8)(g_battle_unit_stats[unit_idx].position.raw >> 15);
    dst->bytes.zero = 0;
}
