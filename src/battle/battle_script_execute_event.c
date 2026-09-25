#include "fft/attack.h"
#include "fft/battle.h"
#include "fft/battle_effect.h"
#include "fft/battle_text.h"
#include "fft/event.h"
#include "fft/main_runtime.h"
#include "fft/menu.h"
#include "fft/script_variables.h"
#include "psx/pad.h"
#include "psx/types.h"
typedef struct battle_script_jump_instruction battle_script_jump_instruction_t;

/* BATTLE scenario interpreter.
 *
 * One cooperative thread runs this loop: fetch the opcode and its first four
 * operand bytes, dispatch through the else-if chain below, then advance by the
 * opcode's encoded length. The chain order is the target's compare order and
 * must not be re-sorted. Handlers that seek (jumps) `continue` without the
 * length step; EventEnd may `goto restart_script` for a chained scenario.
 *
 * WORLD carries the same interpreter (world_script_execute_event); the two are
 * kept structurally identical so differences stay visible.
 *
 * Unresolved external signatures below remain visibly provisional. The native
 * trampoline forwards arbitrary callback arguments; its () is intentional. */

/* Declared locally: other reconstructions bind these under different
 * provisional signatures, or the symbol is still unresolved. */
s32 battle_map_set_tile_data_value();
void battle_script_jump_event_instruction(battle_script_jump_instruction_t* instr);
void battle_script_warp_unit_display_to_paired_unit(s32 misc_id);
s32 battle_script_run_scenario_conditions();

