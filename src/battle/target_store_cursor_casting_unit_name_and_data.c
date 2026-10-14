#include "fft/battle.h"
#include "fft/battle_target.h"

/* Start the unit panels for a selected target tile: the attack preview for the
 * source unit against the unit on the cursor tile, or the source unit's own stats
 * when the tile is empty. */
void target_store_cursor_casting_unit_name_and_data(void) {
    battle_unit_t* target;
    battle_unit_t* source;

    target = battle_unit_find_selectable_at_tile(g_battle_cursor_x, g_battle_cursor_y, g_battle_cursor_z);
    source = battle_unit_get_source();
    if (target != 0) {
        menu_start_unit_view_thread(UNIT_VIEW_MODE_ATTACK_PREVIEW, source->stats->battle_id, target->stats->battle_id);
    } else {
        menu_start_unit_view_thread(UNIT_VIEW_MODE_HOVERED_UNIT, source->stats->battle_id, 0xFF);
    }
}
