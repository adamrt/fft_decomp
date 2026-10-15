#include "fft/battle.h"
#include "fft/battle_target.h"
#include "psx/pad.h"

/* Free-cursor movement step. With the acting unit under player control the
 * confirm button routes the cursor to the pathfinder and, on a reachable
 * destination, installs the returned step list on the casting record and
 * enters the moving state. Without player control the same happens from the
 * unit's own stored target after 0x1f frames. */
void battle_state_handle_close_move_help_state(void) {
    battle_unit_t* source;
    battle_unit_t* casting;
    battle_walk_path_t* path;
    s32 buttons;
    u16 frame;
    s32 x;
    s32 elevation;
    s32 y;
    battle_unit_command_state_t scratch;

    source = battle_unit_get_source();
    casting = battle_unit_get_casting();
    battle_state_handle_free_cursor_input();
    menu_update_menus_and_event_speed(main_gfx_get_otag(), g_input_current_buttons);
    if (source->team_flags & BATTLE_TEAM_FLAG_PLAYER_CONTROLLED) {
        target_move_cursor_by_input();
        camera_handle_rotation_input();
        camera_update_zoom_input();
        camera_update_tilt_input();
        buttons = g_main_input_pressed_buttons;
        if (buttons & PSX_PAD_CIRCLE) {
            path = move_build_path_to_tile(
                casting->stats->battle_id, g_battle_cursor_x, g_battle_cursor_y, g_battle_cursor_z);
            if (path != 0) {
                /* Copy the 0x80-byte movement block in the renderer-side runtime record. */
                casting->path = *path;
                casting->walk_speed = 0x2000;
                target_set_tile_background_color(BATTLE_TILE_MARK_NONE, 1);
                main_sound_play_sfx(MAIN_SFX_CONFIRM);
                battle_state_enter_unit_moving_setup();
                return;
            }
            battle_menu_open_illegal_move_help();
            return;
        }
        if (buttons & PSX_PAD_CROSS) {
            target_set_tile_background_color(BATTLE_TILE_MARK_NONE, 1);
            target_move_cursor_to_unit(source);
            battle_menu_open_active_unit_idle_action_menu();
            return;
        }
        if (buttons & PSX_PAD_TRIANGLE) {
            target_move_cursor_to_unit(battle_unit_get_source());
            target_store_cursor_unit_name_and_data();
        }
        return;
    }
    frame = source->state_frame_counter;
    source->state_frame_counter = frame + 1;
    if (frame < 0x1f) {
        return;
    }
    path = move_build_path_to_tile(casting->stats->battle_id, source->command_state.ai.data.move.x,
        source->command_state.ai.data.move.y, source->command_state.ai.data.move.layer);
    target_set_tile_background_color(BATTLE_TILE_MARK_NONE, 1);
    if (path != 0) {
        x = source->command_state.ai.data.move.x;
        elevation = source->command_state.ai.data.move.layer;
        y = source->command_state.ai.data.move.y;
        g_battle_cursor_x = x;
        g_battle_cursor_z = elevation;
        g_battle_cursor_y = y;
        casting->path = *path;
        casting->walk_speed = 0x2000;
        battle_state_enter_unit_moving_setup();
        return;
    }
    battle_menu_set_next_script_action_menus();
}
