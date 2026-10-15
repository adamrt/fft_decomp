#include "fft/battle.h"
#include "fft/battle_action.h"
#include "fft/battle_gfx.h"
#include "fft/battle_target.h"
#include "fft/battle_unit.h"
#include "psx/types.h"

/* Start the target's reaction to the attacker's action and its post-action
 * display. Both parameters are unit records; byte 4 is the unit id. */
void battle_unit_start_post_attack_animation_display(battle_unit_t* attacker, battle_unit_t* target) {
    if (attacker != 0 && target != 0) {
        battle_unit_set_target_animation_from_attack_type(attacker, target);
        battle_gfx_prepare_post_action_display_by_runtime_id(target->runtime_id);
    }
}

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

/* BattleMessageDisplay game-state handler: when the message closes (command 7, 8
 * or 0xff), queue and show the casting unit's post-effect messages. */
void battle_state_handle_battle_message_display_state(void) {
    s32 selected_command;

    battle_state_handle_free_cursor_input();
    menu_update_menus_and_event_speed(main_gfx_get_otag(), g_input_current_buttons);
    selected_command = menu_get_selected_command_address()->kind;
    if (selected_command >= 7 && (selected_command < 9 || selected_command == 0xff)) {
        g_action_post_action = 1;
    }
    if (g_action_post_action != 0) {
        action_show_caster_post_effect_messages();
    }
}

/* ResumeAttackPhase game-state handler: show the remaining post-effect messages,
 * then finish the strike.
 *
 * Each closed message (command 7, 8 or 0xff) lets
 * action_resume_attack_phase_control open the next queued one. When none
 * remains and the casting unit's numeric display is idle, a continued attack
 * starts its next strike. Otherwise the SP2 buffer and VRAM slot are released,
 * the acting unit's data is stored and the units return to their status
 * animations. Outside the pre-action phase the action phase then advances (up to
 * the reacting-unit phase) and the next due action is announced; when none is
 * due, or in the pre-action phase, the JP and EXP rewards begin.
 *
 * The facing computed for the acting unit's own action is passed to
 * action_post_action_facing_stub, which is empty. */
void battle_state_handle_resume_attack_phase_state(void) {
    s32 facing;
    unit_stats_t* stats;
    s32 selected_command;
    battle_unit_t* unit;

    battle_state_handle_free_cursor_input();
    menu_update_menus_and_event_speed(main_gfx_get_otag(), g_input_current_buttons);
    selected_command = menu_get_selected_command_address()->kind;
    if (selected_command >= 7 && (selected_command < 9 || selected_command == 0xff)) {
        g_action_post_action = 1;
    }
    unit = battle_unit_get_casting();
    if (g_action_post_action != 0 && action_resume_attack_phase_control() == 0 && unit->numeric_display.active == 0) {
        if (unit->strike.continue_attack != 0) {
            unit->continue_attack_count += 1;
            action_set_damage_display_type_based_on_ability();
            return;
        }
        if (g_battle_gfx_sp2_data != 0) {
            main_heap_free(g_battle_gfx_sp2_data);
            g_battle_gfx_sp2_data = 0;
        }
        battle_gfx_invalidate_sp2_vram_slot(unit);
        if (g_action_turn_event_type == BATTLE_TURN_EVENT_UNIT_READY && g_action_phase == ACTION_PHASE_ACTING_UNIT) {
            facing = unit->facing;
            if (facing < 0) {
                facing += 0x3ff;
            }
            facing = (u32)facing >> 10;
            stats = unit->stats;
            action_post_action_facing_stub(stats, facing & 0xff);
        }
        action_store_acting_unit_data(unit->stats);
        battle_unit_call_set_animation_based_on_status(unit);
        battle_unit_update_anim_display_for_all_targets(unit);
        battle_unit_get_source();
        if (g_action_phase != ACTION_PHASE_PRE_ACTION) {
            if (g_action_phase != ACTION_PHASE_REACTING_UNIT) {
                g_action_phase += 1;
            }
            if (battle_state_announce_next_charged_action() != 0) {
                return;
            }
        }
        action_handle_post_action_xp_jp_ability();
    }
}

/* LearnAbilityOnHit game-state handler: steps through the post-action reports
 * (level up, job level up, learn on hit).
 *
 * Command 7 on the learn-on-hit report teaches the used ability to the unit found
 * for it. Commands 7, 8 and 0xff all advance to the next report, and after the
 * third the action is marked finished. From then on, once no animation is pending
 * and neither the casting unit nor any of its targets still shows a numeric
 * display, the source unit returns to its status animation and the after-command
 * state begins. */
void battle_state_handle_learn_ability_on_hit_state(void) {
    ai_command_t* selected_command_address;
    s32 selected_command;
    s32 counter;
    s32 skip_animation;
    s32 hit_count;
    battle_unit_t* casting_unit;
    battle_unit_t* target_unit;
    u8 compiler_stack_pad[8];

    battle_state_handle_free_cursor_input();
    menu_update_menus_and_event_speed(main_gfx_get_otag(), g_input_current_buttons);
    selected_command_address = menu_get_selected_command_address();
    casting_unit = battle_unit_get_casting();
    selected_command = selected_command_address->kind;
    switch (selected_command) {
    case 7:
        if (g_action_post_action_display_phase == BATTLE_POST_ACTION_DISPLAY_LEARN_ON_HIT) {
            target_unit = battle_unit_get_by_battle_id(g_action_learn_on_hit_unit_id);
            battle_unit_learn_used_ability(target_unit->stats);
        }
    case 8:
    case 0xff:
        g_action_post_action_display_phase += 1;
        if (g_action_post_action_display_phase >= 3) {
            g_action_post_action = 1;
        } else {
            action_init_learn_ability_on_hit();
        }
        break;
    }
    if (g_action_post_action != 0 && g_battle_state_animation_continue_check == 0) {
        counter = 0;
        if (casting_unit->numeric_display.active == 0) {
            hit_count = casting_unit->strike.target_count;
            skip_animation = 0;
            if (hit_count > 0) {
                do {
                    target_unit = battle_unit_get_by_battle_id(casting_unit->strike.target_list[counter]);
                    if (target_unit != 0 && target_unit->numeric_display.active != 0) {
                        skip_animation = 1;
                        break;
                    }
                    hit_count = casting_unit->strike.target_count;
                    counter += 1;
                } while (counter < hit_count);
            }
            if (skip_animation == 0) {
                casting_unit = battle_unit_get_source();
                battle_unit_call_set_animation_based_on_status(casting_unit);
                battle_unit_update_post_command_animation_display(casting_unit);
            }
        }
    }
}
