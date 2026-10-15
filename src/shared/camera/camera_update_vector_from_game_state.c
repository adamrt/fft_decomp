#include "fft/battle.h"
#include "fft/battle_action.h"
#include "fft/battle_camera.h"
#include "fft/battle_gfx.h"
#include "psx/types.h"

/* Per-frame camera vector update, dispatched on the game state.
 *
 * The crystal-learn state tracks the map cursor with a narrow acceptance
 * window; the acting-unit states (unit moving, wait direction and pre-attack
 * animation) track the source unit's camera-relative position and step the
 * real coordinates toward the unit; the other states below
 * BATTLE_GAME_STATE_ACTION_ANNOUNCEMENT take the idle path of
 * camera_update_cursor_tile_vector, and that state and those above it
 * clear the vector.
 *
 * The scaling routine passes the normal routine's result through, and these
 * callers hand it on.
 *
 * The idle path is camera_update_cursor_tile_vector verbatim. The four
 * main_util_set_vector calls are one tail-merged block in the target, placed
 * last so it falls into the epilogue, so the idle path's window test is
 * spelled inverted with the clear-vector call as the function's last
 * statement. Written the natural way, the merged block lands mid-function
 * with an extra `j`. */
void camera_update_vector_from_game_state(void) {
    battle_unit_t* unit;
    s32 height;

    if (g_battle_game_state == BATTLE_GAME_STATE_CRYSTAL_LEARN) {
        unit = battle_unit_find_selectable_at_tile(g_battle_cursor_x, g_battle_cursor_y, g_battle_cursor_z);
        height = 0;
        if (unit != 0) {
            height = (g_battle_gfx_spritesheet_data[unit->spritesheet_id].graphic_height >> 1) * g_camera_zoom.vy / ONE;
        }
        if (g_battle_cursor_screen_x >= 0xF9 && g_battle_cursor_screen_x < 0x108
            && g_battle_cursor_screen_y - height >= 0xA1 && g_battle_cursor_screen_y < 0xB0) {
            main_util_set_vector(&g_camera_step_vector, 0, 0, 0);
            return;
        }
        /* The scaling routine passes through the normal routine's height bound. */
        camera_step_focus_toward_cursor_tile(camera_scale_cursor_tile_vector());
        return;
    }
    if (g_battle_game_state == BATTLE_GAME_STATE_UNIT_MOVING || g_battle_game_state == BATTLE_GAME_STATE_WAIT_DIRECTION
        || g_battle_game_state == BATTLE_GAME_STATE_PRE_ATTACK_ANIMATION) {
        unit = battle_unit_get_source();
        height = (g_battle_gfx_spritesheet_data[unit->spritesheet_id].graphic_height >> 1) * g_camera_zoom.vy / ONE;
        if (unit->camera_relative_x >= 0xB1 && unit->camera_relative_x < 0x150
            && unit->camera_relative_y - height >= 0x51 && unit->camera_relative_y < 0xA0) {
            main_util_set_vector(&g_camera_step_vector, 0, 0, 0);
            return;
        }
        if (unit->camera_relative_x >= 0xA1 && unit->camera_relative_x < 0x160
            && unit->camera_relative_y - height >= 0x41 && unit->camera_relative_y < 0xB0) {
            camera_calculate_acting_unit_vector_normal();
        } else {
            camera_shift_acting_unit_vector();
        }
        camera_step_real_coords_toward_target(unit);
        return;
    }
    if (g_battle_game_state >= BATTLE_GAME_STATE_ACTION_ANNOUNCEMENT) {
        main_util_set_vector(&g_camera_step_vector, 0, 0, 0);
        return;
    }
    if (g_camera_zoom_action == CAMERA_STEP_NONE && g_camera_tilt_action == CAMERA_STEP_NONE
        && g_camera_rotation_action == CAMERA_ROTATION_NONE && g_camera_map_pan_enabled == 0) {
        unit = battle_unit_find_selectable_at_tile(g_battle_cursor_x, g_battle_cursor_y, g_battle_cursor_z);
        height = 0;
        if (unit != 0) {
            height = (g_battle_gfx_spritesheet_data[unit->spritesheet_id].graphic_height >> 1) * g_camera_zoom.vy / ONE;
        }
        if (g_battle_cursor_screen_x < 0xB1 || g_battle_cursor_screen_x >= 0x150
            || g_battle_cursor_screen_y - height < 0x51 || g_battle_cursor_screen_y >= 0xA0) {
            if (g_battle_cursor_screen_x >= 0xA1 && g_battle_cursor_screen_x < 0x160
                && g_battle_cursor_screen_y - height >= 0x41 && g_battle_cursor_screen_y < 0xB0) {
                camera_step_focus_toward_cursor_tile(camera_calculate_cursor_tile_vector_normal());
            } else {
                /* The scaling routine passes the normal routine's result through. */
                camera_step_focus_toward_cursor_tile(camera_scale_cursor_tile_vector());
            }
            return;
        }
        main_util_set_vector(&g_camera_step_vector, 0, 0, 0);
    }
}
