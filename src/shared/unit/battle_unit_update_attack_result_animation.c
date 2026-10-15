#include "fft/battle_action.h"
#include "fft/battle_gfx.h"
#include "fft/battle_unit.h"

/* Applies the prepared action result to a unit and shows it.
 *
 * The result code is kept in pending_attack_result; -1 (a rider killed on its
 * mount) finds a relocation tile and moves the unit there. The post-action
 * display and the display update follow. */
void battle_unit_update_attack_result_animation(battle_unit_t* unit) {
    unit->pending_attack_result = action_store_target_stats_pointer_data(unit->stats->battle_id);
    if (unit->pending_attack_result == -1) {
        battle_unit_find_relocation_tile(unit->stats->battle_id, &unit->dismount);
        battle_unit_set_map_coords_after_death_dismount(unit);
    }
    battle_gfx_prepare_post_action_display_by_runtime_id(unit->runtime_id);
    battle_unit_update_display_by_runtime_id(unit->runtime_id);
}