void battle_script_execute_event(void) {
    s32 added_unit_state;
    u8* message_cursor;
    u8* effect_cursor;
    s32 finish_operation;
    u32 copy_value;
    s16 first_halfword;
    s32 remove_unit_misc_id;
    s32 reset_graphic_misc_id;
    s16 shadow_misc_id;
    s16 evtchr_palette_misc_id;
    s16 thrown_item_misc_id;
    s16 arrow_source_misc_id;
    s16 arrow_target_misc_id;
    s16 copied_position;
    s32 ghost_misc_id;
    s32 region_misc_id;
    s32 draw_misc_id;
    s32 facing_misc_id;
    s32 sprite_thread_offset;
    s32 date_month;
    s32 message_thread_offset;
    battle_stats_t* added_unit;
    s32 existing_message_thread_offset;
    s32 arrow_target_battle_id;
    s32 confirm_buttons;
    s32 instruction_offset;
    s32 added_battle_id;
    s32 wait_thread_index;
    s32 work_index;
    s32 finish_task_id;
    event_opcode_e opcode;
    /* Initially four eagerly fetched operand bytes, then per-opcode scratch.
     * They are not four uniformly encoded script arguments. */
    s32 operand_2;
    s32 operand_3;
    s32 operand_4;
    s32 operand_1;
    u8* position_instruction;
    u8* message_instruction;
    s32* script_variables;
    /* Pin: unpinned, `parameters` and `operand_1` swap $s1/$s2. */
    register u8* parameters __asm__("$17");
    u8* ghost_parameters;
    u8* instruction;
    map_tile_t* tile;
    battle_stats_t* copy_destination;
    u8* effect_instruction;
    battle_thread_t* wait_thread;
    battle_thread_t* cancel_thread;

    battle_thread_set_current_task_id(NATIVE_THREAD_TASK_EXECUTE_EVENT);
    if (battle_script_is_tutorial_event_slot() != 0) {
        battle_script_switch_tutorial_thread_for_event_instructions();
    }
    if (g_battle_menu_input_disabled == 0) {
        for (work_index = 2; work_index < 0xE; work_index++) {
            battle_thread_wait_until_inactive(work_index);
        }
        battle_camera_wait_until_idle();
    } else {
        g_battle_menu_hovered_unit_stats_display.y = 0xAA;
        g_battle_menu_attack_caster_stats_display.y = 0xAA;
        g_battle_menu_attack_target_stats_display.y = 0xAA;
        g_battle_menu_right_unit_stats_display.y = 0xAA;
    }
    battle_unit_clear_status_staging_data();
    if (battle_script_get_variable(EVENT_SCRIPT_VAR_PENDING_STAGED_STATUS) != 0) {
        battle_unit_update_staged_status_data(0, 0);
    } else {
        battle_process_inflict_status_commands();
    }
    battle_script_set_variable(EVENT_SCRIPT_VAR_DISABLED_MENU_ACTIONS, 0);
    battle_script_set_variable(EVENT_SCRIPT_VAR_DISABLED_CONTROLLER_INPUTS, 0);
    battle_script_set_variable(EVENT_SCRIPT_VAR_FORCED_CONTROLLER_INPUTS, 0);
restart_script:
    battle_script_set_event_speed(1U);
    instruction_offset = 0;
    battle_camera_normalize_yaw_angle();
    {
        s32 empty_unit_id = EVENT_UNIT_SLOT_EMPTY;
        /* Matching: load the fill value before the descending loop index. */
        for (work_index = 3; work_index >= 0; work_index--) {
            g_battle_event_unit_slots[work_index] = empty_unit_id;
        }
    }
    for (;;) {
        instruction = (u8*)g_battle_event_block;
        instruction += instruction_offset;
        parameters = instruction + 1;
        operand_1 = instruction[1];
        operand_2 = instruction[2];
        operand_3 = instruction[3];
        operand_4 = instruction[4];
        opcode = instruction[0];
        first_halfword = battle_script_load_halfword(parameters);
        /* ---- Control: NOPs, conditions, and script variable arithmetic ---- */
        if (opcode == EVENT_OPCODE_UNKNOWN_C0) {
        } else if (opcode == EVENT_OPCODE_NOP) {
        } else if ((u32)(opcode - EVENT_CONDITION_SECOND_GTE_FIRST) < 6U) {
            battle_script_run_condition(opcode);
        } else if ((u32)(opcode - EVENT_OPCODE_ADD_IMMEDIATE) < 0xFU) {
            operand_1 = first_halfword & 0xFFFF;
            battle_script_run_variable_command(opcode, operand_1,
                battle_script_load_halfword(instruction_offset + ((u8*)g_battle_event_block) + 3) & 0xFFFF, 0);
        } else if (opcode == EVENT_OPCODE_UNKNOWN_4D) {
            g_battle_event_map_jump_in_request = (s16)operand_1;
            battle_thread_wait_frames(2);
        }
        /* ---- Scenario progression: DismissUnit, CallFunction, BlueRemoveUnit, EventEnd ---- */
        else if (opcode == EVENT_OPCODE_DISMISS_UNIT) {
            if (battle_script_get_variable(EVENT_SCRIPT_VAR_SUPPRESS_PROGRESS_EFFECTS) == 0) {
                battle_dismiss_unit_event_instruction(first_halfword);
            }
        } else if (opcode == EVENT_OPCODE_CALL_FUNCTION) {
            /* These are sequential tests, not switch alternatives: selectors
             * 6 and 14 reuse operand_1 as a loop index, and later tests see
             * its changed value. Preserve that target-visible fallthrough. */
            battle_thread_wait_frames(4);
            if (battle_script_get_variable(EVENT_SCRIPT_VAR_SUPPRESS_PROGRESS_EFFECTS) == 0) {
                if (operand_1 == EVENT_CALL_FUNCTION_JOIN_UNITS_SILENTLY) {
                    if (battle_script_get_variable(EVENT_SCRIPT_VAR_CURRENT_EVENT) < 0x4E) {
                        main_item_init_new_game_inventory();
                    }
                    battle_script_join_units_silently_without_needing_darkscreen();
                }
                if (operand_1 == EVENT_CALL_FUNCTION_OPEN_SAVE_MENU) {
                    operand_1 = 2;
                    do {
                        battle_thread_set_parameters(operand_1, 0U, 0, 1);
                        g_battle_thread_contexts[operand_1].task_id = NATIVE_THREAD_TASK_STOP_REQUEST;
                        battle_thread_wait_until_inactive(operand_1);
                        operand_1 += 1;
                    } while (operand_1 < 0xF);
                    if (battle_script_get_variable(EVENT_SCRIPT_VAR_CURRENT_EVENT) != 0x12C) {
                        battle_script_set_variable(EVENT_SCRIPT_VAR_NEXT_SCENARIO, 1);
                    }
                    battle_script_set_variable(EVENT_SCRIPT_VAR_SAVE_IN_PROGRESS, 1);
                    battle_menu_request_open_companion_executable(0xE);
                    g_companion_overlay_state = 5;
                    do {
                        battle_thread_yield();
                    } while (g_companion_overlay_state != 0);
                    battle_script_set_variable(EVENT_SCRIPT_VAR_NEXT_SCENARIO, 0);
                    operand_1 = 2;
                    do {
                        battle_thread_set_parameters(operand_1, 0U, 0, 1);
                        g_battle_thread_contexts[operand_1].task_id = NATIVE_THREAD_TASK_STOP_REQUEST;
                        battle_thread_wait_until_inactive(operand_1);
                        operand_1 += 1;
                    } while (operand_1 < 0xF);
                }
            }
            if ((operand_1 == EVENT_CALL_FUNCTION_ADVANCE_YEAR)
                || (operand_1 == EVENT_CALL_FUNCTION_ADVANCE_YEAR_ALTERNATE)) {
                battle_script_advance_date_by_one_year();
            }
            if (operand_1 == EVENT_CALL_FUNCTION_INCREASE_BLUE_TEAM_BRAVE) {
                battle_unit_increase_blue_team_brave_by_10();
            }
            if (operand_1 == EVENT_CALL_FUNCTION_REMOVE_ALL_ENEMY_UNITS) {
                battle_script_blueremoveunit_all_enemy_units();
                battle_thread_wait_frames(0x78);
            }
            if (operand_1 == EVENT_CALL_FUNCTION_UNKNOWN_05) {
                g_battle_camera_wrap_yaw_pending = 1;
            }
            if (operand_1 == EVENT_CALL_FUNCTION_UNKNOWN_08) {
                g_main_debug_display_enabled = 0;
                D_800459D8 = 0;
            }
            if (operand_1 == EVENT_CALL_FUNCTION_MUTE_TEXT_AUDIO_CUE) {
                battle_script_set_variable(EVENT_SCRIPT_VAR_MUTE_TEXT_AUDIO_CUE, 1);
            }
            if (operand_1 == EVENT_CALL_FUNCTION_UNMUTE_TEXT_AUDIO_CUE) {
                battle_script_set_variable(EVENT_SCRIPT_VAR_MUTE_TEXT_AUDIO_CUE, 0);
            }
            if (operand_1 == EVENT_CALL_FUNCTION_ADVANCE_MAP_DESTRUCTION_AND_WAIT) {
                battle_script_set_specialized_map_destroyed();
                if (g_battle_map_destruction_wait_state != 0) {
                    do {
                        battle_thread_yield();
                    } while (g_battle_map_destruction_wait_state != 0);
                }
            }
            if (operand_1 == EVENT_CALL_FUNCTION_PLAY_ALTIMA_MUSIC) {
                g_battle_current_music_track_index = 0;
                g_battle_music_track_1_id = 0x13;
                g_battle_music_track_2_id = 0;
                battle_sound_play_music_tracks(0x13, 0);
            }
            if (operand_1 == EVENT_CALL_FUNCTION_COPY_UNIT_8_POSITION_TO_UNIT_9_AND_RELOAD_GRAPHICS) {
                parameters = (u8*)battle_unit_get_stats_from_battle_id(7);
                copy_destination = battle_unit_get_stats_from_battle_id(8);
                copy_destination->x = (u8)((battle_stats_t*)parameters)->x;
                copy_destination->position.bits.y = (u8)((battle_stats_t*)parameters)->position.bits.y;
                copy_value = ((battle_stats_t*)parameters)->position.raw & 0x8000;
                copy_destination->position.raw = (copy_destination->position.raw & 0x7FFF) | copy_value;
                battle_gfx_load_unit_graphics_by_battle_id(8, 1U);
                g_battle_unit_graphics_load_pending = 1;
                do {
                    battle_thread_yield();
                } while (g_battle_unit_graphics_load_pending != 0);
                battle_thread_yield();
                battle_thread_yield();
            }
            if (operand_1 == EVENT_CALL_FUNCTION_RESET_BETHLA_REGION_UNIT_GRAPHICS) {
                region_misc_id = 0;
                do {
                    if (battle_unit_has_misc_id(region_misc_id) != 0) {
                        operand_1 = battle_unit_get_battle_index_by_misc_id(region_misc_id);
                        if (operand_1 != -1U) {
                            parameters = (u8*)battle_unit_get_stats_from_battle_id(operand_1);
                            if (((u8)((battle_stats_t*)parameters)->x < 4U)
                                && ((u8)((battle_stats_t*)parameters)->position.bits.y < 0xBU)) {
                                battle_gfx_reset_unit_graphic_trigger(region_misc_id);
                            }
                            if (((u32)(((battle_stats_t*)parameters)->x - 4) < 3U)
                                && ((u32)(((battle_stats_t*)parameters)->position.bits.y - 2) < 6U)) {
                                battle_gfx_reset_unit_graphic_trigger(region_misc_id);
                            }
                        }
                    }
                    region_misc_id += 1;
                } while (region_misc_id < EVENT_UNIT_SLOT_COUNT);
            }
            if (operand_1 == EVENT_CALL_FUNCTION_COPY_UNIT_2_POSITION_AND_FACING_TO_UNIT_8_AND_RELOAD_GRAPHICS) {
                parameters = (u8*)battle_unit_get_stats_from_battle_id(1);
                copy_destination = battle_unit_get_stats_from_battle_id(7);
                copy_value = (u8)((battle_stats_t*)parameters)->x;
                copy_destination->x = copy_value;
                copy_destination->position.bits.y = (u8)((battle_stats_t*)parameters)->position.bits.y;
                copy_value = ((battle_stats_t*)parameters)->position.raw & 0x8000;
                copied_position = (copy_destination->position.raw & 0x7FFF) | copy_value;
                copy_destination->position.raw = copied_position;
                copy_destination->position.raw
                    = (s16)((copied_position & 0xF0FF) | (((battle_stats_t*)parameters)->position.raw & 0xF00));
                battle_gfx_load_unit_graphics_by_battle_id(7, 1U);
                g_battle_unit_graphics_load_pending = 1U;
                do {
                    battle_thread_yield();
                } while (g_battle_unit_graphics_load_pending != 0);
                battle_thread_yield();
                battle_thread_yield();
            }
            if ((u32)(operand_1 - EVENT_CALL_FUNCTION_WARP_UNIT_4_DISPLAY_TO_UNIT_2_POSITION_AND_FACING) < 2U) {
                battle_script_warp_unit_display_to_paired_unit(operand_1);
            }
            if (operand_1 == EVENT_CALL_FUNCTION_CLEAR_ZALERA_ACTIVE_STATUS_GRAPHICS) {
                draw_misc_id = (s16)battle_get_misc_id(CHARACTER_IDENTITY_ZALERA);
                if (draw_misc_id != EVENT_MISC_ID_NONE) {
                    battle_status_queue_current_status_graphics(
                        battle_unit_get_battle_index_by_misc_id(draw_misc_id), 0);
                    g_battle_thread_call_target = (void (*)(void))battle_unit_update_graphics_by_misc_id_wrapper;
                    battle_thread_call_on_main_stack(draw_misc_id);
                }
            }
        } else if (opcode == EVENT_OPCODE_BLUE_REMOVE_UNIT) {
            battle_script_blue_remove_unit(first_halfword);
        } else if (opcode == EVENT_OPCODE_END || opcode == EVENT_OPCODE_END_2) {
            battle_map_wait_for_refresh();
            do {
                battle_thread_yield();
            } while (battle_thread_find_running_by_task(NATIVE_THREAD_TASK_EVENT_BLOCK) != 0);
            do {
                battle_thread_yield();
            } while (main_file_is_still_loading() != 0);
            battle_thread_yield();
            if (battle_script_is_tutorial_event_slot() != 0) {
                battle_script_switch_tutorial_thread_for_eventend();
                g_main_game_options.value = g_battle_saved_game_options;
                battle_script_set_variable(EVENT_SCRIPT_VAR_SUPPRESS_PROGRESS_EFFECTS, 0);
                battle_state_start_close_battle(0x20);
            }
            if (g_battle_menu_input_disabled == 0) {
                for (finish_task_id = 2; finish_task_id < 0xE; finish_task_id++) {
                    battle_thread_wait_until_inactive(finish_task_id);
                }
            }
            operand_1 = battle_script_get_variable(EVENT_SCRIPT_VAR_CURRENT_EVENT);
            if (operand_1 != 0x1AD && operand_1 != 0x1D1) {
                battle_gfx_init_evtchr_vram_slots();
            }
            battle_script_set_variable(EVENT_SCRIPT_VAR_PENDING_STAGED_STATUS, 0);
            finish_operation = ((g_scenario_event_finish_operations[operand_1] & ~0x0CFF) >> 8);
            if (g_battle_menu_input_disabled == 0 && finish_operation == 0 && (u32)(operand_1 - 0x190) >= 0x1AU
                && operand_1 != 0) {
                g_battle_script_condition_unit_battle_id = 0xFF;
                battle_script_run_scenario_conditions();
                if (battle_script_load_next_event() != 0) {
                    goto restart_script;
                }
            }
            facing_misc_id = 0;
            battle_unit_snap_all_facings_to_quadrants();
            battle_unit_update_staged_status_data(0, 1);
            do {
                if (battle_unit_has_misc_id(facing_misc_id) != 0) {
                    operand_1 = battle_unit_get_battle_index_by_misc_id(facing_misc_id);
                    if (operand_1 >= 0) {
                        parameters = (u8*)battle_unit_get_stats_from_battle_id(operand_1);
                        ((battle_stats_t*)parameters)->position.raw
                            = (u16)((((battle_stats_t*)parameters)->position.raw & 0xF0FF)
                                | ((battle_unit_get_facing_quadrant_by_misc_id(facing_misc_id) & 0xF) << 8));
                    }
                }
                facing_misc_id += 1;
            } while (facing_misc_id < EVENT_UNIT_SLOT_COUNT);
            battle_camera_normalize_yaw_angle();
            battle_camera_init_tilt_and_zoom();
            battle_thread_exit_current();
        }
        /* ---- Flow control: forward and backward jumps ---- */
        else if (opcode == EVENT_OPCODE_UNKNOWN_DA) {
        } else if (opcode == EVENT_OPCODE_JUMP_FORWARD_IF_ZERO) {
            if (g_battle_script_variables[EVENT_SCRIPT_VAR_COMPARISON_RESULT] == 0) {
                instruction_offset = battle_script_find_jump_target(
                    instruction_offset + 2, EVENT_OPCODE_FORWARD_TARGET, EVENT_OPCODE_UNKNOWN_D4, operand_1);
                continue;
            }
        } else if (opcode == EVENT_OPCODE_JUMP_FORWARD) {
            instruction_offset
                = battle_script_find_jump_target(instruction_offset + 2, EVENT_OPCODE_FORWARD_TARGET, -1, operand_1);
            continue;
        } else if (opcode == EVENT_OPCODE_FORWARD_TARGET) {
        } else if (opcode == EVENT_OPCODE_UNKNOWN_D4) {
            break;
        } else if (opcode == EVENT_OPCODE_JUMP_BACK) {
            instruction_offset
                = battle_script_find_jump_target(instruction_offset, EVENT_OPCODE_BACK_TARGET, -1, operand_1);
            continue;
        } else if (opcode == EVENT_OPCODE_BACK_TARGET) {
        } else if (opcode == EVENT_OPCODE_UNKNOWN_D8) {
        } else if (opcode == EVENT_OPCODE_UNKNOWN_D9) {
        } else if (opcode == EVENT_OPCODE_UNKNOWN_DC) {
            /* The target stores only the low halfword of $2F. */
            *(s16*)&g_battle_text_substitution_values[0x2f] = 0xD;
        }
        /* ---- Dialogue and portraits ---- */
        else if (opcode == EVENT_OPCODE_DISPLAY_MESSAGE) {
            operand_1 = battle_thread_resolve_id(operand_1);
            battle_thread_start(operand_1, battle_text_character_handling_thread);
            message_thread_offset = operand_1 << 0xA;
            battle_text_build_display_message(instruction_offset, operand_2,
                &g_battle_thread_contexts[(u32)message_thread_offset >> 10].task_words[6]);
            g_battle_thread_contexts[((u32)message_thread_offset >> 10)].function_parameter_1 = (s32)operand_2;
            g_battle_thread_contexts[((u32)message_thread_offset >> 10)].function_parameter_2
                = (s32)(((operand_4 << 8) + operand_3) - 1);
            message_instruction = (u8*)g_battle_event_block;
            message_instruction += instruction_offset;
            operand_4 = message_instruction[6];
            operand_3 = message_instruction[5];
            g_battle_thread_contexts[((u32)message_thread_offset >> 10)].function_parameter_3
                = (s32)((operand_4 << 8) + operand_3);
            g_battle_thread_contexts[((u32)message_thread_offset >> 10)].function_parameter_4
                = (s32)message_instruction[7];
            g_battle_thread_contexts[((u32)message_thread_offset >> 10)].task_words[2]
                = (s32)battle_script_load_halfword(message_instruction + 8);
            g_battle_thread_contexts[((u32)message_thread_offset >> 10)].task_words[3]
                = (s32)battle_script_load_halfword(instruction_offset + ((u8*)g_battle_event_block) + 0xA);
            g_battle_thread_contexts[((u32)message_thread_offset >> 10)].task_words[4]
                = (s32)battle_script_load_halfword(instruction_offset + ((u8*)g_battle_event_block) + 0xC);
            message_cursor = (u8*)g_battle_event_block;
            message_cursor += instruction_offset;
            g_battle_thread_contexts[((u32)message_thread_offset >> 10)].task_words[5] = message_cursor[14];
            battle_thread_yield();
        } else if (opcode == EVENT_OPCODE_CHANGE_DIALOG) {
            for (work_index = 0; work_index < 6; work_index += 2) {
                operand_2 = g_battle_menu_window_buffers[work_index].thread_id;
                if ((g_battle_menu_window_buffers[work_index].dialogue_selector == operand_1)
                    && (battle_thread_is_running_8014cc94(operand_2) == 1)
                    && (existing_message_thread_offset = operand_2 << 0xA,
                        g_battle_thread_contexts[((u32)existing_message_thread_offset >> 10)].task_id
                            == NATIVE_THREAD_TASK_DIALOG_AWAIT_TEXT)) {
                    operand_1 = battle_script_load_halfword(instruction_offset + ((u8*)g_battle_event_block) + 2);
                    if ((operand_1 & 0xFFFF) != 0xFFFF) {
                        operand_1 -= 1;
                    }
                    g_battle_thread_contexts[((u32)existing_message_thread_offset >> 10)].function_parameter_2
                        = operand_1;
                    g_battle_thread_contexts[((u32)existing_message_thread_offset >> 10)].function_parameter_4
                        = (s32)battle_script_load_halfword(instruction_offset + ((u8*)g_battle_event_block) + 4);
                    g_battle_thread_contexts[((u32)existing_message_thread_offset >> 10)].task_id
                        = NATIVE_THREAD_TASK_RESUME;
                    break;
                }
            }
        } else if (opcode == EVENT_OPCODE_PORTRAIT_COL) {
            battle_script_load_portrait_colors_event_instruction(operand_1);
        }
        /* ---- Map objects ---- */
        else if (opcode == EVENT_OPCODE_USE_3D_OBJECT) {
            g_battle_3d_object_use_request = 1;
            g_battle_event_map_command_80_arg1 = (s16)operand_1;
            g_battle_event_map_command_80_arg2 = (s16)operand_2;
            battle_thread_yield();
        } else if (opcode == EVENT_OPCODE_WAIT_3D_OBJECT) {
            g_battle_3d_object_wait_status = 1;
            do {
                battle_thread_yield();
            } while (g_battle_3d_object_wait_status != 0);
        } else if (opcode == EVENT_OPCODE_USE_FIELD_OBJECT) {
            g_battle_field_object_use_request = 1;
            g_battle_event_map_command_83_arg1 = (s16)operand_1;
            g_battle_event_map_command_83_arg2 = (s16)operand_2;
            battle_thread_yield();
        } else if (opcode == EVENT_OPCODE_WAIT_FIELD_OBJECT) {
            g_battle_field_object_wait_status = 1;
            do {
                battle_thread_yield();
            } while (g_battle_field_object_wait_status != 0);
        }
        /* ---- unit_t sprites and animation ---- */
        else if (opcode == EVENT_OPCODE_MIRROR_SPRITE) {
            battle_script_mirrorsprite_event_instruction(parameters);
        } else if (opcode == EVENT_OPCODE_RESET_PALETTE) {
            battle_script_resetpalette_event_instruction(parameters);
        } else if (opcode == EVENT_OPCODE_SET_FIRST_MISC_UNIT) {
            operand_1 = battle_get_misc_id(first_halfword);
            if (operand_1 != EVENT_MISC_ID_NONE) {
                battle_unit_move_misc_unit_to_head(operand_1);
            }
        } else if (opcode == EVENT_OPCODE_UNIT_ANIM) {
            battle_script_unit_anim(parameters);
        } else if (opcode == EVENT_OPCODE_MARCH) {
            battle_script_march_units(parameters);
        } else if (opcode == EVENT_OPCODE_UNKNOWN_81) {
            battle_script_set_units_movement_effect_suppression(parameters);
        } else if (opcode == EVENT_OPCODE_UNKNOWN_12) {
            operand_1 = battle_get_misc_id(first_halfword);
            if (operand_1 != EVENT_MISC_ID_NONE) {
                battle_script_wait_for_unit_ready(operand_1);
            }
        }
        /* ---- Map state, weather, and time of day ---- */
        else if (opcode == EVENT_OPCODE_CHANGE_MAP_BETA) {
            g_battle_event_pending_map_jump_out_id = first_halfword;
            battle_thread_wait_frames(2);
            battle_camera_reset_script_transform();
        } else if (opcode == EVENT_OPCODE_CHANGE_MAP) {
            g_battle_event_pending_map_jump_out_2_id = first_halfword;
            battle_thread_wait_frames(2);
            battle_camera_reset_script_transform();
        } else if (opcode == EVENT_OPCODE_SET_DAYTIME) {
            g_battle_event_pending_map_state = (s16)operand_1;
            battle_thread_wait_frames(2);
            battle_map_wait_for_refresh();
        } else if (opcode == EVENT_OPCODE_WEATHER) {
            g_battle_event_weather_request = first_halfword;
            battle_thread_wait_frames(2);
            battle_map_wait_for_refresh();
        } else if (opcode == EVENT_OPCODE_RELOAD_MAP_STATE) {
            g_battle_map_reload_state = 1;
            do {
                battle_thread_yield();
            } while (g_battle_map_reload_state != 0);
        } else if (opcode == EVENT_OPCODE_APPEND_MAP_STATE) {
            g_battle_map_append_state = 1;
            do {
                battle_thread_yield();
            } while (g_battle_map_append_state != 0);
        } else if (opcode == EVENT_OPCODE_PAUSE) {
            battle_script_pause_event_instruction();
        } else if (opcode == EVENT_OPCODE_UNKNOWN_26) {
            for (work_index = 1; work_index < 16; work_index++) {
                cancel_thread = &g_battle_thread_contexts[work_index];
                if (cancel_thread->is_running != 0 && cancel_thread->task_id == NATIVE_THREAD_TASK_WAIT_FOR_RESUME
                    && cancel_thread->function_parameter_2 == first_halfword) {
                    cancel_thread->task_id = NATIVE_THREAD_TASK_RESUME;
                }
            }
        } else if (opcode == EVENT_OPCODE_UNKNOWN_37) {
            battle_script_toggle_message_portrait_flip(parameters);
        } else if (opcode == EVENT_OPCODE_UNKNOWN_17) {
        }
        /* ---- Effects ---- */
        else if (opcode == EVENT_OPCODE_EFFECT) {
            g_battle_event_pending_effect_id = first_halfword;
            effect_cursor = (u8*)g_battle_event_block;
            effect_cursor += instruction_offset;
            g_battle_event_effect_target_misc_id = battle_get_misc_id((s16)effect_cursor[3]);
            effect_instruction = (u8*)g_battle_event_block;
            effect_instruction += instruction_offset;
            g_battle_event_effect_target_x = (s16)effect_instruction[4];
            g_battle_event_effect_target_y = (s16)effect_instruction[5];
            g_battle_event_effect_target_mode = (s16)effect_instruction[6];
            battle_script_set_variable(EVENT_SCRIPT_VAR_CAMERA_ROTATION, 0);
            battle_thread_wait_frames(2);
        } else if (opcode == EVENT_OPCODE_EFFECT_START) {
            do {
                battle_thread_yield();
                g_battle_thread_call_target = (void (*)(void))battle_effect_try_init_data;
            } while (battle_thread_call_on_main_stack() != 0);
        } else if (opcode == EVENT_OPCODE_EFFECT_END) {
            battle_thread_start(battle_thread_resolve_id(0x10U), &battle_script_play_effect_thread);
        }
        /* ---- Camera ---- */
        else if (opcode == EVENT_OPCODE_FOCUS) {
            battle_camera_store_yaw_band(g_battle_script_variables[EVENT_SCRIPT_VAR_CAMERA_YAW]);
            battle_script_focus(parameters);
        } else if (opcode == EVENT_OPCODE_FOCUS_SPEED) {
            script_variables = g_battle_script_variables;
            battle_script_focus_speed(parameters, &script_variables[EVENT_SCRIPT_VAR_CAMERA_X],
                &script_variables[EVENT_SCRIPT_VAR_CAMERA_ANGLE]);
        } else if (opcode == EVENT_OPCODE_UNKNOWN_73) {
            battle_script_apply_relative_camera(parameters, &g_battle_script_variables[EVENT_SCRIPT_VAR_CAMERA_X]);
        } else if (opcode == EVENT_OPCODE_CAMERA) {
            operand_1 = battle_thread_resolve_id(0x10U);
            battle_thread_start(operand_1, &battle_camera_thread);
            g_battle_thread_contexts[operand_1].function_parameter_1 = (s32)parameters;
        } else if (opcode == EVENT_OPCODE_CAMERA_FUSION_START) {
            operand_1 = battle_thread_resolve_id(0x10U);
            battle_thread_start(operand_1, battle_camera_fusion_thread);
            g_battle_thread_contexts[operand_1].function_parameter_1 = (s32)parameters;
            instruction_offset
                = battle_script_find_instruction_byte_offset(instruction_offset, EVENT_OPCODE_CAMERA_FUSION_END);
        } else if (opcode == EVENT_OPCODE_CAMERA_FUSION_END) {
        } else if (opcode == EVENT_OPCODE_CAMERA_SPEED_CURVE) {
            g_battle_camera_speed_curve = (s32)operand_1;
        } else if (opcode == EVENT_OPCODE_UNKNOWN_74) {
            battle_noop_80149be4(parameters);
        }
        /* ---- unit_t movement and facing ---- */
        else if (opcode == EVENT_OPCODE_WALK_TO) {
            g_battle_thread_call_target = (void (*)(void))battle_script_walk_to_thread;
            battle_thread_call_on_main_stack(parameters);
        } else if (opcode == EVENT_OPCODE_JUMP) {
            battle_script_jump_event_instruction((battle_script_jump_instruction_t*)parameters);
        } else if (opcode == EVENT_OPCODE_WAIT_WALK) {
            battle_script_waitwalk_event_instruction(first_halfword);
        } else if (opcode == EVENT_OPCODE_WALK_TO_ANIM) {
            operand_1 = battle_get_misc_id(first_halfword);
            if (operand_1 != EVENT_MISC_ID_NONE) {
                g_battle_unit_pending_animation_ids[operand_1]
                    = battle_script_load_halfword(instruction_offset + ((u8*)g_battle_event_block) + 3);
                g_battle_unit_was_moving_latch[operand_1] = 0;
            }
        } else if (opcode == EVENT_OPCODE_WAIT_ALL_UNIT_MOVEMENT) {
            do {
                battle_thread_yield();
            } while (battle_script_check_unit_moving_event_instruction(-1) != 0);
        } else if (opcode == EVENT_OPCODE_WAIT_FILE_LOAD) {
            do {
                battle_thread_yield();
            } while (main_file_is_still_loading() != 0);
        } else if (opcode == EVENT_OPCODE_ROTATE_UNIT) {
            battle_rotate_unit(parameters);
        } else if (opcode == EVENT_OPCODE_FACE_UNIT_2) {
            battle_script_face_unit(parameters, 0);
        } else if (opcode == EVENT_OPCODE_FACE_UNIT) {
            battle_script_face_unit(parameters, 1);
        } else if (opcode == EVENT_OPCODE_FACE_TILE) {
            battle_script_face_tile(parameters);
        } else if (opcode == EVENT_OPCODE_WAIT_ROTATE_UNIT) {
            battle_script_waitrotateunit_and_waitrotateall_event_instruction(first_halfword);
        } else if (opcode == EVENT_OPCODE_WAIT_ROTATE_ALL) {
            battle_script_waitrotateunit_and_waitrotateall_event_instruction(-1);
        }
        /* ---- Blocks, earthquakes, darkness, and screen color ---- */
        else if (opcode == EVENT_OPCODE_BLOCK_START) {
            operand_1 = battle_thread_resolve_id(0x10U);
            battle_thread_start(operand_1, battle_block_start_thread);
            g_battle_thread_contexts[operand_1].function_parameter_1 = (s32)parameters;
            instruction_offset = battle_script_find_instruction_byte_offset(instruction_offset, EVENT_OPCODE_BLOCK_END);
        } else if (opcode == EVENT_OPCODE_BLOCK_END) {
        } else if (opcode == EVENT_OPCODE_UNKNOWN_23) {
        } else if (opcode == EVENT_OPCODE_UNKNOWN_24) {
        } else if (opcode == EVENT_OPCODE_EARTHQUAKE_END) {
            battle_thread_wait_frames(1);
            battle_thread_suspend(battle_thread_find_running_by_task(NATIVE_THREAD_TASK_EARTHQUAKE));
        } else if (opcode == EVENT_OPCODE_UNKNOWN_25) {
            g_battle_text_speaker_swap_unit_a = first_halfword;
            g_battle_text_speaker_swap_unit_b
                = battle_script_load_halfword(instruction_offset + ((u8*)g_battle_event_block) + 3);
        } else if (opcode == EVENT_OPCODE_MAP_DARKNESS) {
            operand_1 = battle_thread_resolve_id(0x10U);
            battle_thread_start(operand_1, &battle_script_map_darkness);
            g_battle_thread_contexts[operand_1].function_parameter_1 = (s32)parameters;
        } else if (opcode == EVENT_OPCODE_COLOR_SCREEN) {
            operand_1 = battle_thread_resolve_id(0x10U);
            battle_thread_start(operand_1, &battle_script_color_screen_thread);
            g_battle_thread_contexts[operand_1].function_parameter_1 = (s32)parameters;
        } else if (opcode == EVENT_OPCODE_EARTHQUAKE_START) {
            operand_1 = battle_thread_resolve_id(0x10U);
            battle_thread_start(operand_1, &battle_script_earthquake_start);
            g_battle_thread_contexts[operand_1].function_parameter_1 = (s32)parameters;
        } else if ((opcode == EVENT_OPCODE_SPRITE_MOVE) || (opcode == EVENT_OPCODE_SPRITE_MOVE_BETA)) {
            operand_1 = battle_thread_resolve_id_after_current(0x10);
            if (opcode == EVENT_OPCODE_SPRITE_MOVE) {
                battle_thread_start(operand_1, &battle_script_sprite_move);
            } else {
                battle_thread_start(operand_1, &battle_script_sprite_move_beta);
            }
            sprite_thread_offset = operand_1 << 0xA;
            g_battle_thread_contexts[((u32)sprite_thread_offset >> 10)].function_parameter_1 = (s32)parameters;
            g_battle_thread_contexts[((u32)sprite_thread_offset >> 10)].task_id = NATIVE_THREAD_TASK_SPRITE_MOVE;
            g_battle_thread_contexts[((u32)sprite_thread_offset >> 10)].task_words[0]
                = battle_get_misc_id(first_halfword);
        } else if (opcode == EVENT_OPCODE_WAIT_SPRITE_MOVE) {
            battle_script_waitspritemove_event_instruction(first_halfword);
        } else if (opcode == EVENT_OPCODE_MAP_LIGHT) {
            operand_1 = battle_thread_resolve_id(0x10U);
            battle_thread_start(operand_1, &battle_map_light_thread);
            g_battle_thread_contexts[operand_1].function_parameter_1 = (s32)parameters;
        }
        /* ---- Background, palettes, and terrain ---- */
        else if (opcode == EVENT_OPCODE_BACKGROUND) {
            battle_copy_bytes(&g_battle_event_background_colors.first, parameters, 3);
            battle_copy_bytes(&g_battle_event_background_colors.second, parameters + 3, 3);
            if (parameters[7] == 0) {
                battle_map_transition_and_store_background_gradient(parameters[6], &g_battle_event_background_colors);
            } else {
                battle_map_set_background_gradient(parameters[6], &g_battle_event_background_colors);
            }
        } else if (opcode == EVENT_OPCODE_COLOR_BG_BETA) {
            battle_map_modify_background_gradient(
                parameters[0], parameters[4], ((s8)parameters[1]), ((s8)parameters[2]), (s32)((s8)parameters[3]));
        } else if (opcode == EVENT_OPCODE_COLOR_UNIT) {
            battle_script_color_unit_event_instruction(parameters);
        } else if (opcode == EVENT_OPCODE_UNKNOWN_6C) {
            battle_script_set_units_palette_update_suppression(first_halfword & 0xFFFF, 0);
        } else if (opcode == EVENT_OPCODE_UNKNOWN_6D) {
            battle_script_set_units_palette_update_suppression(first_halfword & 0xFFFF, 1);
        } else if (opcode == EVENT_OPCODE_COLOR_FIELD) {
            battle_map_color_field(
                parameters[0], parameters[4], ((s8)parameters[1]), ((s8)parameters[2]), (s32)((s8)parameters[3]));
        } else if (opcode == EVENT_OPCODE_UNKNOWN_66) {
            battle_map_store_modified_palette_colors(0, 0, 1);
        } else if (opcode == EVENT_OPCODE_SET_TILE_CAMERA_BLOCK) {
            battle_map_set_tile_data_value(MAP_TILE_DATA_FLAT_CAMERA_BLOCK_MASK, parameters[0], (u8)((s8)parameters[1]),
                (u8)((s8)parameters[2]), (s32)(u8)((s8)parameters[3]));
        } else if (opcode == EVENT_OPCODE_UNKNOWN_40) {
            tile = battle_map_get_tile_data_pointer(parameters[0], (u8)((s8)parameters[1]), (u8)((s8)parameters[2]));
            if ((u32)(tile->surface.bits.type - MAP_SURFACE_GRASSLAND) >= 2U) {
                tile->surface.value = (u8)((tile->surface.value & ~MAP_SURFACE_MASK) | MAP_SURFACE_STONE_FLOOR);
            }
            tile->height = (u8)((s8)parameters[3]);
            tile->flags_06.bits.untargetable = parameters[4];
            tile->flags_06.bits.blocked = 0;
        }
        /* ---- Event speed, dark screen, conditions, and graphics ---- */
        else if (opcode == EVENT_OPCODE_EVENT_SPEED) {
            battle_script_set_event_speed(operand_1);
        } else if (opcode == EVENT_OPCODE_DARK_SCREEN) {
            g_battle_menu_hide_numeric_values = 0;
            operand_1 = battle_thread_resolve_id(0x10U);
            battle_thread_start(operand_1, &battle_menu_open_companion_executable_6);
            battle_thread_set_parameters(operand_1, 0, (s32)parameters, 0);
        } else if (opcode == EVENT_OPCODE_REMOVE_DARK_SCREEN) {
            operand_1 = battle_thread_find_running_by_task(NATIVE_THREAD_TASK_DARK_SCREEN_HOLD);
            if (operand_1 != 0) {
                g_battle_thread_contexts[operand_1].task_id = NATIVE_THREAD_TASK_DARK_SCREEN;
            }
        } else if (opcode == EVENT_OPCODE_DISPLAY_CONDITIONS) {
            operand_3 = battle_thread_resolve_id(0x10U);
            if ((operand_3 == 4) && (operand_1 == 7)) {
                battle_thread_wait_until_inactive(2);
                operand_3 = 2;
            }
            battle_thread_start(operand_3, &battle_script_execute_display_conditions_instruction);
            battle_thread_set_parameters(operand_3, operand_1, operand_2, 0);
        } else if (opcode == EVENT_OPCODE_SHOW_GRAPHIC) {
            /* Use: the extra operand_2 reference keeps operand_2/operand_3 in $s5/$s6, not swapped. */
            __asm__("" : : "r"(operand_2));
            operand_3 = battle_thread_resolve_id(0x10U);
            battle_thread_start(operand_3, &battle_script_show_graphic);
            battle_thread_set_parameters(operand_3, operand_1, 0, 0);
        }
        /* ---- Sound and music ---- */
        else if (opcode == EVENT_OPCODE_SOUND_EFFECT) {
            g_sound_effect_id_to_play = (s32)first_halfword;
        } else if (opcode == EVENT_OPCODE_BG_SOUND) {
            operand_1 = battle_thread_resolve_id(0x10U);
            battle_thread_start(operand_1, &battle_sound_bg_thread);
            battle_thread_set_parameters(operand_1, (s32)parameters, 0, 0);
        } else if (opcode == EVENT_OPCODE_EDIT_BG_SOUND) {
            operand_1 = battle_thread_resolve_id(0x10U);
            battle_thread_start(operand_1, &battle_sound_edit_bg_thread);
            battle_thread_set_parameters(operand_1, (s32)parameters, 0, 0);
        } else if (opcode == EVENT_OPCODE_SWITCH_TRACK) {
            g_battle_music_switch_request = (s16)operand_1;
            g_battle_event_music_switch_volume = (s16)operand_2;
            g_battle_event_music_switch_fade = (s16)operand_3;
            battle_thread_yield();
        } else if (opcode == EVENT_OPCODE_FADE_SOUND) {
            /* Matching limitation: target sllv masks its count to five bits.
             * An explicit &31 adds an instruction with GCC 2.6.3, so counts
             * above 31 remain undefined in C; no script restriction is proven. */
            g_battle_music_volume_transition_request = (u32)operand_2 << (operand_1 + 0x10);
            battle_thread_yield();
        } else if (opcode == EVENT_OPCODE_END_TRACK) {
            g_battle_music_unload_slot_request = (s16)operand_1;
            battle_thread_yield();
        }
        /* ---- EVTCHR and reserved VRAM slots ---- */
        else if (opcode == EVENT_OPCODE_LOAD_EVTCHR) {
            operand_2 = battle_thread_resolve_id(0x10U);
            battle_thread_start(operand_2, battle_gfx_load_evtchr_thread);
            battle_thread_set_parameters(operand_2, operand_1,
                battle_script_load_halfword(instruction_offset + ((u8*)g_battle_event_block) + 2), 0);
            battle_thread_yield();
        } else if (opcode == EVENT_OPCODE_LOAD_EVTCHR_CLEAR) {
            g_battle_event_pending_loaded_evtchr_clear_slot = (s32)operand_1;
            battle_thread_yield();
        } else if (opcode == EVENT_OPCODE_SAVE_EVTCHR) {
            g_battle_event_pending_evtchr_save_slot = (s32)operand_1;
            battle_thread_yield();
        } else if (opcode == EVENT_OPCODE_SAVE_EVTCHR_CLEAR) {
            g_battle_event_pending_saved_evtchr_clear_slot = (s32)operand_1;
            battle_thread_yield();
        } else if (opcode == EVENT_OPCODE_COPY_UNIT_GRAPHICS_TO_RESERVED_VRAM_SLOT) {
            g_battle_event_pending_unit_vram_copy = ((u32)battle_get_misc_id(first_halfword) << 8) | (u32)operand_3;
            battle_thread_yield();
        } else if (opcode == EVENT_OPCODE_RELEASE_RESERVED_VRAM_SLOT) {
            g_battle_event_pending_reserved_vram_release_slot = (s32)operand_1;
            battle_thread_yield();
        } else if (opcode == EVENT_OPCODE_UNKNOWN_E0) {
        }
        /* ---- Adding and removing units ---- */
        else if (opcode == EVENT_OPCODE_REMOVE_UNIT) {
            remove_unit_misc_id = (s16)battle_get_misc_id(first_halfword);
            if (remove_unit_misc_id != EVENT_MISC_ID_NONE) {
                work_index = remove_unit_misc_id;
                while (battle_script_check_unit_moving_event_instruction(work_index) != 0) {
                    battle_thread_yield();
                }
                if (work_index != EVENT_MISC_ID_NONE) {
                    operand_1 = battle_unit_get_battle_index_by_misc_id(work_index);
                    battle_unit_remove_misc_by_id(work_index);
                    if (operand_1 >= 0) {
                        battle_unit_set_cannot_exist(operand_1);
                    }
                }
            }
        } else if (opcode == EVENT_OPCODE_DRAW) {
            draw_misc_id = (s16)battle_get_misc_id(first_halfword);
            if (draw_misc_id != EVENT_MISC_ID_NONE) {
                battle_unit_animate_and_set_enemy_level_data_by_misc_id(draw_misc_id);
                if (battle_script_get_variable(EVENT_SCRIPT_VAR_CURRENT_EVENT) == 0x18C) {
                    battle_status_queue_current_status_graphics(
                        battle_unit_get_battle_index_by_misc_id(draw_misc_id), 1);
                    g_battle_thread_call_target = (void (*)(void))battle_unit_update_graphics_by_misc_id_wrapper;
                    battle_thread_call_on_main_stack((void*)draw_misc_id);
                }
            }
        } else if (opcode == EVENT_OPCODE_ERASE) {
            reset_graphic_misc_id = (s16)battle_get_misc_id(first_halfword);
            if (reset_graphic_misc_id != EVENT_MISC_ID_NONE) {
                battle_gfx_reset_unit_graphic_trigger(reset_graphic_misc_id);
            }
        } else if (opcode == EVENT_OPCODE_ADD_GHOST_UNIT) {
            ghost_parameters = instruction_offset + ((u8*)g_battle_event_block) + 3;
            ghost_misc_id = 3;
            do {
                ghost_misc_id -= 1;
            } while (ghost_misc_id >= 0);
            for (ghost_misc_id = 0; ghost_misc_id < EVENT_UNIT_SLOT_COUNT; ghost_misc_id++) {
                if (battle_unit_has_misc_id(ghost_misc_id) == 0) {
                    g_battle_event_unit_slots[ghost_parameters[0]] = ghost_misc_id;
                    break;
                }
            }
            if (ghost_misc_id != EVENT_UNIT_SLOT_COUNT) {
                battle_script_add_ghost_unit_event_instruction(ghost_parameters[1], ghost_parameters[2],
                    ghost_parameters[3], ghost_parameters[4], (s32)first_halfword,
                    (s32)g_battle_event_unit_slots[ghost_parameters[0]], (s32)ghost_parameters[5]);
                g_battle_unit_graphics_load_pending = 1U;
            } else {
                continue;
            }
        } else if (opcode == EVENT_OPCODE_UNKNOWN_75) {
        } else if (opcode == EVENT_OPCODE_ADD_UNIT) {
            battle_noop_80143bd0();
            added_unit = battle_find_unit_data_pointer_for_entd_unit_id(first_halfword, &added_unit_state);
            if ((added_unit_state == -2) || (added_unit_state >= 0)) {
                for (added_battle_id = 0; added_battle_id < EVENT_UNIT_SLOT_COUNT; added_battle_id++) {
                    if (battle_unit_get_stats_from_battle_id(added_battle_id) == added_unit)
                        break;
                }
                operand_1 = battle_unit_get_misc_id_by_battle_id(added_battle_id);
                if (operand_1 == -1) {
                    battle_update_unit_status_and_staged_status_data(added_battle_id);
                    battle_gfx_load_unit_graphics_by_battle_id(added_battle_id, operand_3);
                    g_battle_unit_graphics_load_pending = 1U;
                } else {
                    if (operand_3 == 0) {
                        battle_unit_animate_and_set_enemy_level_data_by_misc_id(operand_1);
                    }
                    if (battle_update_unit_status_and_staged_status_data(added_battle_id) != 0) {
                        g_battle_thread_call_target = (void (*)(void))battle_unit_update_graphics_by_misc_id_wrapper;
                        battle_thread_call_on_main_stack(operand_1);
                    }
                }
            }
        } else if (opcode == EVENT_OPCODE_WARP_UNIT) {
            battle_script_warp_unit(parameters);
        } else if (opcode == EVENT_OPCODE_UNKNOWN_61) {
        } else if (opcode == EVENT_OPCODE_SET_BATTLE_UNIT_POSITION) {
            /* Direct simulation slot, not the encoded identity used by WarpUnit.
             * Operand byte 1 is unused. The target neither guards slots >=21
             * nor synchronizes the renderer's position here. */
            parameters = (u8*)battle_unit_get_stats_from_battle_id((s32)operand_1);
            position_instruction = instruction_offset + ((u8*)g_battle_event_block);
            ((battle_stats_t*)parameters)->x = (u8)position_instruction[3];
            ((battle_stats_t*)parameters)->position.bits.y = (u8)position_instruction[4];
            ((battle_stats_t*)parameters)->position.bits.higher_elevation = position_instruction[5];
            ((battle_stats_t*)parameters)->position.bits.facing = position_instruction[6];
        } else if (opcode == EVENT_OPCODE_WAIT_ADD_UNIT) {
            do {
                battle_thread_yield();
            } while (g_battle_unit_graphics_load_pending != 0);
        } else if (opcode == EVENT_OPCODE_WAIT_ADD_UNIT_END) {
            do {
                battle_thread_yield();
            } while (battle_thread_find_running_by_task(NATIVE_THREAD_TASK_ADD_UNIT) != 0);
        } else if (opcode == EVENT_OPCODE_ADD_UNIT_START) {
            operand_1 = battle_thread_resolve_id(0x10U);
            battle_thread_start(operand_1, battle_script_add_unit_start_thread);
            g_battle_thread_contexts[operand_1].function_parameter_1 = (s32)parameters;
            instruction_offset
                = battle_script_find_instruction_byte_offset(instruction_offset, EVENT_OPCODE_ADD_UNIT_END);
        } else if (opcode == EVENT_OPCODE_ADD_UNIT_END) {
        }
        /* ---- Waits and synchronization ---- */
        else if (opcode == EVENT_OPCODE_WAIT_FOR_INSTRUCTION) {
            /* Yield until no thread runs the named task. This stays a label
             * loop: as C loops the scan gains loop notes that change the
             * operand register allocation (see the WORLD twin). */
            operand_3 = (operand_2 << 8) + operand_1;
            parameters = (u8*)g_battle_thread_contexts;
        yield_before_task_scan:
            wait_thread_index = 1;
            battle_thread_yield();
        scan_task_slots:
            /* Matching: typed indexing reverses the target's addu operands.
             * Keep the address sum explicit; all record access remains typed. */
            wait_thread = (native_thread_t*)((u32)(wait_thread_index * sizeof(native_thread_t)) + (u32)parameters);
            if ((wait_thread->is_running == 0) || (wait_thread->task_id != operand_3)) {
                wait_thread_index += 1;
                if (wait_thread_index < 0x10) {
                    goto scan_task_slots;
                }
            }
            if (wait_thread_index != 0x10) {
                goto yield_before_task_scan;
            }
        } else if (opcode == EVENT_OPCODE_YIELD) {
            battle_thread_yield();
        } else if (opcode == EVENT_OPCODE_WAIT) {
            if (battle_script_is_tutorial_event_slot() != 0 && first_halfword >= 3) {
                battle_thread_wait_frames(first_halfword / battle_state_get_animation_speed());
            } else {
                battle_thread_wait_frames(first_halfword);
            }
        } else if (opcode == EVENT_OPCODE_TUTORIAL_BUTTON_WAIT) {
            do {
                battle_thread_yield();
            } while (!(first_halfword & g_battle_script_unfiltered_controller_input));
        } else if (opcode == EVENT_OPCODE_END_SOUND) {
            main_sound_stop_weather_sfx_music();
        }
        /* ---- Miscellaneous unit and display commands ---- */
        else if (opcode == EVENT_OPCODE_UNIT_SHADOW) {
            shadow_misc_id = battle_get_misc_id(first_halfword);
            if (shadow_misc_id != EVENT_MISC_ID_NONE) {
                if (operand_3 != 0) {
                    battle_gfx_clear_shadow_graphic_trigger_by_misc_id(shadow_misc_id);
                } else {
                    battle_gfx_set_shadow_graphic_trigger_by_misc_id(shadow_misc_id);
                }
            }
        } else if (opcode == EVENT_OPCODE_UNKNOWN_72) {
            battle_script_print_debug_message();
        } else if (opcode == EVENT_OPCODE_WAIT_VALUE) {
            battle_script_wait_value_event_instruction(parameters);
        } else if (opcode == EVENT_OPCODE_EVTCHR_PALETTE) {
            evtchr_palette_misc_id = battle_get_misc_id(first_halfword);
            if (evtchr_palette_misc_id != EVENT_MISC_ID_NONE) {
                battle_gfx_set_evtchr_palette_by_misc_id(evtchr_palette_misc_id, operand_3, operand_4);
            }
        } else if (opcode == EVENT_OPCODE_LOAD_ATTACK_GRAPHICS) {
            battle_script_load_attack_graphics_event_instruction();
        } else if (opcode == EVENT_OPCODE_CHANGE_STATS) {
            battle_script_change_stats(parameters);
        } else if (opcode == EVENT_OPCODE_PLAY_TUNE) {
            main_sound_play_tune(operand_1);
        } else if (opcode == EVENT_OPCODE_UNLOCK_DATE) {
            if ((battle_script_get_variable(EVENT_SCRIPT_VAR_SUPPRESS_PROGRESS_EFFECTS) == 0) && (operand_1 != 0)) {
                date_month = battle_script_get_variable(EVENT_SCRIPT_VAR_MONTH);
                battle_script_unlockdate_event_instruction((s32)g_main_treasure_acquisition_date_bits, operand_1 - 1,
                    date_month, battle_script_get_variable(EVENT_SCRIPT_VAR_DAY));
            }
        } else if (opcode == EVENT_OPCODE_TEMP_WEAPON) {
            thrown_item_misc_id = battle_get_misc_id(first_halfword);
            if (thrown_item_misc_id != EVENT_MISC_ID_NONE) {
                battle_gfx_set_thrown_item_palette_by_misc_id(operand_3, thrown_item_misc_id);
            }
        } else if (opcode == EVENT_OPCODE_ARROW) {
            arrow_source_misc_id = battle_get_misc_id(first_halfword);
            if (arrow_source_misc_id != EVENT_MISC_ID_NONE) {
                operand_1 = battle_unit_get_battle_index_by_misc_id(arrow_source_misc_id);
                arrow_target_misc_id = battle_get_misc_id(
                    battle_script_load_halfword(instruction_offset + ((u8*)g_battle_event_block) + 3));
                if (arrow_target_misc_id != EVENT_MISC_ID_NONE) {
                    arrow_target_battle_id = battle_unit_get_battle_index_by_misc_id(arrow_target_misc_id);
                    g_battle_thread_call_target = (void (*)(void))battle_effect_set_arrow_trajectory;
                    battle_thread_call_on_main_stack(arrow_target_battle_id, g_battle_scratch_coords, operand_1);
                }
            }
        } else if (opcode == EVENT_OPCODE_MAP_FREEZE) {
            battle_map_freeze();
        } else if (opcode == EVENT_OPCODE_MAP_UNFREEZE) {
            battle_map_unfreeze();
        } else if (opcode == EVENT_OPCODE_UNIT_ANIM_ROTATE) {
            battle_script_unit_animation_rotate_event_instruction(parameters);
        } else if (opcode == EVENT_OPCODE_WAIT_GRAPHIC_PRINT) {
            battle_thread_yield();
            do {
                battle_thread_yield();
                operand_1 = battle_thread_find_running_by_task(NATIVE_THREAD_TASK_SHOW_GRAPHIC);
            } while (operand_1 != 0 && g_battle_thread_contexts[operand_1].function_parameter_4 == 0);
        } else if (opcode == EVENT_OPCODE_LOCK_MENU_INPUT) {
            g_event_mode = (s32)operand_1;
        } else if (opcode == EVENT_OPCODE_TUTORIAL_COORDINATE_CONFIRM) {
            while (g_battle_cursor_x != operand_1 || g_battle_cursor_y != operand_2
                || ((confirm_buttons = g_battle_script_unfiltered_controller_input) & PSX_PAD_CIRCLE) == 0
                || (confirm_buttons & (PSX_PAD_UP | PSX_PAD_RIGHT | PSX_PAD_DOWN | PSX_PAD_LEFT)) != 0) {
                battle_thread_yield();
            }
        }
        /* ---- Map title, dates, teleports, and status ---- */
        else if (opcode == EVENT_OPCODE_SHOW_MAP_TITLE) {
            operand_4 = battle_thread_resolve_id(0x10U);
            battle_menu_request_open_companion_executable(0xD);
            attack_map_load_title_graphic();
            battle_thread_start(operand_4, attack_map_show_title);
            battle_thread_set_parameters(operand_4, (s8)operand_1, (s16)(s8)operand_2, (s8)operand_3);
            battle_thread_wait_until_inactive(operand_4);
        } else if (opcode == EVENT_OPCODE_SET_DATE_ADVANCE) {
            if (battle_script_get_variable(EVENT_SCRIPT_VAR_SUPPRESS_PROGRESS_EFFECTS) == 0) {
                battle_script_set_variable(EVENT_SCRIPT_VAR_DATE_ADVANCE, first_halfword);
            }
        } else if (opcode == EVENT_OPCODE_TELEPORT_OUT) {
            battle_script_teleportout_event_instruction(first_halfword, 1);
        } else if (opcode == EVENT_OPCODE_TELEPORT_IN) {
            battle_script_teleportin_event_instruction(first_halfword, 1);
        } else if (opcode == EVENT_OPCODE_INFLICT_STATUS) {
            operand_1 = battle_thread_resolve_id(0x10U);
            battle_thread_start(operand_1, &battle_script_inflict_status_thread);
            battle_thread_set_parameters(operand_1, (s32)parameters, 0, 0);
        } else if (opcode == EVENT_OPCODE_UNKNOWN_95) {
        } else {
            break;
        }
        instruction_offset += 1 + g_battle_script_event_instruction_sizes[opcode];
    }
    battle_thread_exit_current();
}
