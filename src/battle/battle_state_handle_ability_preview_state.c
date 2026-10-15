#include "fft/battle.h"
#include "fft/battle_action.h"
#include "fft/battle_target.h"
#include "psx/pad.h"

/* AbilityPreviewHandling game-state handler: the pending ability's targets are
 * marked and the unit confirms or backs out.
 *
 * For a player-controlled unit circle restores the cursor input mode and opens
 * the confirmation prompt (action_confirm). Cross clears the target marks
 * and returns to the idle action menu when the ability has no target choice,
 * otherwise to the range display (target_set_boxes_red). An AI-controlled
 * unit confirms by itself once its state frame counter reaches 0x1f. */
void battle_state_handle_ability_preview_state(void) {
    u16 frame_data;
    battle_unit_t* unit;

    unit = battle_unit_get_source();
    if ((unit->team_flags & BATTLE_TEAM_FLAG_PLAYER_CONTROLLED) != 0) {
        camera_handle_rotation_input();
        camera_update_zoom_input();
        camera_update_tilt_input();
        if ((g_main_input_pressed_buttons & PSX_PAD_CIRCLE) != 0) {
            g_battle_cursor_input_mode = g_battle_cursor_input_mode_saved;
            battle_gfx_update_unit_palettes();
            action_confirm();
        } else if ((g_main_input_pressed_buttons & PSX_PAD_CROSS) != 0) {
            g_battle_cursor_input_mode = g_battle_cursor_input_mode_saved;
            target_set_tile_background_color(BATTLE_TILE_MARK_HIDE_TARGETS, 3);
            if (unit->ability_preview_phase == TARGET_PANELS_NO_TARGETING) {
                target_set_tile_background_color(BATTLE_TILE_MARK_NONE, 3);
                battle_gfx_update_unit_palettes();
                battle_menu_open_active_unit_idle_action_menu();
            } else {
                target_set_tile_background_color(BATTLE_TILE_MARK_NONE, 3);
                battle_gfx_update_unit_palettes();
                target_set_boxes_red();
            }
        }
    } else {
        frame_data = unit->state_frame_counter;
        unit->state_frame_counter = frame_data + 1;
        if (frame_data >= 0x1f) {
            g_battle_cursor_input_mode = g_battle_cursor_input_mode_saved;
            battle_gfx_update_unit_palettes();
            action_confirm();
        }
    }
    battle_state_handle_free_cursor_input();
    menu_update_menus_and_event_speed(main_gfx_get_otag(), g_input_current_buttons);
}
