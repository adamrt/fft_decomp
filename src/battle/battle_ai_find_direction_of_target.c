#include "fft/battle.h"

battle_ai_direction_e battle_ai_find_direction_of_target(const s8* target, const s8* origin) {
    s32 z1 = target[2];
    s32 x1 = target[0];
    s32 x2 = origin[0];
    s32 z2 = origin[2];
    s32 diff_z_minus_x = ((z1 - x1) + x2) - z2;
    s32 diff_z_plus_x = ((z1 + x1) - x2) - z2;
    s32 unused;

    if (diff_z_minus_x <= 0) {
        if (diff_z_plus_x < 0)
            return BATTLE_AI_DIRECTION_SOUTH;
        unused = 0;
    } else {
        if (diff_z_plus_x <= 0)
            return BATTLE_AI_DIRECTION_WEST;
        unused = 1;
    }
    if (diff_z_minus_x >= 0 && diff_z_plus_x > 0)
        return BATTLE_AI_DIRECTION_NORTH;
    if (diff_z_minus_x < 0 && diff_z_plus_x >= 0)
        return BATTLE_AI_DIRECTION_EAST;
    return BATTLE_AI_DIRECTION_OVERLAP;
}
