#include "fft/battle.h"
#include "fft/battle_action.h"
#include "fft/battle_camera.h"
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

/* IllegalRange game-state handler: when the message closes (command 7, 8 or
 * 0xff), return to the range display. */
void battle_state_handle_illegal_range_state(void) {
    s32 selected_command;

    battle_state_handle_free_cursor_input();
    menu_update_menus_and_event_speed(main_gfx_get_otag(), g_input_current_buttons);
    selected_command = menu_get_selected_command_address()->kind;
    if (selected_command >= 7 && (selected_command < 9 || selected_command == 0xff)) {
        target_set_boxes_red();
    }
}

/* AbilityPreviewHelp game-state handler: when the help window has gone
 * (battle_menu_is_still_building returns something other than 2), return to the
 * ability preview with map panning. */
void battle_state_handle_ability_preview_help_state(void) {
    if (menu_get_thread_activity_variant() != 2) {
        g_battle_menu_help_opening = 0;
        camera_enable_map_pan();
        g_battle_game_state = BATTLE_GAME_STATE_ABILITY_PREVIEW_HANDLING;
    }
    battle_state_handle_free_cursor_input();
    menu_update_menus_and_event_speed(main_gfx_get_otag(), g_input_current_buttons);
}

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
