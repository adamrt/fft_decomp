#include "fft/battle.h"
#include "psx/types.h"

void battle_unit_update_anim_display_for_all_targets(battle_unit_misc_data_t* unit) {
    s32 counter;
    battle_unit_misc_data_t* target;

    if (unit->target_count != 0) {
        for (counter = 0; counter < unit->target_count; counter++) {
            target = battle_unit_get_misc_data_by_battle_id(unit->target_list[counter]);
            if (target != 0) {
                battle_unit_update_display_by_misc_id(target->unit_id);
            }
        }
    }
}
