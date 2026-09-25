#include "fft/world.h"

s32 world_ability_has_two_hands(s16 unit_id) {
    return g_world_formation_unit_pointers[unit_id]->support_sets_3 & BATTLE_SUPPORT_SET_3_TWO_HANDS;
}
