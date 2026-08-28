#include "fft/battle.h"
#include "fft/battle_gfx.h"
#include "fft/main_gfx.h"
#include "fft/main_runtime.h"
#include "fft/main_sound.h"
#include "psx/pad.h"

void battle_state_handle_wait_direction_state(void) {
    s32 facing;
    s32 facing_hint;
    u16 frame_data;
    battle_unit_misc_data_t* unit;
    battle_unit_misc_data_t* mounted_unit;

    unit = battle_unit_get_source_misc_data();
    battle_gfx_update_all_unit_rotation_and_vectors();
    if (g_battle_menu_status_screen_selected != 1) {
        battle_unit_update_and_animate_units();
    }
    if ((unit->team_flags & BATTLE_TEAM_FLAG_PLAYER_CONTROLLED) != 0) {
        battle_gfx_draw_wait_direction_unit();
    }
    battle_gfx_update_status_bubbles_and_graphics();
    battle_menu_draw_selection_data(main_gfx_get_otag(), g_controller_input_raw);
    if ((unit->team_flags & BATTLE_TEAM_FLAG_PLAYER_CONTROLLED) != 0) {
        battle_camera_handle_rotation_input();
        battle_camera_call_zoom_map();
        battle_camera_call_toggle_tilt();
        if ((g_controller_input_pressed & PSX_PAD_CIRCLE) != 0) {
            if (unit->battle_data != 0) {
                battle_action_end_turn(unit->battle_data->misc_unit_id);
                battle_unit_update_mount_animation_display(unit);
            }
            main_sound_play_sfx(MAIN_SFX_CONFIRM);
            battle_action_check_between_turn_events();
        } else if ((g_controller_input_pressed & PSX_PAD_CROSS) != 0) {
            battle_state_enter_after_command();
        }
        /* The target passes only the mode; the definition's second parameter is a matching device. */
        facing = ((s32 (*)(s32))battle_camera_get_input_direction)(1);
        if (facing != -1 && facing != g_current_facing_direction) {
            main_sound_play_sfx(MAIN_SFX_CURSOR_MOVE);
            g_current_facing_direction = facing;
        }
        battle_unit_decide_facing_direction(unit, facing);
        if (unit->mount_state != BATTLE_MISC_MOUNT_STATE_NONE) {
            mounted_unit = battle_unit_get_misc_data_by_misc_id(unit->mount_partner_misc_id);
            if (mounted_unit != 0) {
                battle_unit_decide_facing_direction(mounted_unit, facing);
            }
        }
    } else {
        facing_hint = unit->command_state.cursor.facing_hint;
        if (facing_hint < 4) {
            battle_unit_decide_facing_direction(unit, facing_hint << 10);
            if (unit->mount_state != BATTLE_MISC_MOUNT_STATE_NONE) {
                mounted_unit = battle_unit_get_misc_data_by_misc_id(unit->mount_partner_misc_id);
                if (mounted_unit != 0) {
                    battle_unit_decide_facing_direction(mounted_unit, unit->command_state.cursor.facing_hint << 10);
                }
            }
        }
        frame_data = unit->state_frame_counter++;
        if (frame_data >= 0x1f) {
            if (unit->battle_data != 0) {
                battle_action_end_turn(unit->battle_data->misc_unit_id);
                battle_unit_update_mount_animation_display(unit);
            }
            battle_action_check_between_turn_events();
        }
    }
}
