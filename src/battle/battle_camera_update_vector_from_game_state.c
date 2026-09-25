#include "fft/battle.h"
#include "psx/types.h"

/* Accepted spelling (battle_camera_step_target_t*, per the definition); this
 * site passes the misc record that overlays it, so the view is a cast. */
typedef struct battle_camera_step_target battle_camera_step_target_t;
void battle_camera_step_real_coords_toward_target(battle_camera_step_target_t* target);

/* Per-frame camera vector update, dispatched on the game state.
 *
 * State 0x15 tracks the map cursor with a narrow acceptance window; the
 * acting-unit states (0x11, 0x13, 0x1C) track the source unit's item-get
 * camera position and step the real coordinates toward it; below 0x29 the
 * idle path is the same one battle_camera_update_cursor_tile_vector takes,
 * and 0x29 and above simply clears the vector.
 *
 * The scaling routine is declared void but leaves the normal routine's result
 * in $v0, which these callers consume.
 *
 * The idle path is battle_camera_update_cursor_tile_vector verbatim. The four
 * main_util_set_vector calls are one tail-merged block in the target, placed
 * last so it falls into the epilogue, so the idle path's window test is
 * spelled inverted with the clear-vector call as the function's last
 * statement. Written the natural way, the merged block lands mid-function
 * with an extra `j`. */
void battle_camera_update_vector_from_game_state(void) {
    battle_unit_misc_data_t* unit;
    s32 height;

    if (g_battle_game_state == BATTLE_GAME_STATE_CRYSTAL_LEARN) {
        unit = battle_unit_get_selectable_misc_data_at_map_coords(
            g_battle_cursor_x, g_battle_cursor_y, g_battle_cursor_z);
        height = 0;
        if (unit != 0) {
            height = (g_battle_gfx_spritesheet_data[unit->spritesheet_id].graphic_height >> 1) * g_battle_camera_zoom.vy
                / ONE;
        }
        if (g_battle_cursor_screen_x >= 0xF9 && g_battle_cursor_screen_x < 0x108
            && g_battle_cursor_screen_y - height >= 0xA1 && g_battle_cursor_screen_y < 0xB0) {
            main_util_set_vector(&g_battle_current_vector, 0, 0, 0);
            return;
        }
        /* The void callee leaves the normal routine's result in $v0 for the target. */
        battle_camera_step_focus_toward_cursor_tile(battle_camera_scale_cursor_tile_vector());
        return;
    }
    if (g_battle_game_state == BATTLE_GAME_STATE_UNIT_MOVING || g_battle_game_state == BATTLE_GAME_STATE_WAIT_DIRECTION
        || g_battle_game_state == BATTLE_GAME_STATE_PRE_ATTACK_ANIMATION) {
        unit = battle_unit_get_source_misc_data();
        height
            = (g_battle_gfx_spritesheet_data[unit->spritesheet_id].graphic_height >> 1) * g_battle_camera_zoom.vy / ONE;
        if ((u16)unit->item_get_camera_x >= 0xB1 && (u16)unit->item_get_camera_x < 0x150
            && unit->item_get_camera_y - height >= 0x51 && unit->item_get_camera_y < 0xA0) {
            main_util_set_vector(&g_battle_current_vector, 0, 0, 0);
            return;
        }
        if ((u16)unit->item_get_camera_x >= 0xA1 && (u16)unit->item_get_camera_x < 0x160
            && unit->item_get_camera_y - height >= 0x41 && unit->item_get_camera_y < 0xB0) {
            battle_camera_calculate_acting_unit_vector_normal();
        } else {
            battle_camera_shift_acting_unit_vector();
        }
        battle_camera_step_real_coords_toward_target((battle_camera_step_target_t*)unit);
        return;
    }
    if (g_battle_game_state >= 0x29) {
        main_util_set_vector(&g_battle_current_vector, 0, 0, 0);
        return;
    }
    if (g_battle_camera_zoom_action == 0 && g_battle_camera_tilt_action == 0 && g_battle_camera_rotation_action == 0
        && g_battle_state_game_flow_running == 0) {
        unit = battle_unit_get_selectable_misc_data_at_map_coords(
            g_battle_cursor_x, g_battle_cursor_y, g_battle_cursor_z);
        height = 0;
        if (unit != 0) {
            height = (g_battle_gfx_spritesheet_data[unit->spritesheet_id].graphic_height >> 1) * g_battle_camera_zoom.vy
                / ONE;
        }
        if (g_battle_cursor_screen_x < 0xB1 || g_battle_cursor_screen_x >= 0x150
            || g_battle_cursor_screen_y - height < 0x51 || g_battle_cursor_screen_y >= 0xA0) {
            if (g_battle_cursor_screen_x >= 0xA1 && g_battle_cursor_screen_x < 0x160
                && g_battle_cursor_screen_y - height >= 0x41 && g_battle_cursor_screen_y < 0xB0) {
                battle_camera_step_focus_toward_cursor_tile(battle_camera_calculate_cursor_tile_vector_normal());
            } else {
                /* The void callee leaves the normal routine's result in $v0 for the target. */
                battle_camera_step_focus_toward_cursor_tile(battle_camera_scale_cursor_tile_vector());
            }
            return;
        }
        main_util_set_vector(&g_battle_current_vector, 0, 0, 0);
    }
}
