#include "fft/attack.h"
#include "fft/battle.h"
#include "fft/battle_effect.h"
#include "fft/battle_gfx.h"
#include "fft/data.h"
#include "fft/event.h"
#include "fft/main_runtime.h"
#include "fft/map.h"
#include "fft/menu.h"
#include "fft/script_variables.h"
#include "fft/thread.h"
#include "fft/world.h"
#include "psx/pad.h"

/* WORLD scenario interpreter, 0x800f6f20..0x800f92a0.
 *
 * One cooperative thread runs this loop: fetch the opcode and its first four
 * operand bytes, dispatch through the else-if chain below, then advance by the
 * opcode's encoded length. The chain order is the target's compare order and
 * must not be re-sorted. Handlers that seek (jumps) `continue` without the
 * length step; EventEnd may `goto restart_event` for a chained scenario.
 *
 * BATTLE carries the same interpreter (battle_script_execute_event); the two
 * are kept structurally identical so differences stay visible.
 *
 * Unresolved external interfaces retain provisional names. The native
 * inner-call trampoline intentionally forwards varying argument counts through
 * its () ABI. */

/* Declared locally: other reconstructions bind these under different
 * provisional signatures, or the symbol is still unresolved. */
s32 battle_map_set_tile_data_value(s32 field, s32 x, s32 y, s32 layer, s32 value);
extern s32 D_800459D8;

