#include "fft/battle.h"
#include "fft/battle_ai.h"

void battle_unit_update_attack_result_animation(battle_unit_misc_data_t* unit) {
    s32 result;

    result = battle_action_store_target_stats_pointer_data(unit->battle_data->misc_unit_id);
    unit->pending_attack_result = result;
    if (result == -1) {
        battle_unit_find_relocation_tile(unit->battle_data->misc_unit_id, &unit->dismount);
        battle_unit_set_map_coords_after_death_dismount(unit);
    }
    battle_gfx_prepare_post_action_display_by_misc_id(unit->unit_id);
    battle_unit_update_display_by_misc_id(unit->unit_id);
}
