#include "fft/battle.h"
#include "fft/battle_target.h"
#include "psx/pad.h"

/* TargetingRange game-state handler: the ability's range is shown and the target
 * tile is chosen.
 *
 * A player-controlled unit moves the cursor and the camera. Cross clears the
 * marks and returns to the idle action menu; circle selects the cursor tile when
 * it carries the ability-range mark and opens the out-of-range prompt when not.
 * An AI-controlled unit puts the cursor on its target panel (targeting type 5) or
 * its target unit (type 6) and selects once its state frame counter reaches 0x1f;
 * a target unit that does not exist reopens the action menus.
 *
 * The two tile pointers fetched on the AI path are not used. */
void battle_state_handle_targeting_range_state(void) {
    u16 frame_data;
    battle_unit_t* unit;
    battle_unit_t* target;

    unit = battle_unit_get_source();
    battle_state_handle_free_cursor_input();
    menu_update_menus_and_event_speed(main_gfx_get_otag(), g_input_current_buttons);
    if ((unit->team_flags & BATTLE_TEAM_FLAG_PLAYER_CONTROLLED) != 0) {
        target_move_cursor_by_input();
        camera_handle_rotation_input();
        camera_update_zoom_input();
        camera_update_tilt_input();
        if ((g_main_input_pressed_buttons & PSX_PAD_CROSS) != 0) {
            target_set_tile_background_color(BATTLE_TILE_MARK_NONE, 2);
            battle_menu_open_active_unit_idle_action_menu();
            return;
        }
        if ((g_main_input_pressed_buttons & PSX_PAD_CIRCLE) != 0) {
            if ((map_get_tile_data_pointer(g_battle_cursor_x, g_battle_cursor_y, g_battle_cursor_z)
                        ->ceiling_depth_and_marks.ability_range)
                != 0) {
                target_select_tile();
                return;
            }
            battle_state_enter_target_out_of_range();
        }
    } else {
        switch (unit->command_state.ai.data.action.targeting_type) {
        case BATTLE_TARGETING_TYPE_TILE:
            target_update_free_cursor_selection(unit, 0);
            frame_data = unit->state_frame_counter++;
            if (frame_data >= 0x1f) {
                map_get_tile_data_pointer(unit->command_state.cursor.target_panel.vx,
                    unit->command_state.cursor.target_panel.vz, unit->command_state.cursor.target_panel.vy);
                target_select_tile();
            }
            return;
        case BATTLE_TARGETING_TYPE_UNIT:
            target = battle_unit_get_by_battle_id(unit->command_state.ai.data.action.target_id);
            if (target != 0) {
                target_update_free_cursor_selection(unit, target);
                frame_data = unit->state_frame_counter++;
                if (frame_data >= 0x1f) {
                    map_get_tile_data_pointer(target->map_x, target->map_y, target->map_z);
                    target_select_tile();
                }
            } else {
                battle_menu_set_next_script_action_menus();
            }
            break;
        }
    }
}
