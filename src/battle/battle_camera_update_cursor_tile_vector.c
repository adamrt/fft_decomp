#include "fft/battle.h"
#include "psx/types.h"

/* Refresh the cursor-tile camera vector while the map camera is idle.
 *
 * The cursor unit's sprite height (scaled by g_battle_camera_zoom.vy, 4.12 fixed point)
 * offsets the cursor's screen position (g_battle_cursor_screen_x, g_battle_cursor_screen_y). Inside the
 * central window the vector is cleared; nearer the edge the unit tile normal
 * is used as is, otherwise doubled. The tile height left in $v0 by either
 * vector routine is passed on to battle_camera_step_focus_toward_cursor_tile. */
void battle_camera_update_cursor_tile_vector(void) {
    battle_unit_misc_data_t* unit;
    s32 height;

    if (g_battle_camera_zoom_action == 0 && g_battle_camera_tilt_action == 0 && g_battle_camera_rotation_action == 0
        && g_battle_state_game_flow_running == 0) {
        unit = battle_unit_get_selectable_misc_data_at_map_coords(
            g_battle_cursor_x, g_battle_cursor_y, g_battle_cursor_z);
        height = 0;
        if (unit != 0) {
            height = (g_battle_gfx_spritesheet_data[unit->spritesheet_id].graphic_height >> 1) * g_battle_camera_zoom.vy
                / ONE;
        }
        if (g_battle_cursor_screen_x >= 0xB1 && g_battle_cursor_screen_x < 0x150
            && g_battle_cursor_screen_y - height >= 0x51 && g_battle_cursor_screen_y < 0xA0) {
            main_util_set_vector(&g_battle_current_vector, 0, 0, 0);
            return;
        }
        if (g_battle_cursor_screen_x >= 0xA1 && g_battle_cursor_screen_x < 0x160
            && g_battle_cursor_screen_y - height >= 0x41 && g_battle_cursor_screen_y < 0xB0) {
            battle_camera_step_focus_toward_cursor_tile(battle_camera_calculate_cursor_tile_vector_normal());
        } else {
            /* The scaling routine is declared void but leaves the normal
             * routine's result in $v0, which this caller consumes. */
            battle_camera_step_focus_toward_cursor_tile(battle_camera_scale_cursor_tile_vector());
        }
    }
}
