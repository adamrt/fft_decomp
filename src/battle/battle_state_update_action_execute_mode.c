#include "fft/battle.h"

void battle_state_update_action_execute_mode(void) {
    battle_unit_misc_data_t* unit;
    battle_unit_misc_data_t* target;
    s32 i;

    battle_gfx_update_all_unit_rotation_and_vectors();
    if (g_battle_menu_status_screen_selected != 1) {
        battle_unit_update_and_animate_units();
        battle_gfx_update_screen_color_modulation_fade();
        battle_camera_update_real_coord_animation();
        battle_camera_update_offset_screen_coord_animation();
        battle_camera_update_zoom_animation();
        battle_camera_update_rotation_animation();
    }
    battle_gfx_update_status_bubbles_and_graphics();
    battle_gfx_draw_screen_color_modulation_overlay();
    battle_target_calculate_tile_coords_and_glow_from_at_list();

    unit = battle_unit_get_casting_misc_data();
    /* The target reads the halfword of g_animation_speed. */
    unit->state_frame_counter += (u16)g_animation_speed;
    if (unit->target_count != 0) {
        for (i = 0; i < unit->target_count; i++) {
            target = battle_unit_get_misc_data_by_battle_id(unit->target_list[i]);
            if (target != 0) {
                battle_move_update_knockback_step(target);
                g_battle_state_animation_continue_check |= target->centre_tile_offset;
            }
        }
    }

    if (unit->used_ability_id != 0 && unit->used_ability_id != 0x200
        && unit->used_ability_id != ABILITY_ID_BASIC_SKILL_THROW_STONE) {
        if (battle_effect_load_ability(unit->used_ability_id) != 0 || (u32)(unit->used_ability_id - 0x196) < 8U) {
            battle_camera_update_cursor_tile_vector();
        }
        if (g_battle_state_animation_continue_check == 0 && unit->numeric_display_active == 0
            && (unit->animation_countdown == 0 || (u32)(unit->encoded_animation >> 1) < 0x3cU)) {
            main_sound_replay_weather_sfx();
            if (unit->target_count != 0) {
                for (i = 0; i < unit->target_count; i++) {
                    target = battle_unit_get_misc_data_by_battle_id(unit->target_list[i]);
                    if (target != 0) {
                        if ((u32)(unit->used_ability_id - 0x1a6) < 0x20U) {
                            if (battle_effect_load_ability(*(volatile u16*)&unit->used_ability_id) != 0
                                && g_battle_ability_animation_data[unit->used_ability_id].attack_animation != 0) {
                                /* Both arguments are misc records overlaying the declared parameters. */
                                battle_unit_start_post_attack_animation_display((s32)unit, (u8*)target);
                            }
                        } else {
                            battle_unit_update_display_by_misc_id(target->unit_id);
                        }
                    }
                }
            }
            battle_state_start_battle_message_display();
        }
    } else {
        battle_camera_update_cursor_tile_vector();
        battle_menu_draw_selection_data(main_gfx_get_otag(), g_controller_input_raw);
        if (g_battle_state_animation_continue_check == 0 && unit->numeric_display_active == 0
            && (unit->animation_countdown == 0 || (u32)(unit->encoded_animation >> 1) < 0x3cU)) {
            if (unit->target_count != 0) {
                for (i = 0; i < unit->target_count; i++) {
                    target = battle_unit_get_misc_data_by_battle_id(unit->target_list[i]);
                    if (target != 0) {
                        if (unit->used_ability_id == 0x200) {
                            battle_gfx_prepare_post_action_display_by_misc_id(target->unit_id);
                        }
                        battle_unit_update_display_by_misc_id(target->unit_id);
                    }
                }
            }
            battle_state_start_battle_message_display();
        }
    }
}
