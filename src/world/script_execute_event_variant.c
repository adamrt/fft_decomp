/* The event script interpreter that runs as the event thread. */

#include "fft/battle.h"
#include "fft/battle_effect.h"
#include "fft/battle_gfx.h"
#include "fft/battle_map.h"
#include "fft/battle_script.h"
#include "fft/battle_unit.h"
#include "fft/camera.h"
#include "fft/event_attack.h"
#include "fft/gfx.h"
#include "fft/main.h"
#include "fft/main_file.h"
#include "fft/main_item.h"
#include "fft/main_sound.h"
#include "fft/map.h"
#include "fft/memory.h"
#include "fft/menu.h"
#include "fft/menu_input.h"
#include "fft/menu_overlay.h"
#include "fft/menu_panel.h"
#include "fft/menu_text.h"
#include "fft/script.h"
#include "fft/sound.h"
#include "fft/status.h"
#include "fft/text.h"
#include "fft/thread.h"
#include "fft/unit.h"
#include "fft/world.h"
#include "fft/world_script.h"
#include "psx/pad.h"
#include "psx/types.h"

/* WORLD scenario interpreter, 0x800f6f20..0x800f92a0.
 *
 * One cooperative thread runs this loop: fetch the opcode and its first four
 * operand bytes, dispatch through the else-if chain below, then advance by the
 * opcode's encoded length. The chain order is the target's compare order and
 * must not be re-sorted. Handlers that seek (jumps) `continue` without the
 * length step; EventEnd may `goto restart_event` for a chained scenario.
 *
 * BATTLE carries the same interpreter (script_execute_event_variant); the two
 * are kept structurally identical so differences stay visible.
 *
 * The native inner-call trampoline intentionally forwards varying argument
 * counts through its () ABI. */
