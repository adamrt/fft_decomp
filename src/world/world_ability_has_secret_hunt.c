#include "fft/data.h"
#include "fft/job.h"

s32 world_ability_has_secret_hunt(s16 unit_id) {
    return g_world_formation_unit_pointers[unit_id]->support_sets_3 & BATTLE_SUPPORT_SET_3_SECRET_HUNT;
}
