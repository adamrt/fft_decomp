#include "fft/battle.h"
#include "fft/battle_camera.h"

/* Enter the target-select-denied state for the cursor tile: show the summary of a
 * unit standing there and open the refusal variant of the target-confirm prompt
 * for the casting unit.
 *
 * Option 3 is used for an empty tile, 1 for a targeted auto-battle selection and
 * 2 for a recovery selection. Any other selection command over a unit opens no
 * prompt. */
void battle_state_enter_target_select_denied(void) {
    battle_unit_t* runtime;
    battle_unit_t* unit;
    unit_stats_t* stats;
    s32 kind;

    camera_disable_map_pan();
    g_battle_game_state = BATTLE_GAME_STATE_TARGET_SELECT_DENIED;
    runtime = battle_unit_find_selectable_at_tile(g_battle_cursor_x, g_battle_cursor_y, g_battle_cursor_z);
    if (runtime != 0) {
        stats = runtime->stats;
        if (stats != 0) {
            menu_start_unit_view_thread(UNIT_VIEW_MODE_UNIT_SUMMARY, stats->battle_id, 0);
        }
    }
    unit = battle_unit_get_casting();
    if (runtime == 0) {
        battle_menu_init_system_function(BATTLE_MENU_SYSTEM_COMMAND_TARGET_CONFIRM, 3, unit->stats->battle_id, 0, 1);
        return;
    }
    kind = unit->target_select_command;
    if (kind == AI_COMMAND_AUTOBATTLE_TARGETED_SELECT) {
        battle_menu_init_system_function(BATTLE_MENU_SYSTEM_COMMAND_TARGET_CONFIRM, 1, unit->stats->battle_id, 0, 1);
        return;
    }
    if (kind == AI_COMMAND_AUTOBATTLE_RECOVERY_SELECT) {
        battle_menu_init_system_function(BATTLE_MENU_SYSTEM_COMMAND_TARGET_CONFIRM, 2, unit->stats->battle_id, 0, 1);
    }
}
