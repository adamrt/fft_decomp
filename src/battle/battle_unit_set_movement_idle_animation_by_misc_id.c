#include "fft/battle.h"
#include "psx/types.h"

void battle_unit_set_movement_idle_animation_by_misc_id(u32 misc_id) {
    battle_unit_set_idle_animation_for_movement(battle_unit_get_misc_data_by_misc_id(misc_id & 0xffff));
}
