#include "fft/battle.h"
#include "fft/battle_action.h"
#include "fft/battle_target.h"
#include "psx/pad.h"

/* Free-cursor game state: camera and cursor input, then the button actions
 * (help, team highlight, act menu, move range, AT list and mini menu). */
void battle_state_handle_default_state(void) {
    battle_unit_t* unit;
    s32 input;
    RECT rect; /* never used; retail reserves its 8 bytes */

    camera_handle_rotation_input();
    camera_update_zoom_input();
    camera_update_tilt_input();
    target_move_cursor_by_input();
    battle_state_handle_free_cursor_input();
    menu_update_menus_and_event_speed(main_gfx_get_otag(), g_input_current_buttons);
    input = g_main_input_pressed_buttons;
    if (input & PSX_PAD_SELECT) {
        action_clear_at_list_id();
        battle_menu_open_free_cursor_help();
    } else if (input & PSX_PAD_SQUARE) {
        action_clear_at_list_id();
        battle_state_enter_highlight_units_by_team();
    } else if (input & PSX_PAD_CIRCLE) {
        target_set_tile_background_color(BATTLE_TILE_MARK_NONE, 0);
        main_sound_play_sfx(MAIN_SFX_CONFIRM);
        if (battle_unit_find_selectable_at_tile(g_battle_cursor_x, g_battle_cursor_y, g_battle_cursor_z) != 0) {
            battle_menu_dispatch_idle_action_menu();
        } else {
            unit = battle_unit_get_source();
            if (unit != 0) {
                target_move_cursor_to_unit(unit);
                battle_menu_set_next_script_action_menus();
            }
        }
    } else if (input & PSX_PAD_CROSS) {
        action_clear_at_list_id();
        unit = battle_unit_find_selectable_at_tile(g_battle_cursor_x, g_battle_cursor_y, g_battle_cursor_z);
        if (unit != 0) {
            g_battle_casting_runtime_id = unit->runtime_id;
            target_show_move_range_in_free_cursor();
        }
    } else if (input & PSX_PAD_START) {
        if (g_main_game_options.fields.multi_height_cursor_speed != GAME_MULTI_HEIGHT_CURSOR_SPEED_STOP) {
            main_sound_play_sfx(MAIN_SFX_CONFIRM);
            action_show_next_at_list_entry();
            target_store_cursor_unit_name_and_data();
        }
    } else if (input & PSX_PAD_TRIANGLE) {
        action_clear_at_list_id();
        main_sound_play_sfx(MAIN_SFX_CONFIRM);
        battle_menu_open_free_cursor_mini_menu();
    }
}
