#include "fft/battle.h"
#include "fft/main_gfx.h"
#include "fft/main_runtime.h"
#include "fft/main_sound.h"
#include "psx/pad.h"

/* Free-cursor game state: camera and cursor input, then the button actions
 * (help, team highlight, act menu, move range, AT list and mini menu). */
void battle_state_handle_default_state(void) {
    battle_unit_misc_data_t* unit;
    s32 input;
    s32 unused[2]; /* dead 8-byte local; keeps the target's 0x20 frame */

    battle_camera_handle_rotation_input();
    battle_camera_call_zoom_map();
    battle_camera_call_toggle_tilt();
    battle_target_move_cursor_by_input();
    battle_state_handle_free_cursor_input();
    battle_menu_draw_selection_data(main_gfx_get_otag(), g_controller_input_raw);
    input = g_controller_input_pressed;
    (void)&unused;
    if (input & PSX_PAD_SELECT) {
        battle_action_clear_at_list_id();
        battle_menu_open_free_cursor_help();
    } else if (input & PSX_PAD_SQUARE) {
        battle_action_clear_at_list_id();
        battle_state_enter_highlight_units_by_team();
    } else if (input & PSX_PAD_CIRCLE) {
        battle_target_set_tile_background_color(0, 0);
        main_sound_play_sfx(MAIN_SFX_CONFIRM);
        if (battle_unit_get_selectable_misc_data_at_map_coords(g_battle_cursor_x, g_battle_cursor_y, g_battle_cursor_z)
            != 0) {
            battle_menu_dispatch_idle_action_menu();
        } else {
            unit = battle_unit_get_source_misc_data();
            if (unit != 0) {
                battle_target_move_cursor_to_unit(unit);
                battle_menu_set_next_script_action_menus();
            }
        }
    } else if (input & PSX_PAD_CROSS) {
        battle_action_clear_at_list_id();
        unit = battle_unit_get_selectable_misc_data_at_map_coords(
            g_battle_cursor_x, g_battle_cursor_y, g_battle_cursor_z);
        if (unit != 0) {
            g_battle_casting_unit_id = unit->unit_id;
            battle_target_show_move_range_in_free_cursor();
        }
    } else if (input & PSX_PAD_START) {
        if (g_main_game_options.fields.multi_height_cursor_speed != GAME_MULTI_HEIGHT_CURSOR_SPEED_STOP) {
            main_sound_play_sfx(MAIN_SFX_CONFIRM);
            battle_action_is_at_list_unit_charging_ability();
            battle_target_store_cursor_unit_name_and_data();
        }
    } else if (input & PSX_PAD_TRIANGLE) {
        battle_action_clear_at_list_id();
        main_sound_play_sfx(MAIN_SFX_CONFIRM);
        battle_menu_open_free_cursor_mini_menu();
    }
}