void world_script_execute_event(void) {
    s32 unit_lookup_status;
    s32 ghost_misc_id;
    s32 region_misc_id;
    s32 zalera_misc_id;
    s32 facing_misc_id;
    s32 month;
    s32 appearance_battle_id;
    s32 waiting_thread_index;
    s32 work_index;
    s32 finishing_thread_index;

    s16 first_halfword;
    s32 removed_misc_id;
    s32 erased_misc_id;
    s16 shadow_misc_id;
    s16 misc_id_for_7f;
    s16 palette_misc_id;
    s16 first_misc_id;
    s16 second_misc_id;
    battle_stats_t* appearance_unit;

    s32 initial_buttons;
    s32 event_flags;
    s32 instruction_offset;
    event_opcode_e opcode;
    u32 operand_2;
    u32 operand_3;
    s32 operand_4;
    s32 operand_1;
    void* position_instruction;
    u8* dialogue_instruction;
    s32* event_words;
    /* Preserve the target callee-saved parameter cursor register. */
    register u8* parameters __asm__("$17");
    u8* instruction;
    void* ghost_operands;
    void* tile;
    battle_stats_t* copy_destination;
    u8* focus_instruction;
    native_thread_t* waiting_thread;
    native_thread_t* cancel_thread;

    world_thread_set_current_task_id(NATIVE_THREAD_TASK_EXECUTE_EVENT);
    if (world_script_check_tutorial_event_slot() != 0) {
        world_script_switch_tutorial_thread();
    }
    if (g_world_menu_input_disabled == 0) {
        for (work_index = 2; work_index < 0xE; work_index++) {
            world_thread_wait_until_inactive(work_index);
        }
        world_camera_run_move_thread();
    } else {
        g_world_menu_hovered_unit_stats_display.y = 0xAA;
        g_world_menu_attack_caster_stats_display.y = 0xAA;
        g_world_menu_attack_target_stats_display.y = 0xAA;
        g_world_menu_right_unit_stats_display.y = 0xAA;
    }
    world_unit_clear_status_staging_data();
    if (world_script_get_variable(EVENT_SCRIPT_VAR_PENDING_STAGED_STATUS) != 0) {
        world_unit_update_staged_status_data(0, 0);
    } else {
        world_process_inflict_status_commands();
    }
    world_script_set_variable(EVENT_SCRIPT_VAR_DISABLED_MENU_ACTIONS, 0);
    world_script_set_variable(EVENT_SCRIPT_VAR_DISABLED_CONTROLLER_INPUTS, 0);
    world_script_set_variable(EVENT_SCRIPT_VAR_FORCED_CONTROLLER_INPUTS, 0);
restart_event:
    world_script_set_event_speed(1U);
    instruction_offset = 0;
    world_camera_init_yaw_angle();
    {
        s32 empty_unit_id = EVENT_UNIT_SLOT_EMPTY;
        /* Keep the shared fill value available before the countdown initializer. */
        for (work_index = 3; work_index >= 0; work_index--) {
            g_world_event_unit_slots[work_index] = empty_unit_id;
        }
    }
    for (;;) {
        instruction = (u8*)g_world_event_script;
        instruction += instruction_offset;
        parameters = instruction + 1;
        operand_1 = instruction[1];
        operand_2 = instruction[2];
        operand_3 = instruction[3];
        operand_4 = instruction[4];
        opcode = instruction[0];
        first_halfword = world_script_load_halfword(parameters);
        /* ---- Control: NOPs, conditions, and script variable arithmetic ---- */
        if (opcode == EVENT_OPCODE_UNKNOWN_C0) {
        } else if (opcode == EVENT_OPCODE_NOP) {
        } else if ((u32)(opcode - EVENT_CONDITION_SECOND_GTE_FIRST) < 6U) {
            world_script_run_condition(opcode);
        } else if ((u32)(opcode - EVENT_OPCODE_ADD_IMMEDIATE) < 0xFU) {
            operand_1 = (u16)first_halfword;
            world_script_run_variable_command(opcode, operand_1,
                world_script_load_halfword(instruction_offset + ((u32)g_world_event_script) + 3) & 0xFFFF, 0);
        } else if (opcode == EVENT_OPCODE_UNKNOWN_4D) {
            g_world_event_map_jump_in_request = (s16)operand_1;
            world_thread_wait_frames(2);
        }
        /* ---- Scenario progression: DismissUnit, CallFunction, BlueRemoveUnit, EventEnd ---- */
        else if (opcode == EVENT_OPCODE_DISMISS_UNIT) {
            if (world_script_get_variable(EVENT_SCRIPT_VAR_SUPPRESS_PROGRESS_EFFECTS) == 0) {
                world_script_dismiss_unit_event_instruction(first_halfword);
            }
        } else if (opcode == EVENT_OPCODE_CALL_FUNCTION) {
            /* Sequential selector tests, not switch alternatives: selectors 6
             * and 14 reuse operand_1 as a loop index and later tests see the
             * changed value. That target-visible fallthrough is preserved. */
            world_thread_wait_frames(4);
            if (world_script_get_variable(EVENT_SCRIPT_VAR_SUPPRESS_PROGRESS_EFFECTS) == 0) {
                if (operand_1 == EVENT_CALL_FUNCTION_JOIN_UNITS_SILENTLY) {
                    if (world_script_get_variable(EVENT_SCRIPT_VAR_CURRENT_EVENT) < 0x4E) {
                        main_item_init_new_game_inventory();
                    }
                    world_unit_join_silently_without_darkscreen();
                }
                if (operand_1 == EVENT_CALL_FUNCTION_OPEN_SAVE_MENU) {
                    operand_1 = 2;
                    do {
                        world_thread_set_parameters(operand_1, 0U, 0, 1);
                        g_world_thread_contexts[operand_1].task_id = NATIVE_THREAD_TASK_STOP_REQUEST;
                        world_thread_wait_until_inactive(operand_1);
                        operand_1 += 1;
                    } while (operand_1 < 0xF);
                    if (world_script_get_variable(EVENT_SCRIPT_VAR_CURRENT_EVENT) != 0x12C) {
                        world_script_set_variable(EVENT_SCRIPT_VAR_NEXT_SCENARIO, 1);
                    }
                    world_script_set_variable(EVENT_SCRIPT_VAR_SAVE_IN_PROGRESS, 1);
                    world_bin_load_file((void*)0xE);
                    g_world_companion_overlay_state = 5;
                    do {
                        world_thread_yield();
                    } while (g_world_companion_overlay_state != 0);
                    world_script_set_variable(EVENT_SCRIPT_VAR_NEXT_SCENARIO, 0);
                    operand_1 = 2;
                    do {
                        world_thread_set_parameters((s32)operand_1, 0U, 0, 1);
                        g_world_thread_contexts[operand_1].task_id = NATIVE_THREAD_TASK_STOP_REQUEST;
                        world_thread_wait_until_inactive((s32)operand_1);
                        operand_1 += 1;
                    } while ((s32)operand_1 < 0xF);
                }
            }
            if ((operand_1 == EVENT_CALL_FUNCTION_ADVANCE_YEAR)
                || (operand_1 == EVENT_CALL_FUNCTION_ADVANCE_YEAR_ALTERNATE)) {
                world_script_advance_date_by_one_year();
            }
            if (operand_1 == EVENT_CALL_FUNCTION_INCREASE_BLUE_TEAM_BRAVE) {
                world_unit_increase_blue_team_brave_by_10();
            }
            if (operand_1 == EVENT_CALL_FUNCTION_REMOVE_ALL_ENEMY_UNITS) {
                world_script_blue_remove_all_enemy_units();
                world_thread_wait_frames(0x78);
            }
            if (operand_1 == EVENT_CALL_FUNCTION_UNKNOWN_05) {
                g_world_camera_yaw_unwrap_request = 1;
            }
            if (operand_1 == EVENT_CALL_FUNCTION_UNKNOWN_08) {
                g_main_debug_display_enabled = 0;
                D_800459D8 = 0;
            }
            if (operand_1 == EVENT_CALL_FUNCTION_MUTE_TEXT_AUDIO_CUE) {
                world_script_set_variable(EVENT_SCRIPT_VAR_MUTE_TEXT_AUDIO_CUE, 1);
            }
            if (operand_1 == EVENT_CALL_FUNCTION_UNMUTE_TEXT_AUDIO_CUE) {
                world_script_set_variable(EVENT_SCRIPT_VAR_MUTE_TEXT_AUDIO_CUE, 0);
            }
            if (operand_1 == EVENT_CALL_FUNCTION_ADVANCE_MAP_DESTRUCTION_AND_WAIT) {
                world_script_set_specialized_map_destroyed();
                if (g_world_map_destruction_wait_state != 0) {
                    do {
                        world_thread_yield();
                    } while (g_world_map_destruction_wait_state != 0);
                }
            }
            if (operand_1 == EVENT_CALL_FUNCTION_PLAY_ALTIMA_MUSIC) {
                g_world_sound_current_music_track_index = 0;
                g_world_sound_music_track_1_id = 0x13;
                g_world_sound_music_track_2_id = 0;
                world_sound_play_music_tracks(0x13, 0);
            }
            if (operand_1 == EVENT_CALL_FUNCTION_COPY_UNIT_8_POSITION_TO_UNIT_9_AND_RELOAD_GRAPHICS) {
                parameters = battle_unit_get_stats_from_battle_id(7);
                copy_destination = battle_unit_get_stats_from_battle_id(8);
                copy_destination->x = (u8)((battle_stats_t*)parameters)->x;
                copy_destination->position.bits.y = ((battle_stats_t*)parameters)->position.bits.y;
                copy_destination->position.bits.higher_elevation
                    = ((battle_stats_t*)parameters)->position.bits.higher_elevation;
                battle_gfx_load_unit_graphics_by_battle_id(8, 1);
                g_world_unit_graphics_load_pending = 1;
                do {
                    world_thread_yield();
                } while (g_world_unit_graphics_load_pending != 0);
                world_thread_yield();
                world_thread_yield();
            }
            if (operand_1 == EVENT_CALL_FUNCTION_RESET_BETHLA_REGION_UNIT_GRAPHICS) {
                region_misc_id = 0;
                do {
                    if (battle_unit_has_misc_id(region_misc_id) != 0) {
                        operand_1 = battle_unit_get_battle_index_by_misc_id(region_misc_id);
                        if (operand_1 != -1U) {
                            parameters = battle_unit_get_stats_from_battle_id((s32)operand_1);
                            if (((u8)((battle_stats_t*)parameters)->x < 4U)
                                && (((battle_stats_t*)parameters)->position.bits.y < 0xBU)) {
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
                parameters = battle_unit_get_stats_from_battle_id(1);
                copy_destination = battle_unit_get_stats_from_battle_id(7);
                copy_destination->x = (u8)((battle_stats_t*)parameters)->x;
                copy_destination->position.bits.y = ((battle_stats_t*)parameters)->position.bits.y;
                copy_destination->position.bits.higher_elevation
                    = ((battle_stats_t*)parameters)->position.bits.higher_elevation;
                copy_destination->position.bits.facing = ((battle_stats_t*)parameters)->position.bits.facing;
                battle_gfx_load_unit_graphics_by_battle_id(7, 1);
                g_world_unit_graphics_load_pending = 1U;
                do {
                    world_thread_yield();
                } while (g_world_unit_graphics_load_pending != 0);
                world_thread_yield();
                world_thread_yield();
            }
            if ((u32)(operand_1 - EVENT_CALL_FUNCTION_WARP_UNIT_4_DISPLAY_TO_UNIT_2_POSITION_AND_FACING) < 2U) {
                world_script_warp_unit_display_to_paired_unit((s32)operand_1);
            }
            if (operand_1 == EVENT_CALL_FUNCTION_CLEAR_ZALERA_ACTIVE_STATUS_GRAPHICS) {
                zalera_misc_id = (s16)world_get_misc_id(CHARACTER_IDENTITY_ZALERA);
                if (zalera_misc_id != EVENT_MISC_ID_NONE) {
                    world_unit_apply_zalera_draw_status_flags(
                        battle_unit_get_battle_index_by_misc_id(zalera_misc_id), 0);
                    g_world_thread_call_target = (void (*)(void))battle_unit_update_graphics_by_misc_id_wrapper;
                    world_thread_call_on_main_stack(zalera_misc_id);
                }
            }
        } else if (opcode == EVENT_OPCODE_BLUE_REMOVE_UNIT) {
            world_script_blue_remove_unit(first_halfword);
        } else if ((opcode == EVENT_OPCODE_END) || (opcode == EVENT_OPCODE_END_2)) {
            world_map_wait_for_refresh();
            do {
                world_thread_yield();
            } while (world_thread_find_running_by_task(NATIVE_THREAD_TASK_EVENT_BLOCK) != 0);
            do {
                world_thread_yield();
            } while (main_file_is_still_loading() != 0);
            world_thread_yield();
            if (world_script_check_tutorial_event_slot() != 0) {
                world_gfx_ramp_screen_overlay_intensity_to_max();
                g_main_game_options.value = g_world_saved_game_options;
                world_script_set_variable(EVENT_SCRIPT_VAR_SUPPRESS_PROGRESS_EFFECTS, 0);
                battle_state_start_close_battle(0x20);
            }
            if (g_world_menu_input_disabled == 0) {
                for (finishing_thread_index = 2; finishing_thread_index < 0xE; finishing_thread_index++) {
                    world_thread_wait_until_inactive(finishing_thread_index);
                }
            }
            operand_1 = world_script_get_variable(EVENT_SCRIPT_VAR_CURRENT_EVENT);
            if (operand_1 != 0x1AD) {
                if (operand_1 != 0x1D1) {
                    battle_gfx_init_evtchr_vram_slots();
                }
            }
            world_script_set_variable(EVENT_SCRIPT_VAR_PENDING_STAGED_STATUS, 0);
            event_flags = (g_scenario_event_finish_operations[operand_1] & ~0x0CFF) >> 8;
            if (g_world_menu_input_disabled == 0 && event_flags == 0 && (u32)(operand_1 - 0x190) >= 0x1AU
                && operand_1 != 0) {
                g_world_script_acting_unit_id = 0xFF;
                world_process_scenario_conditionals();
                if (world_script_load_next_event() != 0) {
                    goto restart_event;
                }
            }
            facing_misc_id = 0;
            battle_unit_snap_all_facings_to_quadrants();
            world_unit_update_staged_status_data(0, 1);
            do {
                if (battle_unit_has_misc_id(facing_misc_id) != 0) {
                    operand_1 = battle_unit_get_battle_index_by_misc_id(facing_misc_id);
                    if (operand_1 >= 0) {
                        parameters = battle_unit_get_stats_from_battle_id(operand_1);
                        ((battle_stats_t*)parameters)->position.bits.facing
                            = battle_unit_get_facing_quadrant_by_misc_id(facing_misc_id);
                    }
                }
                facing_misc_id += 1;
            } while (facing_misc_id < EVENT_UNIT_SLOT_COUNT);
            world_camera_init_yaw_angle();
            world_camera_init_tilt_and_zoom();
            world_thread_exit_current();
        }
        /* ---- Flow control: forward and backward jumps ---- */
        else if (opcode == EVENT_OPCODE_UNKNOWN_DA) {
        } else if (opcode == EVENT_OPCODE_JUMP_FORWARD_IF_ZERO) {
            if (g_world_script_variables[EVENT_SCRIPT_VAR_COMPARISON_RESULT] == 0) {
                instruction_offset = world_script_find_jump_target(
                    instruction_offset + 2, EVENT_OPCODE_FORWARD_TARGET, EVENT_OPCODE_UNKNOWN_D4, operand_1);
                continue;
            }
        } else if (opcode == EVENT_OPCODE_JUMP_FORWARD) {
            instruction_offset
                = world_script_find_jump_target(instruction_offset + 2, EVENT_OPCODE_FORWARD_TARGET, -1, operand_1);
            continue;
        } else if (opcode == EVENT_OPCODE_FORWARD_TARGET) {
        } else if (opcode == EVENT_OPCODE_UNKNOWN_D4) {
            break;
        } else if (opcode == EVENT_OPCODE_JUMP_BACK) {
            instruction_offset
                = world_script_find_jump_target(instruction_offset, EVENT_OPCODE_BACK_TARGET, -1, operand_1);
            continue;
        } else if (opcode == EVENT_OPCODE_BACK_TARGET) {
        } else if (opcode == EVENT_OPCODE_UNKNOWN_D8) {
        } else if (opcode == EVENT_OPCODE_UNKNOWN_D9) {
        } else if (opcode == EVENT_OPCODE_UNKNOWN_DC) {
            *(s16*)&g_world_text_substitution_values[0x2f] = 0xD;
        }
        /* ---- Dialogue and portraits ---- */
        else if (opcode == EVENT_OPCODE_DISPLAY_MESSAGE) {
            operand_1 = world_thread_resolve_id(operand_1);
            world_thread_start(operand_1, world_text_character_handling_thread);
            world_text_build_display_message(
                instruction_offset, operand_2, &g_world_thread_contexts[operand_1].task_words[6]);
            g_world_thread_contexts[operand_1].function_parameter_1 = (s32)operand_2;
            g_world_thread_contexts[operand_1].function_parameter_2 = (s32)(((operand_4 << 8) + operand_3) - 1);
            dialogue_instruction = (u8*)g_world_event_script;
            dialogue_instruction += instruction_offset;
            operand_4 = ((u8*)dialogue_instruction)[6];
            operand_3 = ((u8*)dialogue_instruction)[5];
            g_world_thread_contexts[operand_1].function_parameter_3 = (s32)((operand_4 << 8) + operand_3);
            g_world_thread_contexts[operand_1].function_parameter_4 = (s32)((u8*)dialogue_instruction)[7];
            g_world_thread_contexts[operand_1].task_words[2]
                = (s32)world_script_load_halfword(dialogue_instruction + 8);
            g_world_thread_contexts[operand_1].task_words[3]
                = (s32)world_script_load_halfword(instruction_offset + ((u32)g_world_event_script) + 0xA);
            g_world_thread_contexts[operand_1].task_words[4]
                = (s32)world_script_load_halfword(instruction_offset + ((u32)g_world_event_script) + 0xC);
            g_world_thread_contexts[operand_1].task_words[5]
                = (s32)((u8*)(((u32)g_world_event_script) + instruction_offset))[0xE];
            world_thread_yield();
        } else if (opcode == EVENT_OPCODE_CHANGE_DIALOG) {
            for (work_index = 0; work_index < 6; work_index += 2) {
                operand_2 = g_world_menu_window_buffers[work_index].thread_id;
                if ((g_world_menu_window_buffers[work_index].dialogue_selector == operand_1)
                    && (world_thread_is_running_80100164(operand_2) == 1)
                    && (g_world_thread_contexts[operand_2].task_id == NATIVE_THREAD_TASK_DIALOG_AWAIT_TEXT)) {
                    operand_1 = world_script_load_halfword(instruction_offset + ((u32)g_world_event_script) + 2);
                    if ((operand_1 & 0xFFFF) != 0xFFFF) {
                        operand_1 -= 1;
                    }
                    g_world_thread_contexts[operand_2].function_parameter_2 = (s32)operand_1;
                    g_world_thread_contexts[operand_2].function_parameter_4
                        = (s32)world_script_load_halfword(instruction_offset + ((u32)g_world_event_script) + 4);
                    g_world_thread_contexts[operand_2].task_id = NATIVE_THREAD_TASK_RESUME;
                    break;
                }
            }
        } else if (opcode == EVENT_OPCODE_PORTRAIT_COL) {
            world_script_load_portrait_colors_event_instruction(operand_1);
        }
        /* ---- Map objects ---- */
        else if (opcode == EVENT_OPCODE_USE_3D_OBJECT) {
            g_world_3d_object_use_request = 1;
            g_world_event_map_command_80_args.arg1 = (s16)operand_1;
            g_world_event_map_command_80_args.arg2 = (s16)operand_2;
            world_thread_yield();
        } else if (opcode == EVENT_OPCODE_WAIT_3D_OBJECT) {
            g_world_3d_object_wait_status = 1;
            do {
                world_thread_yield();
            } while (g_world_3d_object_wait_status != 0);
        } else if (opcode == EVENT_OPCODE_USE_FIELD_OBJECT) {
            g_world_field_object_use_request = 1;
            g_world_event_map_command_83_args.arg1 = (s16)operand_1;
            g_world_event_map_command_83_args.arg2 = (s16)operand_2;
            world_thread_yield();
        } else if (opcode == EVENT_OPCODE_WAIT_FIELD_OBJECT) {
            g_world_field_object_wait_status = 1;
            do {
                world_thread_yield();
            } while (g_world_field_object_wait_status != 0);
        }
        /* ---- unit_t sprites and animation ---- */
        else if (opcode == EVENT_OPCODE_MIRROR_SPRITE) {
            world_script_mirrorsprite_event_instruction(parameters);
        } else if (opcode == EVENT_OPCODE_RESET_PALETTE) {
            world_gfx_refresh_script_unit_environment_palette(parameters);
        } else if (opcode == EVENT_OPCODE_SET_FIRST_MISC_UNIT) {
            operand_1 = world_get_misc_id(first_halfword);
            if (operand_1 != EVENT_MISC_ID_NONE) {
                battle_unit_move_misc_unit_to_head(operand_1);
            }
        } else if (opcode == EVENT_OPCODE_UNIT_ANIM) {
            world_script_unit_anim(parameters);
        } else if (opcode == EVENT_OPCODE_MARCH) {
            world_script_march_units(parameters);
        } else if (opcode == EVENT_OPCODE_UNKNOWN_81) {
            world_script_set_units_movement_effect_suppression(parameters);
        } else if (opcode == EVENT_OPCODE_UNKNOWN_12) {
            operand_1 = world_get_misc_id(first_halfword);
            if (operand_1 != EVENT_MISC_ID_NONE) {
                world_script_wait_for_unit_ready(operand_1);
            }
        }
        /* ---- Map state, weather, and time of day ---- */
        else if (opcode == EVENT_OPCODE_CHANGE_MAP_BETA) {
            g_world_event_pending_map_jump_out_id = first_halfword;
            world_thread_wait_frames(2);
            world_noop_800f29d8();
        } else if (opcode == EVENT_OPCODE_CHANGE_MAP) {
            g_world_event_pending_map_jump_out_2_id = first_halfword;
            world_thread_wait_frames(2);
            world_noop_800f29d8();
        } else if (opcode == EVENT_OPCODE_SET_DAYTIME) {
            g_world_event_pending_map_state = (s16)operand_1;
            world_thread_wait_frames(2);
            world_map_wait_for_refresh();
        } else if (opcode == EVENT_OPCODE_WEATHER) {
            g_world_event_weather_request = first_halfword;
            world_thread_wait_frames(2);
            world_map_wait_for_refresh();
        } else if (opcode == EVENT_OPCODE_RELOAD_MAP_STATE) {
            g_world_map_reload_state = 1;
            do {
                world_thread_yield();
            } while (g_world_map_reload_state != 0);
        } else if (opcode == EVENT_OPCODE_APPEND_MAP_STATE) {
            g_world_map_append_state = 1;
            do {
                world_thread_yield();
            } while (g_world_map_append_state != 0);
        } else if (opcode == EVENT_OPCODE_PAUSE) {
            world_script_pause_event_instruction();
        } else if (opcode == EVENT_OPCODE_UNKNOWN_26) {
            for (work_index = 1; work_index < 17; ++work_index) {
                cancel_thread = &g_world_thread_contexts[work_index];
                if (cancel_thread->is_running && cancel_thread->task_id == NATIVE_THREAD_TASK_WAIT_FOR_RESUME
                    && cancel_thread->function_parameter_2 == first_halfword) {
                    cancel_thread->task_id = NATIVE_THREAD_TASK_RESUME;
                }
            }
        } else if (opcode == EVENT_OPCODE_UNKNOWN_37) {
            world_script_toggle_message_portrait_flip(parameters);
        } else if (opcode == EVENT_OPCODE_UNKNOWN_17) {
        }
        /* ---- Effects ---- */
        else if (opcode == EVENT_OPCODE_EFFECT) {
            g_world_event_pending_effect_id = first_halfword;
            g_world_event_effect_target_misc_id
                = world_get_misc_id((s16)((u8*)(((u32)g_world_event_script) + instruction_offset))[3]);
            focus_instruction = (u8*)g_world_event_script;
            focus_instruction += instruction_offset;
            g_world_event_effect_target_x = (s16)((u8*)focus_instruction)[4];
            g_world_event_effect_target_y = (s16)((u8*)focus_instruction)[5];
            g_world_event_effect_target_mode = (s16)((u8*)focus_instruction)[6];
            world_script_set_variable(EVENT_SCRIPT_VAR_CAMERA_ROTATION, 0);
            world_thread_wait_frames(2);
        } else if (opcode == EVENT_OPCODE_EFFECT_START) {
            do {
                world_thread_yield();
                g_world_thread_call_target = (void (*)(void))battle_effect_try_init_data;
            } while (world_thread_call_on_main_stack() != 0);
        } else if (opcode == EVENT_OPCODE_EFFECT_END) {
            world_thread_start(world_thread_resolve_id(0x10U), &world_script_play_effect_thread);
        }
        /* ---- Camera ---- */
        else if (opcode == EVENT_OPCODE_FOCUS) {
            world_camera_store_yaw_band(g_world_script_variables[EVENT_SCRIPT_VAR_CAMERA_YAW]);
            world_script_focus(parameters);
        } else if (opcode == EVENT_OPCODE_FOCUS_SPEED) {
            event_words = g_world_script_variables;
            world_script_focus_speed(parameters, event_words + 26, event_words + 29);
        } else if (opcode == EVENT_OPCODE_UNKNOWN_73) {
            world_script_apply_relative_camera(parameters, g_world_script_variables + 26);
        } else if (opcode == EVENT_OPCODE_CAMERA) {
            operand_1 = world_thread_resolve_id(0x10U);
            world_thread_start(operand_1, world_camera_thread);
            g_world_thread_contexts[operand_1].function_parameter_1 = (s32)parameters;
        } else if (opcode == EVENT_OPCODE_CAMERA_FUSION_START) {
            operand_1 = world_thread_resolve_id(0x10U);
            world_thread_start(operand_1, world_camera_fusion_thread);
            g_world_thread_contexts[operand_1].function_parameter_1 = (s32)parameters;
            instruction_offset
                = world_script_find_instruction_byte_offset(instruction_offset, EVENT_OPCODE_CAMERA_FUSION_END);
        } else if (opcode == EVENT_OPCODE_CAMERA_FUSION_END) {
        } else if (opcode == EVENT_OPCODE_CAMERA_SPEED_CURVE) {
            g_world_camera_speed_curve = (s32)operand_1;
        } else if (opcode == EVENT_OPCODE_UNKNOWN_74) {
            world_noop_800fd074(parameters);
        }
        /* ---- unit_t movement and facing ---- */
        else if (opcode == EVENT_OPCODE_WALK_TO) {
        } else if (opcode == EVENT_OPCODE_JUMP) {
            world_script_jump_event_instruction(parameters);
        } else if (opcode == EVENT_OPCODE_WAIT_WALK) {
            world_script_waitwalk_event_instruction(first_halfword);
        } else if (opcode == EVENT_OPCODE_WALK_TO_ANIM) {
            operand_1 = world_get_misc_id(first_halfword);
            if (operand_1 != EVENT_MISC_ID_NONE) {

                g_world_unit_pending_animations[operand_1]
                    = world_script_load_halfword(instruction_offset + ((u32)g_world_event_script) + 3);
                g_world_unit_was_moving_latch[operand_1] = 0;
            }
        } else if (opcode == EVENT_OPCODE_WAIT_ALL_UNIT_MOVEMENT) {
            do {
                world_thread_yield();
            } while (world_unit_is_moving(-1) != 0);
        } else if (opcode == EVENT_OPCODE_WAIT_FILE_LOAD) {
            do {
                world_thread_yield();
            } while (main_file_is_still_loading() != 0);
        } else if (opcode == EVENT_OPCODE_ROTATE_UNIT) {
            world_rotate_unit(parameters);
        } else if (opcode == EVENT_OPCODE_FACE_UNIT_2) {
            world_script_face_unit(parameters, 0);
        } else if (opcode == EVENT_OPCODE_FACE_UNIT) {
            world_script_face_unit(parameters, 1);
        } else if (opcode == EVENT_OPCODE_FACE_TILE) {
            world_script_face_tile(parameters);
        } else if (opcode == EVENT_OPCODE_WAIT_ROTATE_UNIT) {
            world_script_waitrotateunit_and_waitrotateall_event_instruction(first_halfword);
        } else if (opcode == EVENT_OPCODE_WAIT_ROTATE_ALL) {
            world_script_waitrotateunit_and_waitrotateall_event_instruction(-1);
        }
        /* ---- Blocks, earthquakes, darkness, and screen color ---- */
        else if (opcode == EVENT_OPCODE_BLOCK_START) {
            operand_1 = world_thread_resolve_id(0x10U);
            world_thread_start(operand_1, world_block_start_thread);
            g_world_thread_contexts[operand_1].function_parameter_1 = (s32)parameters;
            instruction_offset = world_script_find_instruction_byte_offset(instruction_offset, EVENT_OPCODE_BLOCK_END);
        } else if (opcode == EVENT_OPCODE_BLOCK_END) {
        } else if (opcode == EVENT_OPCODE_UNKNOWN_23) {
        } else if (opcode == EVENT_OPCODE_UNKNOWN_24) {
        } else if (opcode == EVENT_OPCODE_EARTHQUAKE_END) {
            world_thread_wait_frames(1);
            world_thread_suspend(world_thread_find_running_by_task(NATIVE_THREAD_TASK_EARTHQUAKE));
        } else if (opcode == EVENT_OPCODE_UNKNOWN_25) {
            g_world_text_speaker_swap_unit_a = first_halfword;
            g_world_text_speaker_swap_unit_b
                = world_script_load_halfword(instruction_offset + ((u32)g_world_event_script) + 3);
        } else if (opcode == EVENT_OPCODE_MAP_DARKNESS) {
            operand_1 = world_thread_resolve_id(0x10U);
            world_thread_start(operand_1, &world_script_map_darkness);
            g_world_thread_contexts[operand_1].function_parameter_1 = (s32)parameters;
        } else if (opcode == EVENT_OPCODE_COLOR_SCREEN) {
            operand_1 = world_thread_resolve_id(0x10U);
            world_thread_start(operand_1, world_script_color_screen_thread);
            g_world_thread_contexts[operand_1].function_parameter_1 = (s32)parameters;
        } else if (opcode == EVENT_OPCODE_EARTHQUAKE_START) {
            operand_1 = world_thread_resolve_id(0x10U);
            world_thread_start(operand_1, &world_script_earthquake_start);
            g_world_thread_contexts[operand_1].function_parameter_1 = (s32)parameters;
        } else if (opcode == EVENT_OPCODE_SPRITE_MOVE || opcode == EVENT_OPCODE_SPRITE_MOVE_BETA) {
            operand_1 = world_thread_resolve_id_after_current(0x10);
            if (opcode == EVENT_OPCODE_SPRITE_MOVE) {
                world_thread_start(operand_1, &world_script_sprite_move);
            } else {
                world_thread_start(operand_1, &world_script_sprite_move_beta);
            }
            g_world_thread_contexts[operand_1].function_parameter_1 = (s32)parameters;
            g_world_thread_contexts[operand_1].task_id = NATIVE_THREAD_TASK_SPRITE_MOVE;
            g_world_thread_contexts[operand_1].task_words[0] = world_get_misc_id(first_halfword);
        } else if (opcode == EVENT_OPCODE_WAIT_SPRITE_MOVE) {
            world_script_waitspritemove_event_instruction(first_halfword);
        } else if (opcode == EVENT_OPCODE_MAP_LIGHT) {
            operand_1 = world_thread_resolve_id(0x10U);
            world_thread_start(operand_1, world_map_light_thread);
            g_world_thread_contexts[operand_1].function_parameter_1 = (s32)parameters;
        }
        /* ---- Background, palettes, and terrain ---- */
        else if (opcode == EVENT_OPCODE_BACKGROUND) {
            world_script_copy_bytes(&g_world_event_background_colors.first, parameters, 3);
            world_script_copy_bytes(&g_world_event_background_colors.second, parameters + 3, 3);
            if (((u8*)parameters)[7] == 0) {
                battle_map_transition_and_store_background_gradient(
                    ((u8*)parameters)[6], (const u8*)&g_world_event_background_colors);
            } else {
                battle_map_set_background_gradient(((u8*)parameters)[6], &g_world_event_background_colors);
            }
        } else if (opcode == EVENT_OPCODE_COLOR_BG_BETA) {
            battle_map_modify_background_gradient(((u8*)parameters)[0], ((u8*)parameters)[4], (s8)((u8*)parameters)[1],
                (s8)((u8*)parameters)[2], (s32)(s8)((u8*)parameters)[3]);
        } else if (opcode == EVENT_OPCODE_COLOR_UNIT) {
            world_script_color_units(parameters);
        } else if (opcode == EVENT_OPCODE_UNKNOWN_6C) {
            world_script_set_units_palette_update_suppression(first_halfword & 0xFFFF, 0);
        } else if (opcode == EVENT_OPCODE_UNKNOWN_6D) {
            world_script_set_units_palette_update_suppression(first_halfword & 0xFFFF, 1);
        } else if (opcode == EVENT_OPCODE_COLOR_FIELD) {
            battle_map_color_field(((u8*)parameters)[0], ((u8*)parameters)[4], (s8)((u8*)parameters)[1],
                (s8)((u8*)parameters)[2], (s32)(s8)((u8*)parameters)[3]);
        } else if (opcode == EVENT_OPCODE_UNKNOWN_66) {
            battle_map_store_modified_palette_colors(0, 0, 1);
        } else if (opcode == EVENT_OPCODE_SET_TILE_CAMERA_BLOCK) {
            battle_map_set_tile_data_value(MAP_TILE_DATA_FLAT_CAMERA_BLOCK_MASK, ((u8*)parameters)[0],
                (u8)(s8)((u8*)parameters)[1], (u8)(s8)((u8*)parameters)[2], (s32)(u8)(s8)((u8*)parameters)[3]);
        } else if (opcode == EVENT_OPCODE_UNKNOWN_40) {
            tile = battle_map_get_tile_data_pointer(
                ((u8*)parameters)[0], (u8)(s8)((u8*)parameters)[1], (u8)(s8)((u8*)parameters)[2]);
            if ((u32)(((map_tile_t*)tile)->surface.bits.type - MAP_SURFACE_GRASSLAND) >= 2U) {
                ((map_tile_t*)tile)->surface.value
                    = (u8)((((map_tile_t*)tile)->surface.value & ~MAP_SURFACE_MASK) | MAP_SURFACE_STONE_FLOOR);
            }
            ((map_tile_t*)tile)->height = (u8)(s8)((u8*)parameters)[3];
            ((map_tile_t*)tile)->flags_06.bits.untargetable = ((u8*)parameters)[4];
            ((map_tile_t*)tile)->flags_06.bits.blocked = 0;
        }
        /* ---- Event speed, dark screen, conditions, and graphics ---- */
        else if (opcode == EVENT_OPCODE_EVENT_SPEED) {
            world_script_set_event_speed(operand_1);
        } else if (opcode == EVENT_OPCODE_DARK_SCREEN) {
            g_world_menu_hide_numeric_values = 0;
            operand_1 = world_thread_resolve_id(0x10U);
            world_thread_start(operand_1, &world_thread_set_task_id_36);
            world_thread_set_parameters(operand_1, 0, (s32)parameters, 0);
        } else if (opcode == EVENT_OPCODE_REMOVE_DARK_SCREEN) {
            operand_1 = world_thread_find_running_by_task(NATIVE_THREAD_TASK_DARK_SCREEN_HOLD);
            if (operand_1 != 0) {
                g_world_thread_contexts[operand_1].task_id = NATIVE_THREAD_TASK_DARK_SCREEN;
            }
        } else if (opcode == EVENT_OPCODE_DISPLAY_CONDITIONS) {
            operand_3 = world_thread_resolve_id(0x10U);
            if ((operand_3 == 4) && (operand_1 == 7)) {
                world_thread_wait_until_inactive(2);
                operand_3 = 2;
            }
            /* Keep the cached second operand as an allocator input at this join;
             * otherwise GCC exchanges the target s5/s6 operand lifetimes. */
            __asm__("" : : "r"(operand_2));
            world_thread_start(operand_3, &world_script_execute_display_conditions_instruction);
            world_thread_set_parameters(operand_3, operand_1, operand_2, 0);
        } else if (opcode == EVENT_OPCODE_SHOW_GRAPHIC) {
            operand_3 = world_thread_resolve_id(0x10U);
            world_thread_start(operand_3, &world_script_show_graphic);
            world_thread_set_parameters(operand_3, operand_1, 0, 0);
        }
        /* ---- Sound and music ---- */
        else if (opcode == EVENT_OPCODE_SOUND_EFFECT) {
            g_world_sound_effect_id_to_play = (s32)first_halfword;
        } else if (opcode == EVENT_OPCODE_BG_SOUND) {
            operand_1 = world_thread_resolve_id(0x10U);
            world_thread_start(operand_1, world_sound_bg_thread);
            world_thread_set_parameters(operand_1, (s32)parameters, 0, 0);
        } else if (opcode == EVENT_OPCODE_EDIT_BG_SOUND) {
            operand_1 = world_thread_resolve_id(0x10U);
            world_thread_start(operand_1, world_sound_edit_bg_thread);
            world_thread_set_parameters(operand_1, (s32)parameters, 0, 0);
        } else if (opcode == EVENT_OPCODE_SWITCH_TRACK) {
            g_world_sound_music_switch_request = (s16)operand_1;
            g_world_event_music_switch.volume = (s16)operand_2;
            g_world_event_music_switch.fade = (s16)operand_3;
            world_thread_yield();
        } else if (opcode == EVENT_OPCODE_FADE_SOUND) {
            /* The target MIPS variable shift uses the low five count bits.
             * Explicit C masking adds an instruction; counts >=32 retain the
             * original C shift limitation rather than an invented input restriction. */
            g_world_sound_music_volume_transition_request = operand_2 << (operand_1 + 0x10);
            world_thread_yield();
        } else if (opcode == EVENT_OPCODE_END_TRACK) {
            g_world_sound_music_unload_slot_request = (s16)operand_1;
            world_thread_yield();
        }
        /* ---- EVTCHR and reserved VRAM slots ---- */
        else if (opcode == EVENT_OPCODE_LOAD_EVTCHR) {
            operand_2 = world_thread_resolve_id(0x10U);
            world_thread_start(operand_2, world_gfx_load_evtchr_thread);
            world_thread_set_parameters(operand_2, operand_1,
                world_script_load_halfword(instruction_offset + ((u32)g_world_event_script) + 2), 0);
            world_thread_yield();
        } else if (opcode == EVENT_OPCODE_LOAD_EVTCHR_CLEAR) {
            g_world_event_pending_loaded_evtchr_clear_slot = (s32)operand_1;
            world_thread_yield();
        } else if (opcode == EVENT_OPCODE_SAVE_EVTCHR) {
            g_world_event_pending_evtchr_save_slot = (s32)operand_1;
            world_thread_yield();
        } else if (opcode == EVENT_OPCODE_SAVE_EVTCHR_CLEAR) {
            g_world_event_pending_saved_evtchr_clear_slot = (s32)operand_1;
            world_thread_yield();
        } else if (opcode == EVENT_OPCODE_COPY_UNIT_GRAPHICS_TO_RESERVED_VRAM_SLOT) {
            g_world_event_pending_unit_vram_copy = ((u32)world_get_misc_id(first_halfword) << 8) | operand_3;
            world_thread_yield();
        } else if (opcode == EVENT_OPCODE_RELEASE_RESERVED_VRAM_SLOT) {
            g_world_event_pending_reserved_vram_release_slot = (s32)operand_1;
            world_thread_yield();
        } else if (opcode == EVENT_OPCODE_UNKNOWN_E0) {
        }
        /* ---- Adding and removing units ---- */
        else if (opcode == EVENT_OPCODE_REMOVE_UNIT) {
            s16 normalized_unit_id = world_get_misc_id(first_halfword);
            if (normalized_unit_id != EVENT_MISC_ID_NONE) {
                removed_misc_id = normalized_unit_id;
                while (world_unit_is_moving(removed_misc_id) != 0) {
                    world_thread_yield();
                }
                if (removed_misc_id != EVENT_MISC_ID_NONE) {
                    operand_1 = battle_unit_get_battle_index_by_misc_id(removed_misc_id);
                    battle_unit_remove_misc_by_id(removed_misc_id);
                    if (operand_1 >= 0) {
                        battle_unit_set_cannot_exist(operand_1);
                    }
                }
            }
        } else if (opcode == EVENT_OPCODE_DRAW) {
            zalera_misc_id = (s16)world_get_misc_id(first_halfword);
            if (zalera_misc_id != EVENT_MISC_ID_NONE) {
                battle_unit_animate_and_set_enemy_level_data_by_misc_id(zalera_misc_id);
                if (world_script_get_variable(EVENT_SCRIPT_VAR_CURRENT_EVENT) == 0x18C) {
                    world_unit_apply_zalera_draw_status_flags(
                        battle_unit_get_battle_index_by_misc_id(zalera_misc_id), 1);
                    g_world_thread_call_target = (void (*)(void))battle_unit_update_graphics_by_misc_id_wrapper;
                    world_thread_call_on_main_stack(zalera_misc_id);
                }
            }
        } else if (opcode == EVENT_OPCODE_ERASE) {
            erased_misc_id = (s16)world_get_misc_id(first_halfword);
            if (erased_misc_id != EVENT_MISC_ID_NONE) {
                battle_gfx_reset_unit_graphic_trigger(erased_misc_id);
            }
        } else if (opcode == EVENT_OPCODE_ADD_GHOST_UNIT) {
            ghost_operands = instruction_offset + ((u32)g_world_event_script) + 3;
            ghost_misc_id = 3;
            do {
                --ghost_misc_id;
            } while (ghost_misc_id >= 0);
            for (ghost_misc_id = 0; ghost_misc_id < EVENT_UNIT_SLOT_COUNT; ghost_misc_id++) {
                if (battle_unit_has_misc_id(ghost_misc_id) == 0) {
                    g_world_event_unit_slots[((u8*)ghost_operands)[0]] = ghost_misc_id;
                    break;
                }
            }
            if (ghost_misc_id != EVENT_UNIT_SLOT_COUNT) {
                battle_script_add_ghost_unit_event_instruction(((u8*)ghost_operands)[1], ((u8*)ghost_operands)[2],
                    ((u8*)ghost_operands)[3], ((u8*)ghost_operands)[4], (s32)first_halfword,
                    (s32)g_world_event_unit_slots[((u8*)ghost_operands)[0]], (s32)((u8*)ghost_operands)[5]);
                g_world_unit_graphics_load_pending = 1U;
            } else {
                continue;
            }
        } else if (opcode == EVENT_OPCODE_UNKNOWN_75) {
        } else if (opcode == EVENT_OPCODE_ADD_UNIT) {
            world_noop_800f6f18();
            appearance_unit = battle_find_unit_data_pointer_for_entd_unit_id(first_halfword, &unit_lookup_status);
            if ((unit_lookup_status == -2) || (unit_lookup_status >= 0)) {
                for (appearance_battle_id = 0; appearance_battle_id < EVENT_UNIT_SLOT_COUNT; appearance_battle_id++) {
                    if (battle_unit_get_stats_from_battle_id(appearance_battle_id) == appearance_unit) {
                        break;
                    }
                }
                operand_1 = battle_unit_get_misc_id_by_battle_id(appearance_battle_id);
                if (operand_1 == -1) {
                    world_update_unit_status_and_staged_status_data(appearance_battle_id);
                    battle_gfx_load_unit_graphics_by_battle_id(appearance_battle_id, operand_3);
                    g_world_unit_graphics_load_pending = 1U;
                } else {
                    if (operand_3 == 0) {
                        battle_unit_animate_and_set_enemy_level_data_by_misc_id(operand_1);
                    }
                    if (world_update_unit_status_and_staged_status_data(appearance_battle_id) != 0) {
                        g_world_thread_call_target = (void (*)(void))battle_unit_update_graphics_by_misc_id_wrapper;
                        world_thread_call_on_main_stack(operand_1);
                    }
                }
            }
        } else if (opcode == EVENT_OPCODE_WARP_UNIT) {
            world_script_warp_unit(parameters);
        } else if (opcode == EVENT_OPCODE_UNKNOWN_61) {
        } else if (opcode == EVENT_OPCODE_SET_BATTLE_UNIT_POSITION) {
            parameters = battle_unit_get_stats_from_battle_id((s32)operand_1);
            position_instruction = instruction_offset + ((u32)g_world_event_script);
            ((battle_stats_t*)parameters)->x = (u8)((u8*)position_instruction)[3];
            ((battle_stats_t*)parameters)->position.bits.y = (u8)((u8*)position_instruction)[4];
            ((battle_stats_t*)parameters)->position.bits.higher_elevation = ((u8*)position_instruction)[5];
            ((battle_stats_t*)parameters)->position.bits.facing = ((u8*)position_instruction)[6];
        } else if (opcode == EVENT_OPCODE_WAIT_ADD_UNIT) {
            do {
                world_thread_yield();
            } while (g_world_unit_graphics_load_pending != 0);
        } else if (opcode == EVENT_OPCODE_WAIT_ADD_UNIT_END) {
            do {
                world_thread_yield();
            } while (world_thread_find_running_by_task(NATIVE_THREAD_TASK_ADD_UNIT) != 0);
        } else if (opcode == EVENT_OPCODE_ADD_UNIT_START) {
            operand_1 = world_thread_resolve_id(0x10U);
            world_thread_start(operand_1, world_script_add_unit_start_thread);
            g_world_thread_contexts[operand_1].function_parameter_1 = (s32)parameters;
            instruction_offset
                = world_script_find_instruction_byte_offset(instruction_offset, EVENT_OPCODE_ADD_UNIT_END);
        } else if (opcode == EVENT_OPCODE_ADD_UNIT_END) {
        }
        /* ---- Waits and synchronization ---- */
        else if (opcode == EVENT_OPCODE_WAIT_FOR_INSTRUCTION) {
            /* Yield until no thread runs the named task. The outer wait stays
             * a label loop: as a C loop its notes raise operand_3's
             * allocation weight and swap the target's s5/s6. */
            operand_3 = (operand_2 << 8) + operand_1;
            parameters = (u8*)g_world_thread_contexts;
        wait_matching_task:
            waiting_thread_index = 1;
            world_thread_yield();
            for (;;) {
                /* Typed indexing reverses the target addu operands; keep the 32-bit address sum. */
                waiting_thread
                    = (native_thread_t*)((u32)(waiting_thread_index * sizeof(native_thread_t)) + (u32)parameters);
                if ((waiting_thread->is_running == 0) || (waiting_thread->task_id != operand_3)) {
                    waiting_thread_index += 1;
                    if (waiting_thread_index < 0x11) {
                        continue;
                    }
                }
                break;
            }
            if (waiting_thread_index != 0x11) {
                goto wait_matching_task;
            }
        } else if (opcode == EVENT_OPCODE_YIELD) {
            world_thread_yield();
        } else if (opcode == EVENT_OPCODE_WAIT) {
            if (world_script_check_tutorial_event_slot() != 0 && first_halfword >= 3) {
                world_thread_wait_frames(first_halfword / battle_state_get_animation_speed());
            } else {
                world_thread_wait_frames(first_halfword);
            }
        } else if (opcode == EVENT_OPCODE_TUTORIAL_BUTTON_WAIT) {
            do {
                world_thread_yield();
            } while (!(first_halfword & g_world_input_unfiltered_controller));
        } else if (opcode == EVENT_OPCODE_END_SOUND) {
            main_sound_stop_weather_sfx_music();
        }
        /* ---- Miscellaneous unit and display commands ---- */
        else if (opcode == EVENT_OPCODE_UNIT_SHADOW) {
            shadow_misc_id = world_get_misc_id(first_halfword);
            if (shadow_misc_id != EVENT_MISC_ID_NONE) {
                if (operand_3 != 0) {
                    battle_gfx_clear_shadow_graphic_trigger_by_misc_id(shadow_misc_id);
                } else {
                    battle_gfx_set_shadow_graphic_trigger_by_misc_id(shadow_misc_id);
                }
            }
        } else if (opcode == EVENT_OPCODE_UNKNOWN_72) {
            world_script_print_debug_message();
        } else if (opcode == EVENT_OPCODE_WAIT_VALUE) {
            world_script_wait_for_value(parameters);
        } else if (opcode == EVENT_OPCODE_EVTCHR_PALETTE) {
            misc_id_for_7f = world_get_misc_id(first_halfword);
            if (misc_id_for_7f != EVENT_MISC_ID_NONE) {
                battle_gfx_set_evtchr_palette_by_misc_id(misc_id_for_7f, operand_3, operand_4);
            }
        } else if (opcode == EVENT_OPCODE_LOAD_ATTACK_GRAPHICS) {
            world_script_load_attack_graphics_event_instruction();
        } else if (opcode == EVENT_OPCODE_CHANGE_STATS) {
            world_script_change_stats(parameters);
        } else if (opcode == EVENT_OPCODE_PLAY_TUNE) {
            main_sound_play_tune(operand_1);
        } else if (opcode == EVENT_OPCODE_UNLOCK_DATE) {
            if ((world_script_get_variable(EVENT_SCRIPT_VAR_SUPPRESS_PROGRESS_EFFECTS) == 0) && (operand_1 != 0)) {
                month = world_script_get_variable(EVENT_SCRIPT_VAR_MONTH);
                world_script_unlockdate_event_instruction(g_main_treasure_acquisition_date_bits, operand_1 - 1, month,
                    world_script_get_variable(EVENT_SCRIPT_VAR_DAY));
            }
        } else if (opcode == EVENT_OPCODE_TEMP_WEAPON) {
            palette_misc_id = world_get_misc_id(first_halfword);
            if (palette_misc_id != EVENT_MISC_ID_NONE) {
                battle_gfx_set_thrown_item_palette_by_misc_id(operand_3, palette_misc_id);
            }
        } else if (opcode == EVENT_OPCODE_ARROW) {
            first_misc_id = world_get_misc_id(first_halfword);
            if (first_misc_id != EVENT_MISC_ID_NONE) {
                battle_unit_get_battle_index_by_misc_id(first_misc_id);
                second_misc_id = world_get_misc_id(
                    world_script_load_halfword(instruction_offset + ((u32)g_world_event_script) + 3));
                if (second_misc_id != EVENT_MISC_ID_NONE) {
                    battle_unit_get_battle_index_by_misc_id(second_misc_id);
                }
            }
        } else if (opcode == EVENT_OPCODE_MAP_FREEZE) {
            battle_map_freeze();
        } else if (opcode == EVENT_OPCODE_MAP_UNFREEZE) {
            battle_map_unfreeze();
        } else if (opcode == EVENT_OPCODE_UNIT_ANIM_ROTATE) {
            world_script_rotate_unit_animation(parameters);
        } else if (opcode == EVENT_OPCODE_WAIT_GRAPHIC_PRINT) {
            world_thread_yield();
            do {
                world_thread_yield();
                operand_1 = world_thread_find_running_by_task(NATIVE_THREAD_TASK_SHOW_GRAPHIC);
            } while (operand_1 != 0 && g_world_thread_contexts[operand_1].function_parameter_4 == 0);
        } else if (opcode == EVENT_OPCODE_LOCK_MENU_INPUT) {
            g_world_thread_task_active = (s32)operand_1;
        } else if (opcode == EVENT_OPCODE_TUTORIAL_COORDINATE_CONFIRM) {
            while (g_battle_cursor_x != operand_1 || g_battle_cursor_y != operand_2
                || ((initial_buttons = g_world_input_unfiltered_controller) & PSX_PAD_CIRCLE) == 0
                || (initial_buttons & PSX_PAD_DPAD_MASK) != 0) {
                world_thread_yield();
            }
        }
        /* ---- Map title, dates, teleports, and status ---- */
        else if (opcode == EVENT_OPCODE_SHOW_MAP_TITLE) {
            operand_4 = world_thread_resolve_id(0x10U);
            world_bin_load_file((void*)0xD);
            attack_map_load_title_graphic();
            world_thread_start(operand_4, attack_map_show_title);
            world_thread_set_parameters(operand_4, (s32)(s8)operand_1, (s32)(s8)operand_2, (s8)operand_3);
            world_thread_wait_until_inactive(operand_4);
        } else if (opcode == EVENT_OPCODE_SET_DATE_ADVANCE) {
            if (world_script_get_variable(EVENT_SCRIPT_VAR_SUPPRESS_PROGRESS_EFFECTS) == 0) {
                world_script_set_variable(EVENT_SCRIPT_VAR_DATE_ADVANCE, first_halfword);
            }
        } else if (opcode == EVENT_OPCODE_TELEPORT_OUT) {
            world_script_teleport_unit_out(first_halfword, 1);
        } else if (opcode == EVENT_OPCODE_TELEPORT_IN) {
            world_script_teleport_unit_in(first_halfword, 1);
        } else if (opcode == EVENT_OPCODE_INFLICT_STATUS) {
            operand_1 = world_thread_resolve_id(0x10U);
            world_thread_start(operand_1, world_script_inflict_status_thread);
            world_thread_set_parameters(operand_1, (s32)parameters, 0, 0);
        } else if (opcode == EVENT_OPCODE_UNKNOWN_95) {
        } else {
            break;
        }
        instruction_offset += 1 + g_world_event_instruction_sizes[opcode];
    }
    world_thread_exit_current();
}
