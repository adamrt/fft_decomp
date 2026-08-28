#include "fft/battle.h"
#include "fft/battle_camera.h"
#include "fft/battle_move.h"
#include "fft/main_gfx.h"
#include "fft/main_runtime.h"
#include "fft/main_sound.h"
#include "psx/pad.h"

/* The 0x80 bytes at 0x9c..0x11c of the renderer-side Misc record (step count,
 * step list, movement flags and mount byte) copied as one unaligned block. */
typedef struct {
    u8 bytes[0x80];
} battle_movement_path_block_t;

/* Free-cursor movement step. With the acting unit under player control the
 * confirm button routes the cursor to the pathfinder and, on a reachable
 * destination, installs the returned step list on the casting record and
 * enters the moving state. Without player control the same happens from the
 * unit's own stored target after 0x1f frames. */
void battle_state_handle_close_move_help_state(void) {
    battle_unit_misc_data_t* source;
    battle_unit_misc_data_t* casting;
    u8* path;
    s32 buttons;
    u16 frame;
    s32 x;
    s32 elevation;
    s32 y;
    battle_unit_command_state_t scratch;

    source = battle_unit_get_source_misc_data();
    casting = battle_unit_get_casting_misc_data();
    battle_state_handle_free_cursor_input();
    battle_menu_draw_selection_data(main_gfx_get_otag(), g_controller_input_raw);
    if (source->team_flags & 8) {
        battle_target_move_cursor_by_input();
        battle_camera_handle_rotation_input();
        battle_camera_call_zoom_map();
        battle_camera_call_toggle_tilt();
        buttons = g_controller_input_pressed;
        if (buttons & PSX_PAD_CIRCLE) {
            path = battle_move_build_path_to_tile(
                casting->battle_data->misc_unit_id, g_battle_cursor_x, g_battle_cursor_y, g_battle_cursor_z);
            if (path != 0) {
                *(battle_movement_path_block_t*)&casting->movement_path_count = *(battle_movement_path_block_t*)path;
                casting->walk_speed.word = 0x2000;
                battle_target_set_tile_background_color(0, 1);
                main_sound_play_sfx(MAIN_SFX_CONFIRM);
                battle_state_enter_unit_moving_setup();
                return;
            }
            battle_menu_open_illegal_move_help();
            return;
        }
        if (buttons & PSX_PAD_CROSS) {
            battle_target_set_tile_background_color(0, 1);
            battle_target_move_cursor_to_unit(source);
            battle_menu_open_active_unit_idle_action_menu();
            return;
        }
        if (buttons & PSX_PAD_TRIANGLE) {
            battle_target_move_cursor_to_unit(battle_unit_get_source_misc_data());
            battle_target_store_cursor_unit_name_and_data();
        }
        return;
    }
    frame = source->state_frame_counter;
    source->state_frame_counter = frame + 1;
    if (frame < 0x1f) {
        return;
    }
    path = battle_move_build_path_to_tile(casting->battle_data->misc_unit_id,
        *(s16*)&source->command_state.ai.data.move.x, *(s16*)&source->command_state.ai.data.move.y,
        *(s16*)&source->command_state.ai.data.move.elevation);
    battle_target_set_tile_background_color(0, 1);
    if (path != 0) {
        x = *(s16*)&source->command_state.ai.data.move.x;
        elevation = *(s16*)&source->command_state.ai.data.move.elevation;
        y = *(s16*)&source->command_state.ai.data.move.y;
        g_battle_cursor_x = x;
        g_battle_cursor_z = elevation;
        g_battle_cursor_y = y;
        *(battle_movement_path_block_t*)&casting->movement_path_count = *(battle_movement_path_block_t*)path;
        casting->walk_speed.word = 0x2000;
        battle_state_enter_unit_moving_setup();
        return;
    }
    battle_menu_set_next_script_action_menus();
}
