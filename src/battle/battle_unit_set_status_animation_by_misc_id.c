#include "fft/battle.h"
#include "psx/types.h"

void battle_unit_set_status_animation_by_misc_id(u32 misc_id) {
    battle_unit_set_animation_based_on_status(battle_unit_get_misc_data_by_misc_id(misc_id & 0xffff));
}
