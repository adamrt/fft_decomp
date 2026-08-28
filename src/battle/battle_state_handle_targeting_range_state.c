#include "fft/battle.h"
#include "fft/main_gfx.h"
#include "fft/main_runtime.h"
#include "psx/pad.h"

void battle_state_handle_targeting_range_state(void) {
    u16 frame_data;
    battle_unit_misc_data_t* unit;
    battle_unit_misc_data_t* target;

    unit = battle_unit_get_source_misc_data();
    battle_state_handle_free_cursor_input();
    battle_menu_draw_selection_data(main_gfx_get_otag(), g_controller_input_raw);
    if ((unit->team_flags & BATTLE_TEAM_FLAG_PLAYER_CONTROLLED) != 0) {
        battle_target_move_cursor_by_input();
        battle_camera_handle_rotation_input();
        battle_camera_call_zoom_map();
        battle_camera_call_toggle_tilt();
        if ((g_controller_input_pressed & PSX_PAD_CROSS) != 0) {
            battle_target_set_tile_background_color(0, 2);
            battle_menu_open_active_unit_idle_action_menu();
            return;
        }
        if ((g_controller_input_pressed & PSX_PAD_CIRCLE) != 0) {
            if ((battle_map_get_tile_data_pointer(g_battle_cursor_x, g_battle_cursor_y, g_battle_cursor_z)
                        ->ceiling_depth_and_marks
                    & MAP_TILE_FLAG_ABILITY_RANGE)
                != 0) {
                battle_target_select_tile();
                return;
            }
            battle_state_enter_target_out_of_range();
        }
    } else {
        switch (unit->command_state.ai.data.action.targeting_type) {
        case 5:
            battle_target_update_free_cursor_selection(unit, 0);
            frame_data = unit->state_frame_counter++;
            if (frame_data >= 0x1f) {
                battle_map_get_tile_data_pointer(unit->command_state.cursor.target_panel.vx,
                    unit->command_state.cursor.target_panel.vz, unit->command_state.cursor.target_panel.vy);
                battle_target_select_tile();
            }
            return;
        case 6:
            target = battle_unit_get_misc_data_by_battle_id(unit->command_state.ai.data.action.target_id);
            if (target != 0) {
                battle_target_update_free_cursor_selection(unit, target);
                frame_data = unit->state_frame_counter++;
                if (frame_data >= 0x1f) {
                    battle_map_get_tile_data_pointer(target->map_x, target->map_y, target->map_z);
                    battle_target_select_tile();
                }
            } else {
                battle_menu_set_next_script_action_menus();
            }
            break;
        }
    }
}
