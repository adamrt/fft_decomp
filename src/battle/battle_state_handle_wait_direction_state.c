#include "fft/battle.h"
#include "psx/pad.h"

/* WaitDirection game-state handler: the acting unit picks the facing it ends its
 * turn with.
 *
 * A player-controlled unit shows the direction indicator and turns with the pad
 * (a mount or rider turns with it); circle ends the turn and runs the
 * between-turn events, cross returns to the after-command state. An AI-controlled
 * unit takes its stored facing hint (0-3, in quarter turns) and ends the turn
 * once its state frame counter reaches 0x1f.
 *
 * Ending the turn applies action_end_turn and re-registers the unit's tile
 * together with its mount partner. */
void battle_state_handle_wait_direction_state(void) {
    s32 facing;
    s32 facing_hint;
    u16 frame_data;
    battle_unit_t* unit;
    battle_unit_t* mounted_unit;

    unit = battle_unit_get_source();
    battle_gfx_update_all_unit_rotation_and_vectors();
    if (g_battle_menu_status_screen_selected != 1) {
        battle_unit_update_and_animate_units();
    }
    if ((unit->team_flags & BATTLE_TEAM_FLAG_PLAYER_CONTROLLED) != 0) {
        battle_gfx_draw_wait_direction_unit();
    }
    battle_gfx_update_status_bubbles_and_graphics();
    menu_update_menus_and_event_speed(main_gfx_get_otag(), g_main_input_current_buttons);
    if ((unit->team_flags & BATTLE_TEAM_FLAG_PLAYER_CONTROLLED) != 0) {
        camera_handle_rotation_input();
        camera_update_zoom_input();
        camera_update_tilt_input();
        if ((g_main_input_pressed_buttons & PSX_PAD_CIRCLE) != 0) {
            if (unit->stats != 0) {
                action_end_turn(unit->stats->battle_id);
                battle_unit_set_tile_position_with_mount_partner(unit);
            }
            main_sound_play_sfx(MAIN_SFX_CONFIRM);
            action_check_between_turn_events();
        } else if ((g_main_input_pressed_buttons & PSX_PAD_CROSS) != 0) {
            battle_state_enter_after_command();
        }
        /* The target passes only the mode; the definition's second parameter is a matching device. */
        facing = camera_get_input_direction(CAMERA_INPUT_PRESSED);
        if (facing != -1 && facing != g_battle_state_wait_direction_facing) {
            main_sound_play_sfx(MAIN_SFX_CURSOR_MOVE);
            g_battle_state_wait_direction_facing = facing;
        }
        battle_unit_decide_facing_direction(unit, facing);
        if (unit->mount_state != BATTLE_UNIT_MOUNT_STATE_NONE) {
            mounted_unit = battle_unit_get_by_runtime_id(unit->mount_partner_runtime_id);
            if (mounted_unit != 0) {
                battle_unit_decide_facing_direction(mounted_unit, facing);
            }
        }
    } else {
        facing_hint = unit->command_state.cursor.facing_hint;
        if (facing_hint < 4) {
            battle_unit_decide_facing_direction(unit, facing_hint << 10);
            if (unit->mount_state != BATTLE_UNIT_MOUNT_STATE_NONE) {
                mounted_unit = battle_unit_get_by_runtime_id(unit->mount_partner_runtime_id);
                if (mounted_unit != 0) {
                    battle_unit_decide_facing_direction(mounted_unit, unit->command_state.cursor.facing_hint << 10);
                }
            }
        }
        frame_data = unit->state_frame_counter++;
        if (frame_data >= 0x1f) {
            if (unit->stats != 0) {
                action_end_turn(unit->stats->battle_id);
                battle_unit_set_tile_position_with_mount_partner(unit);
            }
            action_check_between_turn_events();
        }
    }
}
