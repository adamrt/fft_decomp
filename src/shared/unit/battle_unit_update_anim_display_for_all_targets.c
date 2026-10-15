#include "fft/battle_unit.h"
#include "psx/types.h"

void battle_unit_update_anim_display_for_all_targets(battle_unit_t* unit) {
    s32 counter;
    battle_unit_t* target;

    if (unit->strike.target_count != 0) {
        for (counter = 0; counter < unit->strike.target_count; counter++) {
            target = battle_unit_get_by_battle_id(unit->strike.target_list[counter]);
            if (target != 0) {
                battle_unit_update_display_by_runtime_id(target->runtime_id);
            }
        }
    }
}
