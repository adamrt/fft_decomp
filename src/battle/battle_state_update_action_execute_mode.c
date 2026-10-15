#include "fft/battle.h"
#include "fft/battle_action.h"
#include "fft/battle_gfx.h"
#include "fft/battle_target.h"
#include "fft/battle_unit.h"
#include "psx/types.h"

/* ActionExecute game-state handler: animate the strike, run the knockback step of
 * every target and, when everything has settled, refresh the targets and show the
 * battle message.
 *
 * The strike has settled when no effect cycle or target step is running
 * (g_battle_state_animation_continue_check), the casting unit shows no numeric
 * display, and its animation countdown is 0 or its animation id is below 0x3c.
 *
 * An ability other than Knockback and Throw Stone follows the cursor tile with
 * the camera only when it has no effect file or is a Charge ability; on settling
 * it restarts the background sound and refreshes each target's display. For a
 * reaction ability the target instead starts the post-attack animation display,
 * and only when the ability has no effect file and its attack animation is not
 * the weapon's. A plain attack (id 0), Knockback or Throw Stone always follows
 * the cursor tile and runs the menu layer each frame; Knockback prepares each
 * target's post-action display before the refresh. */
void battle_state_update_action_execute_mode(void) {
    battle_unit_t* unit;
    battle_unit_t* target;
    s32 i;

    battle_gfx_update_all_unit_rotation_and_vectors();
    if (g_battle_menu_status_screen_selected != 1) {
        battle_unit_update_and_animate_units();
        battle_gfx_update_screen_color_modulation_fade();
        camera_update_real_coord_animation();
        camera_update_screen_offset_animation();
        camera_update_zoom_animation();
        camera_update_rotation_animation();
    }
    battle_gfx_update_status_bubbles_and_graphics();
    battle_gfx_draw_screen_color_modulation_overlay();
    target_calculate_tile_coords_and_glow_from_at_list();

    unit = battle_unit_get_casting();
    /* The target reads the 16-bit value of g_main_system_animation_speed. */
    unit->state_frame_counter += g_main_system_animation_speed;
    if (unit->strike.target_count != 0) {
        for (i = 0; i < unit->strike.target_count; i++) {
            target = battle_unit_get_by_battle_id(unit->strike.target_list[i]);
            if (target != 0) {
                move_update_knockback_step(target);
                g_battle_state_animation_continue_check |= target->step_phase;
            }
        }
    }

    if (unit->used_ability_id != 0 && unit->used_ability_id != ABILITY_ID_KNOCKBACK
        && unit->used_ability_id != ABILITY_ID_BASIC_SKILL_THROW_STONE) {
        if (effect_load_ability(unit->used_ability_id) != 0
            || (unit->used_ability_id >= ABILITY_ID_CHARGE_FIRST && unit->used_ability_id < ABILITY_ID_MATH_FIRST)) {
            camera_update_cursor_tile_vector();
        }
        if (g_battle_state_animation_continue_check == 0 && unit->numeric_display.active == 0
            && (unit->animation.wait == 0 || (unit->animation.sequence_id >> 1) < 0x3cU)) {
            main_sound_replay_bg_sfx();
            if (unit->strike.target_count != 0) {
                for (i = 0; i < unit->strike.target_count; i++) {
                    target = battle_unit_get_by_battle_id(unit->strike.target_list[i]);
                    if (target != 0) {
                        if (unit->used_ability_id >= ABILITY_ID_REACTION_FIRST
                            && unit->used_ability_id < ABILITY_ID_SUPPORT_FIRST) {
                            if (effect_load_ability(unit->used_ability_id) != 0
                                && g_battle_ability_animation_data[unit->used_ability_id].attack_animation
                                    != BATTLE_ATTACK_ANIMATION_USE_WEAPON) {
                                /* Both arguments are runtime records overlaying the declared parameters. */
                                battle_unit_start_post_attack_animation_display(unit, target);
                            }
                        } else {
                            battle_unit_update_display_by_runtime_id(target->runtime_id);
                        }
                    }
                }
            }
            battle_state_start_battle_message_display();
        }
    } else {
        camera_update_cursor_tile_vector();
        menu_update_menus_and_event_speed(main_gfx_get_otag(), g_input_current_buttons);
        if (g_battle_state_animation_continue_check == 0 && unit->numeric_display.active == 0
            && (unit->animation.wait == 0 || (unit->animation.sequence_id >> 1) < 0x3cU)) {
            if (unit->strike.target_count != 0) {
                for (i = 0; i < unit->strike.target_count; i++) {
                    target = battle_unit_get_by_battle_id(unit->strike.target_list[i]);
                    if (target != 0) {
                        if (unit->used_ability_id == ABILITY_ID_KNOCKBACK) {
                            battle_gfx_prepare_post_action_display_by_runtime_id(target->runtime_id);
                        }
                        battle_unit_update_display_by_runtime_id(target->runtime_id);
                    }
                }
            }
            battle_state_start_battle_message_display();
        }
    }
}