void script_execute_event_variant(void) {
    s32 added_unit_state;
    s32 ghost_runtime_id;
    s32 region_runtime_id;
    s16 draw_runtime_id;
    s32 date_month;
    s32 added_battle_id;
    s32 work_index;

    s16 first_operand;
    s32 removed_runtime_id;
    s16 erased_runtime_id;
    s16 shadow_runtime_id;
    s16 runtime_id_for_7f;
    s16 palette_runtime_id;
    s16 first_runtime_id;
    s16 second_runtime_id;
    unit_stats_t* appearance_unit;

    s32 initial_buttons;
    s32 event_flags;
    s32 instruction_offset;
    event_opcode_e opcode;
    u32 operand_2;
    u32 operand_3;
    s32 operand_4;
    s32 operand_1;
    event_set_battle_unit_position_instruction_t* position_instruction;
    u8* dialogue_cursor;
    event_display_message_instruction_t* message_instruction;
    s32* script_variables;
    u8* parameters;
    /* One unit pointer, one thread pointer and one index serve every handler;
     * separate ones per handler change the saved-register allocation. */
    unit_stats_t* unit;
    u8* instruction;
    u8* ghost_parameters;
    map_tile_t* tile;
    unit_stats_t* copy_destination;
    u8* effect_instruction;
    thread_t* thread;
    thread_t* threads;

    thread_set_current_task_id(THREAD_TASK_EXECUTE_EVENT);
    if (script_is_tutorial_event_slot() != 0) {
        gfx_ramp_screen_overlay_intensity_to_zero();
    }
    if (g_menu_input_mode == MENU_INPUT_PLAYER) {
        for (work_index = 2; work_index < 0xE; work_index++) {
            thread_wait_until_inactive(work_index);
        }
        camera_wait_until_idle();
    } else {
        g_menu_hovered_unit_stats_rect.y = 0xAA;
        g_menu_attack_caster_stats_rect.y = 0xAA;
        g_menu_attack_target_stats_rect.y = 0xAA;
        g_menu_right_unit_stats_rect.y = 0xAA;
    }
    unit_clear_status_staging_data();
    if (script_get_variable(EVENT_SCRIPT_VAR_BATTLE_NOT_STARTED) != 0) {
        unit_update_staged_status_data(0, 0);
    } else {
        script_process_inflict_status_commands();
    }
    script_set_variable(EVENT_SCRIPT_VAR_DISABLED_MENU_ACTIONS, 0);
    script_set_variable(EVENT_SCRIPT_VAR_DISABLED_CONTROLLER_INPUTS, 0);
    script_set_variable(EVENT_SCRIPT_VAR_FORCED_CONTROLLER_INPUTS, 0);
restart_script:
    script_set_event_speed(1U);
    instruction_offset = 0;
    camera_normalize_yaw_angle();
    {
        s32 empty_unit_id = EVENT_UNIT_SLOT_EMPTY;
        /* Keep the shared fill value available before the countdown initializer. */
        for (work_index = 3; work_index >= 0; work_index--) {
            g_script_unit_slots[work_index] = empty_unit_id;
        }
    }
    for (;;) {
        instruction = (u8*)g_script_event_block;
        instruction += instruction_offset;
        parameters = instruction + 1;
        operand_1 = instruction[1];
        operand_2 = instruction[2];
        operand_3 = instruction[3];
        operand_4 = instruction[4];
        opcode = instruction[0];
        first_operand = script_read_s16(parameters);
        /* ---- Control: NOPs, conditions, and script variable arithmetic ---- */
        if (opcode == EVENT_OPCODE_CAMERA_DISABLED) {
        } else if (opcode == EVENT_OPCODE_NOP) {
        } else if ((opcode - EVENT_CONDITION_SECOND_GTE_FIRST) < 6U) {
            script_run_condition(opcode);
        } else if ((opcode - EVENT_OPCODE_ADD_IMMEDIATE) < 0xFU) {
            operand_1 = (u16)first_operand;
            script_run_variable_command(
                opcode, operand_1, script_read_s16(instruction_offset + ((u8*)g_script_event_block) + 3) & 0xFFFF, 0);
        } else if (opcode == EVENT_OPCODE_REQUEST_MAP_JUMP_IN) {
            g_event_map_jump_in_request = operand_1;
            thread_wait_frames(2);
        }
        /* ---- Scenario progression: DismissUnit, CallFunction, BlueRemoveUnit, EventEnd ---- */
        else if (opcode == EVENT_OPCODE_DISMISS_UNIT) {
            if (script_get_variable(EVENT_SCRIPT_VAR_SUPPRESS_PROGRESS_EFFECTS) == 0) {
                script_opcode_dismiss_unit(first_operand);
            }
        } else if (opcode == EVENT_OPCODE_CALL_FUNCTION) {
            /* Sequential selector tests, not switch alternatives: selectors 6
             * and 14 reuse operand_1 as a loop index and later tests see the
             * changed value. That target-visible fallthrough is preserved. */
            thread_wait_frames(4);
            if (script_get_variable(EVENT_SCRIPT_VAR_SUPPRESS_PROGRESS_EFFECTS) == 0) {
                if (operand_1 == EVENT_CALL_FUNCTION_JOIN_UNITS_SILENTLY) {
                    if (script_get_variable(EVENT_SCRIPT_VAR_CURRENT_EVENT) < EVENT_ID_NEW_GAME_INVENTORY_END) {
                        main_item_init_new_game_inventory();
                    }
                    script_join_units_silently();
                }
                if (operand_1 == EVENT_CALL_FUNCTION_OPEN_SAVE_MENU) {
                    operand_1 = 2;
                    do {
                        thread_set_parameters(operand_1, 0U, 0, 1);
                        g_thread_contexts[operand_1].task_id = THREAD_TASK_STOP_REQUEST;
                        thread_wait_until_inactive(operand_1);
                        operand_1 += 1;
                    } while (operand_1 < 0xF);
                    if (script_get_variable(EVENT_SCRIPT_VAR_CURRENT_EVENT) != EVENT_ID_EPILOGUE) {
                        script_set_variable(EVENT_SCRIPT_VAR_NEXT_SCENARIO, 1);
                    }
                    script_set_variable(EVENT_SCRIPT_VAR_SAVE_IN_PROGRESS, 1);
                    menu_request_open_companion_executable(COMPANION_OVERLAY_CARD);
                    g_menu_companion_overlay_state = COMPANION_OVERLAY_STATE_CARD_SAVE;
                    do {
                        thread_yield();
                    } while (g_menu_companion_overlay_state != COMPANION_OVERLAY_STATE_IDLE);
                    script_set_variable(EVENT_SCRIPT_VAR_NEXT_SCENARIO, 0);
                    operand_1 = 2;
                    do {
                        thread_set_parameters(operand_1, 0U, 0, 1);
                        g_thread_contexts[operand_1].task_id = THREAD_TASK_STOP_REQUEST;
                        thread_wait_until_inactive(operand_1);
                        operand_1 += 1;
                    } while (operand_1 < 0xF);
                }
            }
            if ((operand_1 == EVENT_CALL_FUNCTION_ADVANCE_YEAR)
                || (operand_1 == EVENT_CALL_FUNCTION_ADVANCE_YEAR_ALTERNATE)) {
                script_advance_date_by_one_year();
            }
            if (operand_1 == EVENT_CALL_FUNCTION_INCREASE_BLUE_TEAM_BRAVE) {
                unit_increase_blue_team_brave_by_10();
            }
            if (operand_1 == EVENT_CALL_FUNCTION_REMOVE_ALL_ENEMY_UNITS) {
                script_blue_remove_all_enemy_units();
                thread_wait_frames(0x78);
            }
            if (operand_1 == EVENT_CALL_FUNCTION_REQUEST_CAMERA_YAW_WRAP) {
                g_camera_wrap_yaw_pending = 1;
            }
            if (operand_1 == EVENT_CALL_FUNCTION_DISABLE_DEBUG_DISPLAY) {
                g_main_debug_display_enabled = 0;
                g_main_unknown_800459d8 = 0;
            }
            if (operand_1 == EVENT_CALL_FUNCTION_MUTE_TEXT_AUDIO_CUE) {
                script_set_variable(EVENT_SCRIPT_VAR_MUTE_TEXT_AUDIO_CUE, 1);
            }
            if (operand_1 == EVENT_CALL_FUNCTION_UNMUTE_TEXT_AUDIO_CUE) {
                script_set_variable(EVENT_SCRIPT_VAR_MUTE_TEXT_AUDIO_CUE, 0);
            }
            if (operand_1 == EVENT_CALL_FUNCTION_ADVANCE_MAP_DESTRUCTION_AND_WAIT) {
                script_set_specialized_map_destroyed();
                if (g_map_destruction_wait_state != 0) {
                    do {
                        thread_yield();
                    } while (g_map_destruction_wait_state != 0);
                }
            }
            if (operand_1 == EVENT_CALL_FUNCTION_PLAY_ALTIMA_MUSIC) {
                g_world_sound_current_music_track_index = 0;
                g_sound_music_track_1_id = MAIN_MUSIC_ALTIMA;
                g_sound_music_track_2_id = 0;
                sound_play_music_tracks(MAIN_MUSIC_ALTIMA, MAIN_MUSIC_NONE);
            }
            if (operand_1 == EVENT_CALL_FUNCTION_COPY_UNIT_8_POSITION_TO_UNIT_9_AND_RELOAD_GRAPHICS) {
                unit = battle_unit_get_stats_from_battle_id(7);
                copy_destination = battle_unit_get_stats_from_battle_id(8);
                copy_destination->x = unit->x;
                copy_destination->position.y = unit->position.y;
                copy_destination->position.higher_elevation = unit->position.higher_elevation;
                battle_gfx_load_unit_graphics_by_battle_id(8, 1);
                g_unit_graphics_load_pending = 1;
                do {
                    thread_yield();
                } while (g_unit_graphics_load_pending != 0);
                thread_yield();
                thread_yield();
            }
            if (operand_1 == EVENT_CALL_FUNCTION_RESET_BETHLA_REGION_UNIT_GRAPHICS) {
                region_runtime_id = 0;
                do {
                    if (battle_unit_has_runtime_id(region_runtime_id) != 0) {
                        operand_1 = battle_unit_get_battle_id_by_runtime_id(region_runtime_id);
                        if (operand_1 != -1U) {
                            unit = battle_unit_get_stats_from_battle_id(operand_1);
                            if ((unit->x < 4U) && (unit->position.y < 0xBU)) {
                                battle_gfx_reset_unit_graphic_trigger(region_runtime_id);
                            }
                            if (((unit->x - 4) < 3U) && ((unit->position.y - 2) < 6U)) {
                                battle_gfx_reset_unit_graphic_trigger(region_runtime_id);
                            }
                        }
                    }
                    region_runtime_id += 1;
                } while (region_runtime_id < BATTLE_UNIT_SLOT_COUNT);
            }
            if (operand_1 == EVENT_CALL_FUNCTION_COPY_UNIT_2_POSITION_AND_FACING_TO_UNIT_8_AND_RELOAD_GRAPHICS) {
                unit = battle_unit_get_stats_from_battle_id(1);
                copy_destination = battle_unit_get_stats_from_battle_id(7);
                copy_destination->x = unit->x;
                copy_destination->position.y = unit->position.y;
                copy_destination->position.higher_elevation = unit->position.higher_elevation;
                copy_destination->position.facing = unit->position.facing;
                battle_gfx_load_unit_graphics_by_battle_id(7, 1);
                g_unit_graphics_load_pending = 1U;
                do {
                    thread_yield();
                } while (g_unit_graphics_load_pending != 0);
                thread_yield();
                thread_yield();
            }
            if ((operand_1 - EVENT_CALL_FUNCTION_WARP_UNIT_4_DISPLAY_TO_UNIT_2_POSITION_AND_FACING) < 2U) {
                script_warp_unit_display_to_paired_unit(operand_1);
            }
            if (operand_1 == EVENT_CALL_FUNCTION_CLEAR_ZALERA_ACTIVE_STATUS_GRAPHICS) {
                draw_runtime_id = script_resolve_runtime_id(CHARACTER_IDENTITY_ZALERA);
                if (draw_runtime_id != EVENT_RUNTIME_ID_NONE) {
                    status_queue_current_status_graphics(battle_unit_get_battle_id_by_runtime_id(draw_runtime_id), 0);
                    g_thread_call_target = (void (*)(void))battle_unit_update_graphics_by_runtime_id;
                    thread_call_on_main_stack(draw_runtime_id);
                }
            }
        } else if (opcode == EVENT_OPCODE_BLUE_REMOVE_UNIT) {
            script_blue_remove_unit(first_operand);
        } else if ((opcode == EVENT_OPCODE_END) || (opcode == EVENT_OPCODE_END_2)) {
            map_wait_for_refresh();
            do {
                thread_yield();
            } while (thread_find_running_by_task(THREAD_TASK_EVENT_BLOCK) != 0);
            do {
                thread_yield();
            } while (main_file_is_still_loading() != 0);
            thread_yield();
            if (script_is_tutorial_event_slot() != 0) {
                gfx_ramp_screen_overlay_intensity_to_max();
                g_main_game_options = g_menu_saved_game_options;
                script_set_variable(EVENT_SCRIPT_VAR_SUPPRESS_PROGRESS_EFFECTS, 0);
                battle_state_start_close_battle(0x20);
            }
            if (g_menu_input_mode == MENU_INPUT_PLAYER) {
                for (work_index = 2; work_index < 0xE; work_index++) {
                    thread_wait_until_inactive(work_index);
                }
            }
            operand_1 = script_get_variable(EVENT_SCRIPT_VAR_CURRENT_EVENT);
            if (operand_1 != EVENT_ID_OLAN_WOUNDED) {
                if (operand_1 != EVENT_ID_BEOWULF_INVITATION) {
                    battle_gfx_init_evtchr_vram_slots();
                }
            }
            script_set_variable(EVENT_SCRIPT_VAR_BATTLE_NOT_STARTED, 0);
            event_flags = (g_main_scenario_event_finish_operations[operand_1] & ~0x0CFF) >> 8;
            if (g_menu_input_mode == MENU_INPUT_PLAYER && event_flags == 0
                && (operand_1 - EVENT_ID_COMMON_FIRST) >= 0x1AU && operand_1 != 0) {
                g_script_condition_unit_battle_id = 0xFF;
                script_run_scenario_conditions();
                if (script_load_next_event() != 0) {
                    goto restart_script;
                }
            }
            work_index = 0;
            battle_unit_snap_all_facings_to_quadrants();
            unit_update_staged_status_data(0, 1);
            for (; work_index < BATTLE_UNIT_SLOT_COUNT; work_index++) {
                if (battle_unit_has_runtime_id(work_index) != 0) {
                    operand_1 = battle_unit_get_battle_id_by_runtime_id(work_index);
                    if (operand_1 >= 0) {
                        unit = battle_unit_get_stats_from_battle_id(operand_1);
                        unit->position.facing = battle_unit_get_facing_quadrant_by_runtime_id(work_index);
                    }
                }
            }
            camera_normalize_yaw_angle();
            camera_init_tilt_and_zoom();
            thread_exit_current();
        }
        /* ---- Flow control: forward and backward jumps ---- */
        else if (opcode == EVENT_OPCODE_UNKNOWN_DA) {
        } else if (opcode == EVENT_OPCODE_JUMP_FORWARD_IF_ZERO) {
            if (g_script_variables[EVENT_SCRIPT_VAR_COMPARISON_RESULT] == 0) {
                instruction_offset = script_find_jump_target(
                    instruction_offset + 2, EVENT_OPCODE_FORWARD_TARGET, EVENT_OPCODE_FORWARD_TARGET_EXIT, operand_1);
                continue;
            }
        } else if (opcode == EVENT_OPCODE_JUMP_FORWARD) {
            instruction_offset
                = script_find_jump_target(instruction_offset + 2, EVENT_OPCODE_FORWARD_TARGET, -1, operand_1);
            continue;
        } else if (opcode == EVENT_OPCODE_FORWARD_TARGET) {
        } else if (opcode == EVENT_OPCODE_FORWARD_TARGET_EXIT) {
            break;
        } else if (opcode == EVENT_OPCODE_JUMP_BACK) {
            instruction_offset = script_find_jump_target(instruction_offset, EVENT_OPCODE_BACK_TARGET, -1, operand_1);
            continue;
        } else if (opcode == EVENT_OPCODE_BACK_TARGET) {
        } else if (opcode == EVENT_OPCODE_UNKNOWN_D8) {
        } else if (opcode == EVENT_OPCODE_BACK_TARGET_BARRIER) {
        } else if (opcode == EVENT_OPCODE_UNKNOWN_DC) {
            g_world_script_opcode_dc_value = 0xD;
        }
        /* ---- Dialogue and portraits ---- */
        else if (opcode == EVENT_OPCODE_DISPLAY_MESSAGE) {
            operand_1 = thread_resolve_id(operand_1);
            thread_start(operand_1, text_run_dialog_thread_variant);
            text_build_display_message(instruction_offset, operand_2, &g_thread_contexts[operand_1].task_words[6]);
            g_thread_contexts[operand_1].function_parameter_1 = operand_2;
            g_thread_contexts[operand_1].function_parameter_2 = ((operand_4 << 8) + operand_3) - 1;
            dialogue_cursor = (u8*)g_script_event_block;
            dialogue_cursor += instruction_offset;
            message_instruction = (event_display_message_instruction_t*)dialogue_cursor;
            operand_4 = message_instruction->unit_id_le[1];
            operand_3 = message_instruction->unit_id_le[0];
            g_thread_contexts[operand_1].function_parameter_3 = (operand_4 << 8) + operand_3;
            g_thread_contexts[operand_1].function_parameter_4 = message_instruction->portrait_selector;
            g_thread_contexts[operand_1].task_words[2] = script_read_s16(message_instruction->x_offset_le);
            g_thread_contexts[operand_1].task_words[3] = script_read_s16(
                ((event_display_message_instruction_t*)(instruction_offset + (u8*)g_script_event_block))->y_offset_le);
            g_thread_contexts[operand_1].task_words[4] = script_read_s16(
                ((event_display_message_instruction_t*)(instruction_offset + (u8*)g_script_event_block))
                    ->arrow_offset_le);
            g_thread_contexts[operand_1].task_words[5]
                = ((event_display_message_instruction_t*)((u8*)g_script_event_block + instruction_offset))
                      ->window_options;
            thread_yield();
        } else if (opcode == EVENT_OPCODE_CHANGE_DIALOG) {
            for (work_index = 0; work_index < 6; work_index += 2) {
                operand_2 = g_menu_window_buffers[work_index].thread_id;
                if ((g_menu_window_buffers[work_index].dialogue_selector == operand_1)
                    && (thread_is_running_asm(operand_2) == 1)
                    && (g_thread_contexts[operand_2].task_id == THREAD_TASK_DIALOG_AWAIT_TEXT)) {
                    operand_1 = script_read_s16(
                        ((event_change_dialog_instruction_t*)(instruction_offset + (u8*)g_script_event_block))
                            ->text_id_le);
                    if ((u16)operand_1 != 0xFFFF) {
                        operand_1 -= 1;
                    }
                    g_thread_contexts[operand_2].function_parameter_2 = operand_1;
                    g_thread_contexts[operand_2].function_parameter_4 = script_read_s16(
                        ((event_change_dialog_instruction_t*)(instruction_offset + (u8*)g_script_event_block))
                            ->portrait_selector_le);
                    g_thread_contexts[operand_2].task_id = THREAD_TASK_RESUME;
                    break;
                }
            }
        } else if (opcode == EVENT_OPCODE_PORTRAIT_COL) {
            script_opcode_portrait_col(operand_1);
        }
        /* ---- Map objects ---- */
        else if (opcode == EVENT_OPCODE_USE_3D_OBJECT) {
            g_world_3d_object_use_request = 1;
            g_world_event_map_command_80_args.arg1 = operand_1;
            g_world_event_map_command_80_args.arg2 = operand_2;
            thread_yield();
        } else if (opcode == EVENT_OPCODE_WAIT_3D_OBJECT) {
            g_3d_object_wait_status = 1;
            do {
                thread_yield();
            } while (g_3d_object_wait_status != 0);
        } else if (opcode == EVENT_OPCODE_USE_FIELD_OBJECT) {
            g_world_field_object_use_request = 1;
            g_world_event_map_command_83_args.arg1 = operand_1;
            g_world_event_map_command_83_args.arg2 = operand_2;
            thread_yield();
        } else if (opcode == EVENT_OPCODE_WAIT_FIELD_OBJECT) {
            g_field_object_wait_status = 1;
            do {
                thread_yield();
            } while (g_field_object_wait_status != 0);
        }
        /* ---- unit sprites and animation ---- */
        else if (opcode == EVENT_OPCODE_MIRROR_SPRITE) {
            script_opcode_mirror_sprite((const event_mirror_sprite_parameters_t*)parameters);
        } else if (opcode == EVENT_OPCODE_RESET_PALETTE) {
            script_opcode_reset_palette((const event_unit_parameters_t*)parameters);
        } else if (opcode == EVENT_OPCODE_SET_FIRST_UNIT) {
            operand_1 = script_resolve_runtime_id(first_operand);
            if (operand_1 != EVENT_RUNTIME_ID_NONE) {
                battle_unit_move_to_head(operand_1);
            }
        } else if (opcode == EVENT_OPCODE_UNIT_ANIM) {
            script_unit_anim((const event_unit_anim_parameters_t*)parameters);
        } else if (opcode == EVENT_OPCODE_MARCH) {
            script_march_units((const event_march_parameters_t*)parameters);
        } else if (opcode == EVENT_OPCODE_SET_UNIT_MOVEMENT_EFFECT_SUPPRESSION) {
            script_set_units_movement_effect_suppression((const event_movement_effect_parameters_t*)parameters);
        } else if (opcode == EVENT_OPCODE_WAIT_UNIT_READY) {
            operand_1 = script_resolve_runtime_id(first_operand);
            if (operand_1 != EVENT_RUNTIME_ID_NONE) {
                script_wait_for_unit_ready(operand_1);
            }
        }
        /* ---- Map state, weather, and time of day ---- */
        else if (opcode == EVENT_OPCODE_CHANGE_MAP_BETA) {
            g_event_pending_map_jump_out_id = first_operand;
            thread_wait_frames(2);
            map_reset_script_light_transform();
        } else if (opcode == EVENT_OPCODE_CHANGE_MAP) {
            g_event_pending_map_jump_out_2_id = first_operand;
            thread_wait_frames(2);
            map_reset_script_light_transform();
        } else if (opcode == EVENT_OPCODE_SET_DAYTIME) {
            g_event_pending_map_state = operand_1;
            thread_wait_frames(2);
            map_wait_for_refresh();
        } else if (opcode == EVENT_OPCODE_WEATHER) {
            g_event_weather_request = first_operand;
            thread_wait_frames(2);
            map_wait_for_refresh();
        } else if (opcode == EVENT_OPCODE_RELOAD_MAP_STATE) {
            g_world_map_reload_state = 1;
            do {
                thread_yield();
            } while (g_world_map_reload_state != 0);
        } else if (opcode == EVENT_OPCODE_APPEND_MAP_STATE) {
            g_world_map_append_state = 1;
            do {
                thread_yield();
            } while (g_world_map_append_state != 0);
        } else if (opcode == EVENT_OPCODE_PAUSE) {
            script_opcode_pause();
        } else if (opcode == EVENT_OPCODE_RESUME_WAITING_DIALOG) {
            for (work_index = 1; work_index < 17; ++work_index) {
                thread = &g_thread_contexts[work_index];
                if (thread->is_running && thread->task_id == THREAD_TASK_WAIT_FOR_RESUME
                    && thread->function_parameter_2 == first_operand) {
                    thread->task_id = THREAD_TASK_RESUME;
                }
            }
        } else if (opcode == EVENT_OPCODE_FLIP_MESSAGE_PORTRAIT) {
            script_toggle_message_portrait_flip(parameters);
        } else if (opcode == EVENT_OPCODE_UNKNOWN_17) {
        }
        /* ---- Effects ---- */
        else if (opcode == EVENT_OPCODE_EFFECT) {
            g_event_pending_effect_id = first_operand;
            g_world_event_effect_target_runtime_id
                = script_resolve_runtime_id(((((u8*)g_script_event_block) + instruction_offset))[3]);
            effect_instruction = (u8*)g_script_event_block;
            effect_instruction += instruction_offset;
            g_event_effect_target_x = effect_instruction[4];
            g_event_effect_target_y = effect_instruction[5];
            g_event_effect_target_mode = effect_instruction[6];
            script_set_variable(EVENT_SCRIPT_VAR_CAMERA_ROLL, 0);
            thread_wait_frames(2);
        } else if (opcode == EVENT_OPCODE_EFFECT_START) {
            do {
                thread_yield();
                g_thread_call_target = (void (*)(void))battle_effect_try_init_data;
            } while (thread_call_on_main_stack() != 0);
        } else if (opcode == EVENT_OPCODE_EFFECT_END) {
            thread_start(thread_resolve_id(THREAD_SLOT_FIRST_IDLE), &script_play_effect_thread);
        }
        /* ---- Camera ---- */
        else if (opcode == EVENT_OPCODE_FOCUS) {
            camera_store_yaw_band(g_script_variables[EVENT_SCRIPT_VAR_CAMERA_YAW]);
            script_focus((event_focus_parameters_t*)parameters);
        } else if (opcode == EVENT_OPCODE_FOCUS_SPEED) {
            script_variables = g_script_variables;
            script_focus_speed(parameters, script_variables + EVENT_SCRIPT_VAR_CAMERA_X,
                script_variables + EVENT_SCRIPT_VAR_CAMERA_TILT);
        } else if (opcode == EVENT_OPCODE_RELATIVE_CAMERA) {
            script_apply_relative_camera(
                (event_camera_values_t*)parameters, g_script_variables + EVENT_SCRIPT_VAR_CAMERA_X);
        } else if (opcode == EVENT_OPCODE_CAMERA) {
            operand_1 = thread_resolve_id(THREAD_SLOT_FIRST_IDLE);
            thread_start(operand_1, camera_thread);
            g_thread_contexts[operand_1].function_parameter_1 = (s32)parameters;
        } else if (opcode == EVENT_OPCODE_CAMERA_FUSION_START) {
            operand_1 = thread_resolve_id(THREAD_SLOT_FIRST_IDLE);
            thread_start(operand_1, camera_fusion_thread);
            g_thread_contexts[operand_1].function_parameter_1 = (s32)parameters;
            instruction_offset
                = script_find_instruction_byte_offset(instruction_offset, EVENT_OPCODE_CAMERA_FUSION_END);
        } else if (opcode == EVENT_OPCODE_CAMERA_FUSION_END) {
        } else if (opcode == EVENT_OPCODE_CAMERA_SPEED_CURVE) {
            g_camera_speed_curve = operand_1;
        } else if (opcode == EVENT_OPCODE_UNKNOWN_74) {
            script_opcode_unknown_74(parameters);
        }
        /* ---- unit movement and facing ---- */
        else if (opcode == EVENT_OPCODE_WALK_TO) {
        } else if (opcode == EVENT_OPCODE_JUMP) {
            script_opcode_jump((const event_jump_parameters_t*)parameters);
        } else if (opcode == EVENT_OPCODE_WAIT_WALK) {
            script_opcode_wait_walk(first_operand);
        } else if (opcode == EVENT_OPCODE_WALK_TO_ANIM) {
            operand_1 = script_resolve_runtime_id(first_operand);
            if (operand_1 != EVENT_RUNTIME_ID_NONE) {

                g_unit_pending_animation_ids[operand_1]
                    = script_read_s16(instruction_offset + ((u8*)g_script_event_block) + 3);
                g_unit_was_moving_latch[operand_1] = 0;
            }
        } else if (opcode == EVENT_OPCODE_WAIT_ALL_UNIT_MOVEMENT) {
            do {
                thread_yield();
            } while (unit_is_moving(-1) != 0);
        } else if (opcode == EVENT_OPCODE_WAIT_FILE_LOAD) {
            do {
                thread_yield();
            } while (main_file_is_still_loading() != 0);
        } else if (opcode == EVENT_OPCODE_ROTATE_UNIT) {
            script_rotate_unit((const event_rotate_unit_parameters_t*)parameters);
        } else if (opcode == EVENT_OPCODE_FACE_EACH_OTHER) {
            script_face_unit((const event_face_unit_parameters_t*)parameters, 0);
        } else if (opcode == EVENT_OPCODE_FACE_UNIT) {
            script_face_unit((const event_face_unit_parameters_t*)parameters, 1);
        } else if (opcode == EVENT_OPCODE_FACE_TILE) {
            script_face_tile((const event_face_tile_parameters_t*)parameters);
        } else if (opcode == EVENT_OPCODE_WAIT_ROTATE_UNIT) {
            script_opcode_wait_rotate_all(first_operand);
        } else if (opcode == EVENT_OPCODE_WAIT_ROTATE_ALL) {
            script_opcode_wait_rotate_all(-1);
        }
        /* ---- Blocks, earthquakes, darkness, and screen color ---- */
        else if (opcode == EVENT_OPCODE_BLOCK_START) {
            operand_1 = thread_resolve_id(THREAD_SLOT_FIRST_IDLE);
            thread_start(operand_1, script_block_start_thread);
            g_thread_contexts[operand_1].function_parameter_1 = (s32)parameters;
            instruction_offset = script_find_instruction_byte_offset(instruction_offset, EVENT_OPCODE_BLOCK_END);
        } else if (opcode == EVENT_OPCODE_BLOCK_END) {
        } else if (opcode == EVENT_OPCODE_UNKNOWN_23) {
        } else if (opcode == EVENT_OPCODE_UNKNOWN_24) {
        } else if (opcode == EVENT_OPCODE_EARTHQUAKE_END) {
            thread_wait_frames(1);
            thread_suspend(thread_find_running_by_task(THREAD_TASK_EARTHQUAKE));
        } else if (opcode == EVENT_OPCODE_SET_SPEAKER_PAIR) {
            g_text_speaker_swap_unit_a = first_operand;
            g_text_speaker_swap_unit_b = script_read_s16(instruction_offset + ((u8*)g_script_event_block) + 3);
        } else if (opcode == EVENT_OPCODE_MAP_DARKNESS) {
            operand_1 = thread_resolve_id(THREAD_SLOT_FIRST_IDLE);
            thread_start(operand_1, &script_map_darkness);
            g_thread_contexts[operand_1].function_parameter_1 = (s32)parameters;
        } else if (opcode == EVENT_OPCODE_COLOR_SCREEN) {
            operand_1 = thread_resolve_id(THREAD_SLOT_FIRST_IDLE);
            thread_start(operand_1, script_color_screen_thread);
            g_thread_contexts[operand_1].function_parameter_1 = (s32)parameters;
        } else if (opcode == EVENT_OPCODE_EARTHQUAKE_START) {
            operand_1 = thread_resolve_id(THREAD_SLOT_FIRST_IDLE);
            thread_start(operand_1, &script_earthquake_start);
            g_thread_contexts[operand_1].function_parameter_1 = (s32)parameters;
        } else if (opcode == EVENT_OPCODE_SPRITE_MOVE || opcode == EVENT_OPCODE_SPRITE_MOVE_BETA) {
            operand_1 = thread_resolve_id_after_current(THREAD_SLOT_FIRST_IDLE);
            if (opcode == EVENT_OPCODE_SPRITE_MOVE) {
                thread_start(operand_1, &script_sprite_move);
            } else {
                thread_start(operand_1, &script_sprite_move_beta);
            }
            g_thread_contexts[operand_1].function_parameter_1 = (s32)parameters;
            g_thread_contexts[operand_1].task_id = THREAD_TASK_SPRITE_MOVE;
            g_thread_contexts[operand_1].task_words[0] = script_resolve_runtime_id(first_operand);
        } else if (opcode == EVENT_OPCODE_WAIT_SPRITE_MOVE) {
            script_opcode_wait_sprite_move(first_operand);
        } else if (opcode == EVENT_OPCODE_MAP_LIGHT) {
            operand_1 = thread_resolve_id(THREAD_SLOT_FIRST_IDLE);
            thread_start(operand_1, map_light_thread);
            g_thread_contexts[operand_1].function_parameter_1 = (s32)parameters;
        }
        /* ---- Background, palettes, and terrain ---- */
        else if (opcode == EVENT_OPCODE_BACKGROUND) {
            mem_copy_bytes_nonempty(&g_event_background_colors.colors[MAP_BACKGROUND_GRADIENT_TOP], parameters, 3);
            mem_copy_bytes_nonempty(
                &g_event_background_colors.colors[MAP_BACKGROUND_GRADIENT_BOTTOM], parameters + 3, 3);
            if (parameters[7] == 0) {
                battle_map_transition_and_store_background_gradient(parameters[6], &g_event_background_colors);
            } else {
                battle_map_set_background_gradient(parameters[6], &g_event_background_colors);
            }
        } else if (opcode == EVENT_OPCODE_COLOR_BG_BETA) {
            battle_map_modify_background_gradient(
                parameters[0], parameters[4], (s8)(parameters)[1], (s8)(parameters)[2], (s8)(parameters)[3]);
        } else if (opcode == EVENT_OPCODE_COLOR_UNIT) {
            script_opcode_color_unit((const event_color_unit_parameters_t*)parameters);
        } else if (opcode == EVENT_OPCODE_RESUME_UNIT_PALETTE_UPDATES) {
            script_set_units_palette_update_suppression((u16)first_operand, 0);
        } else if (opcode == EVENT_OPCODE_SUPPRESS_UNIT_PALETTE_UPDATES) {
            script_set_units_palette_update_suppression((u16)first_operand, 1);
        } else if (opcode == EVENT_OPCODE_COLOR_FIELD) {
            battle_map_color_field(
                parameters[0], parameters[4], (s8)(parameters)[1], (s8)(parameters)[2], (s8)(parameters)[3]);
        } else if (opcode == EVENT_OPCODE_STORE_MODIFIED_MAP_PALETTE) {
            battle_map_store_modified_palette_colors(0, 0, 1);
        } else if (opcode == EVENT_OPCODE_SET_TILE_CAMERA_BLOCK) {
            battle_map_set_tile_data_value(
                MAP_TILE_DATA_FLAT_CAMERA_BLOCK_MASK, parameters[0], parameters[1], parameters[2], parameters[3]);
        } else if (opcode == EVENT_OPCODE_SET_TILE_HEIGHT) {
            tile = battle_map_get_tile_data_pointer(parameters[0], parameters[1], parameters[2]);
            if ((tile->surface.type - MAP_SURFACE_GRASSLAND) >= 2U) {
                tile->surface.type = MAP_SURFACE_STONE_FLOOR;
            }
            tile->height = parameters[3];
            tile->flags.untargetable = parameters[4];
            tile->flags.blocked = 0;
        }
        /* ---- Event speed, dark screen, conditions, and graphics ---- */
        else if (opcode == EVENT_OPCODE_EVENT_SPEED) {
            script_set_event_speed(operand_1);
        } else if (opcode == EVENT_OPCODE_DARK_SCREEN) {
            g_menu_hide_numeric_values = 0;
            operand_1 = thread_resolve_id(THREAD_SLOT_FIRST_IDLE);
            thread_start(operand_1, &script_dark_screen_thread);
            thread_set_parameters(operand_1, 0, (s32)parameters, 0);
        } else if (opcode == EVENT_OPCODE_REMOVE_DARK_SCREEN) {
            operand_1 = thread_find_running_by_task(THREAD_TASK_DARK_SCREEN_HOLD);
            if (operand_1 != 0) {
                g_thread_contexts[operand_1].task_id = THREAD_TASK_DARK_SCREEN;
            }
        } else if (opcode == EVENT_OPCODE_DISPLAY_CONDITIONS) {
            operand_3 = thread_resolve_id(THREAD_SLOT_FIRST_IDLE);
            if ((operand_3 == 4) && (operand_1 == EVENT_DISPLAY_CONDITION_APPLY_POST_BATTLE_UNIT_CHANGES)) {
                thread_wait_until_inactive(2);
                operand_3 = 2;
            }
            /* Keep the cached second operand as an allocator input at this join;
             * otherwise GCC exchanges the target s5/s6 operand lifetimes. */
            __asm__("" : : "r"(operand_2));
            thread_start(operand_3, &script_execute_display_conditions_instruction);
            thread_set_parameters(operand_3, operand_1, operand_2, 0);
        } else if (opcode == EVENT_OPCODE_SHOW_GRAPHIC) {
            operand_3 = thread_resolve_id(THREAD_SLOT_FIRST_IDLE);
            thread_start(operand_3, &script_show_graphic);
            thread_set_parameters(operand_3, operand_1, 0, 0);
        }
        /* ---- Sound and music ---- */
        else if (opcode == EVENT_OPCODE_SOUND_EFFECT) {
            g_sound_pending_sfx_id = first_operand;
        } else if (opcode == EVENT_OPCODE_BG_SOUND) {
            operand_1 = thread_resolve_id(THREAD_SLOT_FIRST_IDLE);
            thread_start(operand_1, sound_run_background_sfx_thread);
            thread_set_parameters(operand_1, (s32)parameters, 0, 0);
        } else if (opcode == EVENT_OPCODE_EDIT_BG_SOUND) {
            operand_1 = thread_resolve_id(THREAD_SLOT_FIRST_IDLE);
            thread_start(operand_1, sound_run_background_fade_thread);
            thread_set_parameters(operand_1, (s32)parameters, 0, 0);
        } else if (opcode == EVENT_OPCODE_SWITCH_TRACK) {
            g_world_sound_music_switch_request = operand_1;
            g_world_event_music_switch.volume = operand_2;
            g_world_event_music_switch.fade = operand_3;
            thread_yield();
        } else if (opcode == EVENT_OPCODE_FADE_SOUND) {
            /* The target MIPS variable shift uses the low five count bits.
             * Explicit C masking adds an instruction; counts >=32 retain the
             * original C shift limitation rather than an invented input restriction. */
            g_world_sound_music_volume_transition_request = operand_2 << (operand_1 + 0x10);
            thread_yield();
        } else if (opcode == EVENT_OPCODE_END_TRACK) {
            g_world_sound_music_unload_slot_request = operand_1;
            thread_yield();
        }
        /* ---- EVTCHR and reserved VRAM slots ---- */
        else if (opcode == EVENT_OPCODE_LOAD_EVTCHR) {
            operand_2 = thread_resolve_id(THREAD_SLOT_FIRST_IDLE);
            thread_start(operand_2, gfx_load_evtchr_thread);
            thread_set_parameters(
                operand_2, operand_1, script_read_s16(instruction_offset + ((u8*)g_script_event_block) + 2), 0);
            thread_yield();
        } else if (opcode == EVENT_OPCODE_LOAD_EVTCHR_CLEAR) {
            g_event_pending_loaded_evtchr_clear_slot = operand_1;
            thread_yield();
        } else if (opcode == EVENT_OPCODE_SAVE_EVTCHR) {
            g_event_pending_evtchr_save_slot = operand_1;
            thread_yield();
        } else if (opcode == EVENT_OPCODE_SAVE_EVTCHR_CLEAR) {
            g_event_pending_saved_evtchr_clear_slot = operand_1;
            thread_yield();
        } else if (opcode == EVENT_OPCODE_COPY_UNIT_GRAPHICS_TO_RESERVED_VRAM_SLOT) {
            g_event_pending_unit_vram_copy = (script_resolve_runtime_id(first_operand) << 8) | operand_3;
            thread_yield();
        } else if (opcode == EVENT_OPCODE_RELEASE_RESERVED_VRAM_SLOT) {
            g_event_pending_reserved_vram_release_slot = operand_1;
            thread_yield();
        } else if (opcode == EVENT_OPCODE_UNKNOWN_E0) {
        }
        /* ---- Adding and removing units ---- */
        else if (opcode == EVENT_OPCODE_REMOVE_UNIT) {
            s16 normalized_unit_id = script_resolve_runtime_id(first_operand);
            if (normalized_unit_id != EVENT_RUNTIME_ID_NONE) {
                removed_runtime_id = normalized_unit_id;
                while (unit_is_moving(removed_runtime_id) != 0) {
                    thread_yield();
                }
                if (removed_runtime_id != EVENT_RUNTIME_ID_NONE) {
                    operand_1 = battle_unit_get_battle_id_by_runtime_id(removed_runtime_id);
                    battle_unit_remove_by_runtime_id(removed_runtime_id);
                    if (operand_1 >= 0) {
                        battle_unit_set_cannot_exist(operand_1);
                    }
                }
            }
        } else if (opcode == EVENT_OPCODE_DRAW) {
            draw_runtime_id = script_resolve_runtime_id(first_operand);
            if (draw_runtime_id != EVENT_RUNTIME_ID_NONE) {
                battle_unit_draw_and_activate_by_runtime_id(draw_runtime_id);
                if (script_get_variable(EVENT_SCRIPT_VAR_CURRENT_EVENT) == EVENT_ID_LIMBERRY_CEMETERY_BATTLE) {
                    status_queue_current_status_graphics(battle_unit_get_battle_id_by_runtime_id(draw_runtime_id), 1);
                    g_thread_call_target = (void (*)(void))battle_unit_update_graphics_by_runtime_id;
                    thread_call_on_main_stack(draw_runtime_id);
                }
            }
        } else if (opcode == EVENT_OPCODE_ERASE) {
            erased_runtime_id = script_resolve_runtime_id(first_operand);
            if (erased_runtime_id != EVENT_RUNTIME_ID_NONE) {
                battle_gfx_reset_unit_graphic_trigger(erased_runtime_id);
            }
        } else if (opcode == EVENT_OPCODE_ADD_GHOST_UNIT) {
            ghost_parameters = (u8*)g_script_event_block + instruction_offset + 3;
            ghost_runtime_id = 3;
            do {
                --ghost_runtime_id;
            } while (ghost_runtime_id >= 0);
            for (ghost_runtime_id = 0; ghost_runtime_id < BATTLE_UNIT_SLOT_COUNT; ghost_runtime_id++) {
                if (battle_unit_has_runtime_id(ghost_runtime_id) == 0) {
                    g_script_unit_slots[ghost_parameters[0]] = ghost_runtime_id;
                    break;
                }
            }
            if (ghost_runtime_id != BATTLE_UNIT_SLOT_COUNT) {
                battle_script_opcode_add_ghost_unit(ghost_parameters[1], ghost_parameters[2], ghost_parameters[3],
                    ghost_parameters[4], first_operand, g_script_unit_slots[ghost_parameters[0]], ghost_parameters[5]);
                g_unit_graphics_load_pending = 1U;
            } else {
                continue;
            }
        } else if (opcode == EVENT_OPCODE_UNKNOWN_75) {
        } else if (opcode == EVENT_OPCODE_ADD_UNIT) {
            script_add_unit_stub();
            appearance_unit = battle_unit_find_stats_by_entd_unit_id(first_operand, &added_unit_state);
            if ((added_unit_state == -2) || (added_unit_state >= 0)) {
                for (added_battle_id = 0; added_battle_id < BATTLE_UNIT_SLOT_COUNT; added_battle_id++) {
                    if (battle_unit_get_stats_from_battle_id(added_battle_id) == appearance_unit) {
                        break;
                    }
                }
                operand_1 = battle_unit_get_runtime_id_by_battle_id(added_battle_id);
                if (operand_1 == -1) {
                    unit_save_staged_status_data(added_battle_id);
                    battle_gfx_load_unit_graphics_by_battle_id(added_battle_id, operand_3);
                    g_unit_graphics_load_pending = 1U;
                } else {
                    if (operand_3 == 0) {
                        battle_unit_draw_and_activate_by_runtime_id(operand_1);
                    }
                    if (unit_save_staged_status_data(added_battle_id) != 0) {
                        g_thread_call_target = (void (*)(void))battle_unit_update_graphics_by_runtime_id;
                        thread_call_on_main_stack(operand_1);
                    }
                }
            }
        } else if (opcode == EVENT_OPCODE_WARP_UNIT) {
            script_warp_unit((const event_warp_unit_parameters_t*)parameters);
        } else if (opcode == EVENT_OPCODE_UNKNOWN_61) {
        } else if (opcode == EVENT_OPCODE_SET_BATTLE_UNIT_POSITION) {
            unit = battle_unit_get_stats_from_battle_id(operand_1);
            position_instruction
                = (event_set_battle_unit_position_instruction_t*)((u8*)g_script_event_block + instruction_offset);
            unit->x = position_instruction->placement.x;
            unit->position.y = position_instruction->placement.y;
            unit->position.higher_elevation = position_instruction->placement.elevation;
            unit->position.facing = position_instruction->placement.facing;
        } else if (opcode == EVENT_OPCODE_WAIT_ADD_UNIT) {
            do {
                thread_yield();
            } while (g_unit_graphics_load_pending != 0);
        } else if (opcode == EVENT_OPCODE_WAIT_ADD_UNIT_END) {
            do {
                thread_yield();
            } while (thread_find_running_by_task(THREAD_TASK_ADD_UNIT) != 0);
        } else if (opcode == EVENT_OPCODE_ADD_UNIT_START) {
            operand_1 = thread_resolve_id(THREAD_SLOT_FIRST_IDLE);
            thread_start(operand_1, script_add_unit_start_thread);
            g_thread_contexts[operand_1].function_parameter_1 = (s32)parameters;
            instruction_offset = script_find_instruction_byte_offset(instruction_offset, EVENT_OPCODE_ADD_UNIT_END);
        } else if (opcode == EVENT_OPCODE_ADD_UNIT_END) {
        }
        /* ---- Waits and synchronization ---- */
        else if (opcode == EVENT_OPCODE_WAIT_FOR_INSTRUCTION) {
            /* Yield until no thread runs the named task. The outer wait stays
             * a label loop: as a C loop its notes raise operand_3's
             * allocation weight and swap the target's s5/s6. */
            operand_3 = (operand_2 << 8) + operand_1;
            threads = g_thread_contexts;
        wait_matching_task:
            thread_yield();
            for (work_index = 1; work_index < 0x11; work_index++) {
                /* Typed indexing reverses the target addu operands; keep the 32-bit address sum. */
                thread = (thread_t*)((work_index * sizeof(thread_t)) + (u32)threads);
                if (thread->is_running != 0 && thread->task_id == operand_3) {
                    break;
                }
            }
            if (work_index != 0x11) {
                goto wait_matching_task;
            }
        } else if (opcode == EVENT_OPCODE_YIELD) {
            thread_yield();
        } else if (opcode == EVENT_OPCODE_WAIT) {
            if (script_is_tutorial_event_slot() != 0 && first_operand >= 3) {
                thread_wait_frames(first_operand / battle_state_get_vsync_interval());
            } else {
                thread_wait_frames(first_operand);
            }
        } else if (opcode == EVENT_OPCODE_TUTORIAL_BUTTON_WAIT) {
            do {
                thread_yield();
            } while (!(first_operand & g_script_unfiltered_controller_input));
        } else if (opcode == EVENT_OPCODE_END_SOUND) {
            main_sound_stop_weather_sfx_music();
        }
        /* ---- Miscellaneous unit and display commands ---- */
        else if (opcode == EVENT_OPCODE_UNIT_SHADOW) {
            shadow_runtime_id = script_resolve_runtime_id(first_operand);
            if (shadow_runtime_id != EVENT_RUNTIME_ID_NONE) {
                if (operand_3 != 0) {
                    battle_gfx_clear_shadow_graphic_trigger_by_runtime_id(shadow_runtime_id);
                } else {
                    battle_gfx_set_shadow_graphic_trigger_by_runtime_id(shadow_runtime_id);
                }
            }
        } else if (opcode == EVENT_OPCODE_DEBUG_PAUSE) {
            script_print_debug_message();
        } else if (opcode == EVENT_OPCODE_WAIT_VALUE) {
            script_opcode_wait_value((const event_wait_value_parameters_t*)parameters);
        } else if (opcode == EVENT_OPCODE_EVTCHR_PALETTE) {
            runtime_id_for_7f = script_resolve_runtime_id(first_operand);
            if (runtime_id_for_7f != EVENT_RUNTIME_ID_NONE) {
                battle_gfx_set_evtchr_palette_by_runtime_id(runtime_id_for_7f, operand_3, operand_4);
            }
        } else if (opcode == EVENT_OPCODE_LOAD_ATTACK_GRAPHICS) {
            script_opcode_load_attack_graphics();
        } else if (opcode == EVENT_OPCODE_CHANGE_STATS) {
            script_change_stats((const event_change_stats_parameters_t*)parameters);
        } else if (opcode == EVENT_OPCODE_PLAY_TUNE) {
            main_sound_play_tune(operand_1);
        } else if (opcode == EVENT_OPCODE_UNLOCK_DATE) {
            if ((script_get_variable(EVENT_SCRIPT_VAR_SUPPRESS_PROGRESS_EFFECTS) == 0) && (operand_1 != 0)) {
                date_month = script_get_variable(EVENT_SCRIPT_VAR_MONTH);
                script_opcode_unlock_date(g_main_treasure_acquisition_date_bits, operand_1 - 1, date_month,
                    script_get_variable(EVENT_SCRIPT_VAR_DAY));
            }
        } else if (opcode == EVENT_OPCODE_TEMP_WEAPON) {
            palette_runtime_id = script_resolve_runtime_id(first_operand);
            if (palette_runtime_id != EVENT_RUNTIME_ID_NONE) {
                battle_gfx_set_thrown_item_palette_by_runtime_id(operand_3, palette_runtime_id);
            }
        } else if (opcode == EVENT_OPCODE_ARROW) {
            first_runtime_id = script_resolve_runtime_id(first_operand);
            if (first_runtime_id != EVENT_RUNTIME_ID_NONE) {
                battle_unit_get_battle_id_by_runtime_id(first_runtime_id);
                second_runtime_id
                    = script_resolve_runtime_id(script_read_s16(instruction_offset + ((u8*)g_script_event_block) + 3));
                if (second_runtime_id != EVENT_RUNTIME_ID_NONE) {
                    battle_unit_get_battle_id_by_runtime_id(second_runtime_id);
                }
            }
        } else if (opcode == EVENT_OPCODE_MAP_FREEZE) {
            battle_map_freeze();
        } else if (opcode == EVENT_OPCODE_MAP_UNFREEZE) {
            battle_map_unfreeze();
        } else if (opcode == EVENT_OPCODE_UNIT_ANIM_ROTATE) {
            script_opcode_unit_anim_rotate((const event_unit_anim_rotate_parameters_t*)parameters);
        } else if (opcode == EVENT_OPCODE_WAIT_GRAPHIC_PRINT) {
            thread_yield();
            do {
                thread_yield();
                operand_1 = thread_find_running_by_task(THREAD_TASK_SHOW_GRAPHIC);
            } while (operand_1 != 0 && g_thread_contexts[operand_1].function_parameter_4 == 0);
        } else if (opcode == EVENT_OPCODE_LOCK_MENU_INPUT) {
            g_menu_input_lock_mode = operand_1;
        } else if (opcode == EVENT_OPCODE_TUTORIAL_COORDINATE_CONFIRM) {
            while (g_battle_cursor_x != operand_1 || g_battle_cursor_y != operand_2
                || ((initial_buttons = g_script_unfiltered_controller_input) & PSX_PAD_CIRCLE) == 0
                || (initial_buttons & PSX_PAD_DPAD_MASK) != 0) {
                thread_yield();
            }
        }
        /* ---- Map title, dates, teleports, and status ---- */
        else if (opcode == EVENT_OPCODE_SHOW_MAP_TITLE) {
            operand_4 = thread_resolve_id(THREAD_SLOT_FIRST_IDLE);
            menu_request_open_companion_executable(COMPANION_OVERLAY_ATTACK);
            attack_map_load_title_graphic();
            thread_start(operand_4, attack_map_show_title);
            thread_set_parameters(operand_4, (s8)operand_1, (s8)operand_2, (s8)operand_3);
            thread_wait_until_inactive(operand_4);
        } else if (opcode == EVENT_OPCODE_SET_DATE_ADVANCE) {
            if (script_get_variable(EVENT_SCRIPT_VAR_SUPPRESS_PROGRESS_EFFECTS) == 0) {
                script_set_variable(EVENT_SCRIPT_VAR_DATE_ADVANCE, first_operand);
            }
        } else if (opcode == EVENT_OPCODE_TELEPORT_OUT) {
            script_opcode_teleport_out(first_operand, 1);
        } else if (opcode == EVENT_OPCODE_TELEPORT_IN) {
            script_opcode_teleport_in(first_operand, 1);
        } else if (opcode == EVENT_OPCODE_INFLICT_STATUS) {
            operand_1 = thread_resolve_id(THREAD_SLOT_FIRST_IDLE);
            thread_start(operand_1, script_inflict_status_thread);
            thread_set_parameters(operand_1, (s32)parameters, 0, 0);
        } else if (opcode == EVENT_OPCODE_UNKNOWN_95) {
        } else {
            break;
        }
        instruction_offset += 1 + g_script_event_instruction_sizes[opcode];
    }
    thread_exit_current();
}
