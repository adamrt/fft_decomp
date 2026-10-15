#include "fft/main.h"
#include "fft/world.h"
#include "psx/types.h"

s32 world_ability_has_two_swords(s16 unit_id) {
    return g_world_unit_pointers[unit_id]->support_abilities[2] & BATTLE_SUPPORT_SET_3_TWO_SWORDS;
}
