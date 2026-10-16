/* The event script interpreter that runs as the event thread. */

#include "fft/battle_map.h"
#include "fft/battle_map_light.h"
#include "fft/battle_map_load.h"
#include "fft/battle_map_mesh.h"
#include "fft/battle_map_palette.h"
#include "fft/brave_story.h"
#include "fft/calendar.h"
#include "fft/camera.h"
#include "fft/cursor.h"
#include "fft/effect_script.h"
#include "fft/file.h"
#include "fft/gfx_image.h"
#include "fft/gfx_primitive.h"
#include "fft/item.h"
#include "fft/memory.h"
#include "fft/menu_input.h"
#include "fft/menu_window.h"
#include "fft/message.h"
#include "fft/music.h"
#include "fft/option.h"
#include "fft/overlay.h"
#include "fft/party.h"
#include "fft/projectile.h"
#include "fft/script.h"
#include "fft/script_instruction.h"
#include "fft/script_variable.h"
#include "fft/sound.h"
#include "fft/sprite.h"
#include "fft/sprite_sheet.h"
#include "fft/state.h"
#include "fft/status.h"
#include "fft/system.h"
#include "fft/text.h"
#include "fft/thread.h"
#include "fft/tutorial.h"
#include "fft/unit.h"
#include "fft/unit_event.h"
#include "fft/unit_identity.h"
#include "fft/unit_panel.h"
#include "fft/unit_runtime.h"
#include "fft/unit_view.h"
#include "psx/pad.h"
#include "psx/types.h"

/* BATTLE event-script interpreter: the body of the event thread
 * (THREAD_SLOT_EVENT), entered from script_run_next_event once
 * script_load_next_event has read the event block.
 *
 * Setup: with player input it waits for thread slots 2..13 and the camera to
 * go idle (in the other input modes it moves the four unit-stats displays to
 * y 0xAA). It then clears the status staging data, snapshots every unit's
 * statuses while the battle has not started (otherwise it runs the staged
 * inflict-status commands), and clears the disabled-menu, disabled-input and
 * forced-input variables.
 *
 * Each pass fetches the opcode at instruction_offset in g_script_event_block
 * with its first four operand bytes and its first 16-bit operand,
 * dispatches through the else-if chain, then advances by
 * 1 + g_script_event_instruction_sizes[opcode]. The jumps `continue`
 * without the length step; the Start instructions (camera fusion, block, add
 * unit) hand their body to a new thread and seek to the matching End. Most
 * long-running instructions start a helper thread on the first idle slot with
 * a pointer to their operands; work that belongs to the main loop is posted
 * in request globals that script_process_pending_requests serves.
 *
 * EventEnd either chains into the event the scenario conditions select
 * (`goto restart_script`) or replays the staged statuses, settles the units'
 * facings and exits the thread. An opcode outside the chain, or a
 * ForwardTargetExit reached in sequence, also ends the thread.
 *
 * The chain order is retail's compare order. WORLD links its own body
 * under the same name. BATTLE differs in five places: WalkTo
 * starts the unit's movement and Arrow sets an arrow trajectory (WORLD
 * recognizes both and does nothing), DarkScreen's thread opens REQUIRE and
 * runs its image burst (WORLD's only takes the task id), the two ChangeMap
 * instructions reset the script camera transform, and the thread scans of
 * ResumeWaitingDialog and WaitForInstruction cover slots 1..15 where WORLD's
 * cover 1..16.
 *
 * thread_call_on_main_stack is declared without a parameter list: it
 * forwards whatever arguments its current call target takes. */
void script_execute_event(void) {
    s32 added_unit_state;
    u8* message_cursor;
    u8* effect_cursor;
    s32 finish_operation;
    s16 first_operand;
    s16 remove_unit_runtime_id;
    s16 erased_runtime_id;
    s16 shadow_runtime_id;
    s16 evtchr_palette_runtime_id;
    s16 thrown_item_runtime_id;
    s16 arrow_target_runtime_id;
    s16 arrow_shooter_runtime_id;
    s32 ghost_runtime_id;
    s32 region_runtime_id;
    s16 draw_runtime_id;
    s32 date_month;
    unit_stats_t* added_unit;
    s32 arrow_shooter_battle_id;
    s32 confirm_buttons;
    s32 instruction_offset;
    s32 added_battle_id;
    s32 work_index;
    event_opcode_e opcode;
    /* Initially four eagerly fetched operand bytes, then per-opcode scratch.
     * They are not four uniformly encoded script arguments. */
    s32 operand_2;
    s32 operand_3;
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
    u8* ghost_parameters;
    u8* instruction;
    battle_map_tile_t* tile;
    unit_stats_t* copy_destination;
    u8* effect_instruction;
    thread_t* thread;
    thread_t* threads;

    thread_set_current_task_id(THREAD_TASK_EXECUTE_EVENT);
    if (tutorial_is_current_event() != 0) {
        gfx_ramp_screen_overlay_intensity_to_zero();
    }

    if (g_menu_input_mode == MENU_INPUT_PLAYER) {
        for (work_index = 2; work_index < 0xE; work_index++) {
            thread_wait_until_inactive(work_index);
        }
        camera_wait_until_idle();
    } else {
        g_unit_view_hovered_stats_rect.y = 0xAA;
        g_unit_view_attack_caster_stats_rect.y = 0xAA;
        g_unit_view_attack_target_stats_rect.y = 0xAA;
        g_unit_view_right_stats_rect.y = 0xAA;
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
        /* Matching: load the fill value before the descending loop index. */
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
                script_opcode_7a_dismiss_unit(first_operand);
            }
        } else if (opcode == EVENT_OPCODE_CALL_FUNCTION) {
            /* These are sequential tests, not switch alternatives: selectors
             * 6 and 14 reuse operand_1 as a loop index, and later tests see
             * its changed value. Preserve that target-visible fallthrough. */
            thread_wait_frames(4);

            if (script_get_variable(EVENT_SCRIPT_VAR_SUPPRESS_PROGRESS_EFFECTS) == 0) {
                if (operand_1 == EVENT_CALL_FUNCTION_JOIN_UNITS_SILENTLY) {
                    if (script_get_variable(EVENT_SCRIPT_VAR_CURRENT_EVENT) < EVENT_ID_NEW_GAME_INVENTORY_END) {
                        item_init_new_game_inventory();
                    }
                    party_join_event_units();
                }

                /* Stop thread slots 2..14 around the save menu, which runs in the CARD
                 * companion overlay. */
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
                    overlay_request_companion(COMPANION_OVERLAY_CARD);
                    g_overlay_companion_overlay_state = COMPANION_OVERLAY_STATE_CARD_SAVE;
                    do {
                        thread_yield();
                    } while (g_overlay_companion_overlay_state != COMPANION_OVERLAY_STATE_IDLE);

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
                calendar_advance_one_year();
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
                g_debug_display_enabled = 0;
                g_debug_display_unread_flag = 0;
            }

            if (operand_1 == EVENT_CALL_FUNCTION_MUTE_TEXT_AUDIO_CUE) {
                script_set_variable(EVENT_SCRIPT_VAR_MUTE_TEXT_AUDIO_CUE, 1);
            }

            if (operand_1 == EVENT_CALL_FUNCTION_UNMUTE_TEXT_AUDIO_CUE) {
                script_set_variable(EVENT_SCRIPT_VAR_MUTE_TEXT_AUDIO_CUE, 0);
            }

            if (operand_1 == EVENT_CALL_FUNCTION_ADVANCE_MAP_DESTRUCTION_AND_WAIT) {
                script_set_specialized_map_destroyed();
                if (g_battle_map_destruction_wait_state != 0) {
                    do {
                        thread_yield();
                    } while (g_battle_map_destruction_wait_state != 0);
                }
            }

            if (operand_1 == EVENT_CALL_FUNCTION_PLAY_ALTIMA_MUSIC) {
                g_sound_current_music_track_index = 0;
                g_sound_music_track_1_id = MUSIC_ALTIMA;
                g_sound_music_track_2_id = 0;
                music_play_tracks(MUSIC_ALTIMA, MUSIC_NONE);
            }

            if (operand_1 == EVENT_CALL_FUNCTION_COPY_UNIT_8_POSITION_TO_UNIT_9_AND_RELOAD_GRAPHICS) {
                unit = unit_get_stats(7);
                copy_destination = unit_get_stats(8);
                copy_destination->x = unit->x;
                copy_destination->position.y = unit->position.y;
                copy_destination->position.higher_elevation = unit->position.higher_elevation;
                sprite_load_unit_graphics_by_battle_id(8, 1U);
                g_unit_graphics_load_pending = 1;
                do {
                    thread_yield();
                } while (g_unit_graphics_load_pending != 0);
                thread_yield();
                thread_yield();
            }

            /* Hides every unit standing in x 0..3, y 0..10 or in
             * x 4..6, y 2..7. operand_1 is left holding the last unit's battle id, which the
             * selector tests below then see. */
            if (operand_1 == EVENT_CALL_FUNCTION_HIDE_BETHLA_REGION_UNITS) {
                region_runtime_id = 0;
                do {
                    if (unit_has_runtime_id(region_runtime_id) != 0) {
                        operand_1 = unit_get_battle_id_by_runtime_id(region_runtime_id);
                        if (operand_1 != -1U) {
                            unit = unit_get_stats(operand_1);
                            if ((unit->x < 4U) && (unit->position.y < 0xBU)) {
                                unit_hide_by_runtime_id(region_runtime_id);
                            }
                            if (((unit->x - 4) < 3U) && ((unit->position.y - 2) < 6U)) {
                                unit_hide_by_runtime_id(region_runtime_id);
                            }
                        }
                    }
                    region_runtime_id += 1;
                } while (region_runtime_id < UNIT_SLOT_COUNT);
            }

            if (operand_1 == EVENT_CALL_FUNCTION_COPY_UNIT_2_POSITION_AND_FACING_TO_UNIT_8_AND_RELOAD_GRAPHICS) {
                unit = unit_get_stats(1);
                copy_destination = unit_get_stats(7);
                copy_destination->x = unit->x;
                copy_destination->position.y = unit->position.y;
                copy_destination->position.higher_elevation = unit->position.higher_elevation;
                copy_destination->position.facing = unit->position.facing;
                sprite_load_unit_graphics_by_battle_id(7, 1U);
                g_unit_graphics_load_pending = 1U;
                do {
                    thread_yield();
                } while (g_unit_graphics_load_pending != 0);
                thread_yield();
                thread_yield();
            }

            /* OpenSaveMenu's stop loops leave operand_1 at 0x0f, so an unsuppressed 0x06
             * also runs this warp. */
            if ((operand_1 - EVENT_CALL_FUNCTION_WARP_UNIT_4_DISPLAY_TO_UNIT_2_POSITION_AND_FACING) < 2U) {
                script_warp_unit_display_to_paired_unit(operand_1);
            }

            if (operand_1 == EVENT_CALL_FUNCTION_CLEAR_ZALERA_ACTIVE_STATUS_GRAPHICS) {
                draw_runtime_id = script_resolve_runtime_id(CHARACTER_IDENTITY_ZALERA);
                if (draw_runtime_id != EVENT_RUNTIME_ID_NONE) {
                    status_queue_current_status_graphics(unit_get_battle_id_by_runtime_id(draw_runtime_id), 0);
                    g_thread_call_target = (void (*)(void))unit_update_graphics_by_runtime_id;
                    thread_call_on_main_stack(draw_runtime_id);
                }
            }
        } else if (opcode == EVENT_OPCODE_BLUE_REMOVE_UNIT) {
            script_opcode_99_blue_remove_unit(first_operand);
        } else if (opcode == EVENT_OPCODE_END || opcode == EVENT_OPCODE_END_2) {
            battle_map_wait_for_refresh();
            do {
                thread_yield();
            } while (thread_find_running_by_task(THREAD_TASK_EVENT_BLOCK) != 0);
            do {
                thread_yield();
            } while (file_is_still_loading() != 0);
            thread_yield();

            if (tutorial_is_current_event() != 0) {
                gfx_ramp_screen_overlay_intensity_to_max();
                g_game_options = g_game_options_saved;
                script_set_variable(EVENT_SCRIPT_VAR_SUPPRESS_PROGRESS_EFFECTS, 0);
                state_enter_close_battle_to_world(0x20);
            }

            if (g_menu_input_mode == MENU_INPUT_PLAYER) {
                for (work_index = 2; work_index < 0xE; work_index++) {
                    thread_wait_until_inactive(work_index);
                }
            }

            operand_1 = script_get_variable(EVENT_SCRIPT_VAR_CURRENT_EVENT);
            if (operand_1 != EVENT_ID_OLAN_WOUNDED && operand_1 != EVENT_ID_BEOWULF_INVITATION) {
                sprite_evtchr_init_slots();
            }
            script_set_variable(EVENT_SCRIPT_VAR_BATTLE_NOT_STARTED, 0);

            /* An event with no after-event code, outside events
             * 0x190..0x1a9, checks the scenario conditions and chains into
             * the event they select. */
            finish_operation = (g_scenario_event_finish_operations[operand_1] & ~0x0CFF) >> 8;
            if (g_menu_input_mode == MENU_INPUT_PLAYER && finish_operation == 0
                && (operand_1 - EVENT_ID_COMMON_FIRST) >= 0x1AU && operand_1 != 0) {
                g_scenario_condition_unit_battle_id = 0xFF;
                scenario_run_conditions();
                if (script_load_next_event() != 0) {
                    goto restart_script;
                }
            }

            /* Otherwise the event ends: replay the staged statuses and copy
             * each displayed unit's facing quadrant to its battle record. */
            work_index = 0;
            unit_facing_snap_all_to_quadrants();
            unit_update_staged_status_data(0, 1);
            for (; work_index < UNIT_SLOT_COUNT; work_index++) {
                if (unit_has_runtime_id(work_index) != 0) {
                    operand_1 = unit_get_battle_id_by_runtime_id(work_index);
                    if (operand_1 >= 0) {
                        unit = unit_get_stats(operand_1);
                        unit->position.facing = unit_facing_get_quadrant_by_runtime_id(work_index);
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
            g_script_opcode_dc_value = 0xD;
        }
        /* ---- Dialogue and portraits ---- */
        else if (opcode == EVENT_OPCODE_DISPLAY_MESSAGE) {
            operand_1 = thread_resolve_id(operand_1);
            thread_start(operand_1, message_dialog_thread);
            message_build_display(
                instruction_offset, operand_2, &g_thread_contexts[operand_1].task_words[DIALOG_THREAD_WORD_TEXT_WIDTH]);
            g_thread_contexts[operand_1].function_parameter_1 = operand_2;
            /* Script text ids count from 1. */
            g_thread_contexts[operand_1].function_parameter_2 = ((operand_4 << 8) + operand_3) - 1;

            dialogue_cursor = (u8*)g_script_event_block;
            dialogue_cursor += instruction_offset;
            message_instruction = (event_display_message_instruction_t*)dialogue_cursor;
            operand_4 = message_instruction->unit_id_le[1];
            operand_3 = message_instruction->unit_id_le[0];
            g_thread_contexts[operand_1].function_parameter_3 = (operand_4 << 8) + operand_3;
            g_thread_contexts[operand_1].function_parameter_4 = message_instruction->portrait_selector;
            g_thread_contexts[operand_1].task_words[DIALOG_THREAD_WORD_X_OFFSET]
                = script_read_s16(message_instruction->x_offset_le);
            g_thread_contexts[operand_1].task_words[DIALOG_THREAD_WORD_Y_OFFSET] = script_read_s16(
                ((event_display_message_instruction_t*)(instruction_offset + (u8*)g_script_event_block))->y_offset_le);
            g_thread_contexts[operand_1].task_words[DIALOG_THREAD_WORD_ARROW_OFFSET] = script_read_s16(
                ((event_display_message_instruction_t*)(instruction_offset + (u8*)g_script_event_block))
                    ->arrow_offset_le);
            message_cursor = (u8*)g_script_event_block;
            message_cursor += instruction_offset;
            g_thread_contexts[operand_1].task_words[DIALOG_THREAD_WORD_WINDOW_OPTIONS]
                = ((event_display_message_instruction_t*)message_cursor)->window_options;
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
                    /* Script text ids count from 1; 0xFFFF, which closes the dialog, is kept. */
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
            script_opcode_50_portrait_col(operand_1);
        }
        /* ---- Map objects ---- */
        else if (opcode == EVENT_OPCODE_USE_3D_OBJECT) {
            g_event_3d_object_use_request = 1;
            g_event_3d_object_id = operand_1;
            g_event_3d_object_state = operand_2;
            thread_yield();
        } else if (opcode == EVENT_OPCODE_WAIT_3D_OBJECT) {
            g_3d_object_wait_status = 1;
            do {
                thread_yield();
            } while (g_3d_object_wait_status != 0);
        } else if (opcode == EVENT_OPCODE_USE_FIELD_OBJECT) {
            g_event_field_object_use_request = 1;
            g_event_map_command_83_arg1 = operand_1;
            g_event_map_command_83_arg2 = operand_2;
            thread_yield();
        } else if (opcode == EVENT_OPCODE_WAIT_FIELD_OBJECT) {
            g_field_object_wait_status = 1;
            do {
                thread_yield();
            } while (g_field_object_wait_status != 0);
        }
        /* ---- Unit sprites and animation ---- */
        else if (opcode == EVENT_OPCODE_MIRROR_SPRITE) {
            script_opcode_68_mirror_sprite((const event_mirror_sprite_parameters_t*)parameters);
        } else if (opcode == EVENT_OPCODE_RESET_PALETTE) {
            script_opcode_97_reset_palette((const event_unit_parameters_t*)parameters);
        } else if (opcode == EVENT_OPCODE_SET_FIRST_UNIT) {
            operand_1 = script_resolve_runtime_id(first_operand);
            if (operand_1 != EVENT_RUNTIME_ID_NONE) {
                unit_move_to_head(operand_1);
            }
        } else if (opcode == EVENT_OPCODE_UNIT_ANIM) {
            script_opcode_11_unit_anim((const event_unit_anim_parameters_t*)parameters);
        } else if (opcode == EVENT_OPCODE_MARCH) {
            script_opcode_80_march((const event_march_parameters_t*)parameters);
        } else if (opcode == EVENT_OPCODE_SET_UNIT_MOVEMENT_EFFECT_SUPPRESSION) {
            script_opcode_81_set_unit_movement_effect_suppression(
                (const event_movement_effect_parameters_t*)parameters);
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
            battle_map_reset_script_light_transform();
        } else if (opcode == EVENT_OPCODE_CHANGE_MAP) {
            g_event_pending_map_jump_out_2_id = first_operand;
            thread_wait_frames(2);
            battle_map_reset_script_light_transform();
        } else if (opcode == EVENT_OPCODE_SET_DAYTIME) {
            g_event_pending_map_state = operand_1;
            thread_wait_frames(2);
            battle_map_wait_for_refresh();
        } else if (opcode == EVENT_OPCODE_WEATHER) {
            g_event_weather_request = first_operand;
            thread_wait_frames(2);
            battle_map_wait_for_refresh();
        } else if (opcode == EVENT_OPCODE_RELOAD_MAP_STATE) {
            g_battle_map_reload_state = 1;
            do {
                thread_yield();
            } while (g_battle_map_reload_state != 0);
        } else if (opcode == EVENT_OPCODE_APPEND_MAP_STATE) {
            g_battle_map_append_state = 1;
            do {
                thread_yield();
            } while (g_battle_map_append_state != 0);
        } else if (opcode == EVENT_OPCODE_PAUSE) {
            script_opcode_16_pause();
        } else if (opcode == EVENT_OPCODE_RESUME_WAITING_DIALOG) {
            for (work_index = 1; work_index < THREAD_ID_COUNT; work_index++) {
                thread = &g_thread_contexts[work_index];
                if (thread->is_running != 0 && thread->task_id == THREAD_TASK_WAIT_FOR_RESUME
                    && thread->function_parameter_2 == first_operand) {
                    thread->task_id = THREAD_TASK_RESUME;
                }
            }
        } else if (opcode == EVENT_OPCODE_FLIP_MESSAGE_PORTRAIT) {
            script_opcode_37_flip_message_portrait(parameters);
        } else if (opcode == EVENT_OPCODE_UNKNOWN_17) {
        }
        /* ---- Effects ---- */
        else if (opcode == EVENT_OPCODE_EFFECT) {
            g_event_pending_effect_id = first_operand;
            effect_cursor = (u8*)g_script_event_block;
            effect_cursor += instruction_offset;
            g_event_effect_target_runtime_id = script_resolve_runtime_id(effect_cursor[3]);
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
                g_thread_call_target = (void (*)(void))effect_file_poll_load;
            } while (thread_call_on_main_stack() != 0);
        } else if (opcode == EVENT_OPCODE_EFFECT_END) {
            thread_start(thread_resolve_id(THREAD_SLOT_FIRST_IDLE), &script_opcode_8b_effect_end_thread);
        }
        /* ---- Camera ---- */
        else if (opcode == EVENT_OPCODE_FOCUS) {
            camera_store_yaw_band(g_script_variables[EVENT_SCRIPT_VAR_CAMERA_YAW]);
            script_focus((event_focus_parameters_t*)parameters);
        } else if (opcode == EVENT_OPCODE_FOCUS_SPEED) {
            script_variables = g_script_variables;
            script_focus_speed(parameters, &script_variables[EVENT_SCRIPT_VAR_CAMERA_X],
                &script_variables[EVENT_SCRIPT_VAR_CAMERA_TILT]);
        } else if (opcode == EVENT_OPCODE_RELATIVE_CAMERA) {
            script_apply_relative_camera(
                (event_camera_values_t*)parameters, &g_script_variables[EVENT_SCRIPT_VAR_CAMERA_X]);
        } else if (opcode == EVENT_OPCODE_CAMERA) {
            operand_1 = thread_resolve_id(THREAD_SLOT_FIRST_IDLE);
            thread_start(operand_1, &camera_thread);
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
            script_opcode_74_unknown(parameters);
        }
        /* ---- Unit movement and facing ---- */
        else if (opcode == EVENT_OPCODE_WALK_TO) {
            g_thread_call_target = (void (*)(void))script_opcode_28_walk_to;
            thread_call_on_main_stack(parameters);
        } else if (opcode == EVENT_OPCODE_JUMP) {
            script_opcode_70_jump((const event_jump_parameters_t*)parameters);
        } else if (opcode == EVENT_OPCODE_WAIT_WALK) {
            script_opcode_29_wait_walk(first_operand);
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
            } while (file_is_still_loading() != 0);
        } else if (opcode == EVENT_OPCODE_ROTATE_UNIT) {
            script_opcode_2d_rotate_unit((const event_rotate_unit_parameters_t*)parameters);
        } else if (opcode == EVENT_OPCODE_FACE_EACH_OTHER) {
            script_opcode_2c_face_unit((const event_face_unit_parameters_t*)parameters, 0);
        } else if (opcode == EVENT_OPCODE_FACE_UNIT) {
            script_opcode_2c_face_unit((const event_face_unit_parameters_t*)parameters, 1);
        } else if (opcode == EVENT_OPCODE_FACE_TILE) {
            script_opcode_69_face_tile((const event_face_tile_parameters_t*)parameters);
        } else if (opcode == EVENT_OPCODE_WAIT_ROTATE_UNIT) {
            script_opcode_65_wait_rotate_all(first_operand);
        } else if (opcode == EVENT_OPCODE_WAIT_ROTATE_ALL) {
            script_opcode_65_wait_rotate_all(-1);
        }
        /* ---- Blocks, earthquakes, darkness, and screen color ---- */
        else if (opcode == EVENT_OPCODE_BLOCK_START) {
            operand_1 = thread_resolve_id(THREAD_SLOT_FIRST_IDLE);
            thread_start(operand_1, script_opcode_2a_block_start_thread);
            g_thread_contexts[operand_1].function_parameter_1 = (s32)parameters;
            instruction_offset = script_find_instruction_byte_offset(instruction_offset, EVENT_OPCODE_BLOCK_END);
        } else if (opcode == EVENT_OPCODE_BLOCK_END) {
        } else if (opcode == EVENT_OPCODE_UNKNOWN_23) {
        } else if (opcode == EVENT_OPCODE_UNKNOWN_24) {
        } else if (opcode == EVENT_OPCODE_EARTHQUAKE_END) {
            thread_wait_frames(1);
            thread_suspend(thread_find_running_by_task(THREAD_TASK_EARTHQUAKE));
        } else if (opcode == EVENT_OPCODE_SET_SPEAKER_PAIR) {
            g_message_speaker_swap_unit_a = first_operand;
            g_message_speaker_swap_unit_b = script_read_s16(instruction_offset + ((u8*)g_script_event_block) + 3);
        } else if (opcode == EVENT_OPCODE_MAP_DARKNESS) {
            operand_1 = thread_resolve_id(THREAD_SLOT_FIRST_IDLE);
            thread_start(operand_1, &script_opcode_1a_map_darkness);
            g_thread_contexts[operand_1].function_parameter_1 = (s32)parameters;
        } else if (opcode == EVENT_OPCODE_COLOR_SCREEN) {
            operand_1 = thread_resolve_id(THREAD_SLOT_FIRST_IDLE);
            thread_start(operand_1, &script_opcode_3e_color_screen_thread);
            g_thread_contexts[operand_1].function_parameter_1 = (s32)parameters;
        } else if (opcode == EVENT_OPCODE_EARTHQUAKE_START) {
            operand_1 = thread_resolve_id(THREAD_SLOT_FIRST_IDLE);
            thread_start(operand_1, &script_opcode_41_earthquake_start);
            g_thread_contexts[operand_1].function_parameter_1 = (s32)parameters;
        } else if ((opcode == EVENT_OPCODE_SPRITE_MOVE) || (opcode == EVENT_OPCODE_SPRITE_MOVE_BETA)) {
            operand_1 = thread_resolve_id_after_current(THREAD_SLOT_FIRST_IDLE);
            if (opcode == EVENT_OPCODE_SPRITE_MOVE) {
                thread_start(operand_1, &script_sprite_move);
            } else {
                thread_start(operand_1, &script_sprite_move_beta);
            }
            g_thread_contexts[operand_1].function_parameter_1 = (s32)parameters;
            g_thread_contexts[operand_1].task_id = THREAD_TASK_SPRITE_MOVE;
            g_thread_contexts[operand_1].task_words[SPRITE_MOVE_THREAD_WORD_RUNTIME_ID]
                = script_resolve_runtime_id(first_operand);
        } else if (opcode == EVENT_OPCODE_WAIT_SPRITE_MOVE) {
            script_opcode_6f_wait_sprite_move(first_operand);
        } else if (opcode == EVENT_OPCODE_MAP_LIGHT) {
            operand_1 = thread_resolve_id(THREAD_SLOT_FIRST_IDLE);
            thread_start(operand_1, &battle_map_light_thread);
            g_thread_contexts[operand_1].function_parameter_1 = (s32)parameters;
        }
        /* ---- Background, palettes, and terrain ---- */
        else if (opcode == EVENT_OPCODE_BACKGROUND) {
            mem_copy_bytes_nonempty(
                &g_event_background_colors.colors[BATTLE_MAP_BACKGROUND_GRADIENT_TOP], parameters, 3);
            mem_copy_bytes_nonempty(
                &g_event_background_colors.colors[BATTLE_MAP_BACKGROUND_GRADIENT_BOTTOM], parameters + 3, 3);
            if (parameters[7] == 0) {
                battle_map_background_transition_and_store(parameters[6], &g_event_background_colors);
            } else {
                battle_map_background_set(parameters[6], &g_event_background_colors);
            }
        } else if (opcode == EVENT_OPCODE_COLOR_BG_BETA) {
            battle_map_background_modify(
                parameters[0], parameters[4], ((s8)parameters[1]), ((s8)parameters[2]), ((s8)parameters[3]));
        } else if (opcode == EVENT_OPCODE_COLOR_UNIT) {
            script_opcode_32_color_unit((const event_color_unit_parameters_t*)parameters);
        } else if (opcode == EVENT_OPCODE_RESUME_UNIT_PALETTE_UPDATES) {
            script_set_units_palette_update_suppression((u16)first_operand, 0);
        } else if (opcode == EVENT_OPCODE_SUPPRESS_UNIT_PALETTE_UPDATES) {
            script_set_units_palette_update_suppression((u16)first_operand, 1);
        } else if (opcode == EVENT_OPCODE_COLOR_FIELD) {
            battle_map_color_field(
                parameters[0], parameters[4], ((s8)parameters[1]), ((s8)parameters[2]), ((s8)parameters[3]));
        } else if (opcode == EVENT_OPCODE_STORE_MODIFIED_MAP_PALETTE) {
            battle_map_palette_store_modified_colors(0, 0, 1);
        } else if (opcode == EVENT_OPCODE_SET_TILE_CAMERA_BLOCK) {
            battle_map_tile_set_value(BATTLE_MAP_TILE_DATA_FLAT_CAMERA_BLOCK_MASK, parameters[0], parameters[1],
                parameters[2], parameters[3]);
        } else if (opcode == EVENT_OPCODE_SET_TILE_HEIGHT) {
            tile = battle_map_tile_get_pointer(parameters[0], parameters[1], parameters[2]);
            if ((tile->surface.type - BATTLE_MAP_SURFACE_GRASSLAND) >= 2U) {
                tile->surface.type = BATTLE_MAP_SURFACE_STONE_FLOOR;
            }
            tile->height = parameters[3];
            tile->flags.untargetable = parameters[4];
            tile->flags.blocked = 0;
        }
        /* ---- Event speed, dark screen, conditions, and graphics ---- */
        else if (opcode == EVENT_OPCODE_EVENT_SPEED) {
            script_set_event_speed(operand_1);
        } else if (opcode == EVENT_OPCODE_DARK_SCREEN) {
            g_unit_panel_hide_numeric_values = 0;
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
            thread_start(operand_3, &script_execute_display_conditions_instruction);
            thread_set_parameters(operand_3, operand_1, operand_2, 0);
        } else if (opcode == EVENT_OPCODE_SHOW_GRAPHIC) {
            /* Use: the extra operand_2 reference keeps operand_2/operand_3 in $s5/$s6, not swapped. */
            __asm__("" : : "r"(operand_2));
            operand_3 = thread_resolve_id(THREAD_SLOT_FIRST_IDLE);
            thread_start(operand_3, &script_show_graphic);
            thread_set_parameters(operand_3, operand_1, 0, 0);
        }
        /* ---- Sound and music ---- */
        else if (opcode == EVENT_OPCODE_SOUND_EFFECT) {
            g_sound_pending_sfx_id = first_operand;
        } else if (opcode == EVENT_OPCODE_BG_SOUND) {
            operand_1 = thread_resolve_id(THREAD_SLOT_FIRST_IDLE);
            thread_start(operand_1, &sound_background_thread);
            thread_set_parameters(operand_1, (s32)parameters, 0, 0);
        } else if (opcode == EVENT_OPCODE_EDIT_BG_SOUND) {
            operand_1 = thread_resolve_id(THREAD_SLOT_FIRST_IDLE);
            thread_start(operand_1, &sound_background_fade_thread);
            thread_set_parameters(operand_1, (s32)parameters, 0, 0);
        } else if (opcode == EVENT_OPCODE_SWITCH_TRACK) {
            g_sound_music_switch_request = operand_1;
            g_event_music_switch_volume = operand_2;
            g_event_music_switch_fade = operand_3;
            thread_yield();
        } else if (opcode == EVENT_OPCODE_FADE_SOUND) {
            /* The request carries the target volume in its low half and the fade time in
             * its high half. Byte 1 only adds to the shift, so the target volume is always
             * 0: FadeSound fades the music out over byte 2's frames, doubled per unit of
             * byte 1 (which scripts leave 0). */
            /* Matching limitation: target sllv masks its count to five bits.
             * An explicit &31 adds an instruction with GCC 2.6.3, so counts
             * above 31 remain undefined in C; no script restriction is proven. */
            g_sound_music_volume_transition_request = operand_2 << (operand_1 + 0x10);
            thread_yield();
        } else if (opcode == EVENT_OPCODE_END_TRACK) {
            g_sound_music_unload_slot_request = operand_1;
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
            remove_unit_runtime_id = script_resolve_runtime_id(first_operand);
            if (remove_unit_runtime_id != EVENT_RUNTIME_ID_NONE) {
                work_index = remove_unit_runtime_id;
                while (unit_is_moving(work_index) != 0) {
                    thread_yield();
                }
                if (work_index != EVENT_RUNTIME_ID_NONE) {
                    operand_1 = unit_get_battle_id_by_runtime_id(work_index);
                    unit_remove_by_runtime_id(work_index);
                    if (operand_1 >= 0) {
                        unit_set_off_field(operand_1);
                    }
                }
            }
        } else if (opcode == EVENT_OPCODE_DRAW) {
            draw_runtime_id = script_resolve_runtime_id(first_operand);
            if (draw_runtime_id != EVENT_RUNTIME_ID_NONE) {
                unit_draw_and_activate_by_runtime_id(draw_runtime_id);
                if (script_get_variable(EVENT_SCRIPT_VAR_CURRENT_EVENT) == EVENT_ID_LIMBERRY_CEMETERY_BATTLE) {
                    status_queue_current_status_graphics(unit_get_battle_id_by_runtime_id(draw_runtime_id), 1);
                    g_thread_call_target = (void (*)(void))unit_update_graphics_by_runtime_id;
                    thread_call_on_main_stack(draw_runtime_id);
                }
            }
        } else if (opcode == EVENT_OPCODE_ERASE) {
            erased_runtime_id = script_resolve_runtime_id(first_operand);
            if (erased_runtime_id != EVENT_RUNTIME_ID_NONE) {
                unit_hide_by_runtime_id(erased_runtime_id);
            }
        } else if (opcode == EVENT_OPCODE_ADD_GHOST_UNIT) {
            ghost_parameters = instruction_offset + ((u8*)g_script_event_block) + 3;
            /* An empty four-pass countdown precedes the search for a free
             * runtime slot. */
            ghost_runtime_id = 3;
            do {
                ghost_runtime_id -= 1;
            } while (ghost_runtime_id >= 0);
            for (ghost_runtime_id = 0; ghost_runtime_id < UNIT_SLOT_COUNT; ghost_runtime_id++) {
                if (unit_has_runtime_id(ghost_runtime_id) == 0) {
                    g_script_unit_slots[ghost_parameters[0]] = ghost_runtime_id;
                    break;
                }
            }
            if (ghost_runtime_id != UNIT_SLOT_COUNT) {
                script_opcode_47_add_ghost_unit(ghost_parameters[1], ghost_parameters[2], ghost_parameters[3],
                    ghost_parameters[4], first_operand, g_script_unit_slots[ghost_parameters[0]], ghost_parameters[5]);
                g_unit_graphics_load_pending = 1U;
            } else {
                /* No free runtime slot: the instruction is retried at once,
                 * with no yield in between. */
                continue;
            }
        } else if (opcode == EVENT_OPCODE_UNKNOWN_75) {
        } else if (opcode == EVENT_OPCODE_ADD_UNIT) {
            script_opcode_45_add_unit();
            added_unit = unit_entd_find_stats(first_operand, &added_unit_state);
            /* Add a present unit (state >= 0) or an off-field one (-2); crystal, treasure,
             * disabled and removed units are skipped. */
            if ((added_unit_state == -2) || (added_unit_state >= 0)) {
                for (added_battle_id = 0; added_battle_id < UNIT_SLOT_COUNT; added_battle_id++) {
                    if (unit_get_stats(added_battle_id) == added_unit)
                        break;
                }
                operand_1 = unit_get_runtime_id_by_battle_id(added_battle_id);
                if (operand_1 == -1) {
                    unit_save_staged_status_data(added_battle_id);
                    sprite_load_unit_graphics_by_battle_id(added_battle_id, operand_3);
                    g_unit_graphics_load_pending = 1U;
                } else {
                    if (operand_3 == 0) {
                        unit_draw_and_activate_by_runtime_id(operand_1);
                    }
                    if (unit_save_staged_status_data(added_battle_id) != 0) {
                        g_thread_call_target = (void (*)(void))unit_update_graphics_by_runtime_id;
                        thread_call_on_main_stack(operand_1);
                    }
                }
            }
        } else if (opcode == EVENT_OPCODE_WARP_UNIT) {
            script_opcode_5f_warp_unit((const event_warp_unit_parameters_t*)parameters);
        } else if (opcode == EVENT_OPCODE_UNKNOWN_61) {
        } else if (opcode == EVENT_OPCODE_SET_BATTLE_UNIT_POSITION) {
            /* Direct simulation slot, not the encoded identity used by WarpUnit.
             * Operand byte 1 is unused. The target neither guards slots >=21
             * nor synchronizes the renderer's position here. */
            unit = unit_get_stats(operand_1);
            position_instruction
                = (event_set_battle_unit_position_instruction_t*)(instruction_offset + (u8*)g_script_event_block);
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
            thread_start(operand_1, script_opcode_49_add_unit_start_thread);
            g_thread_contexts[operand_1].function_parameter_1 = (s32)parameters;
            instruction_offset = script_find_instruction_byte_offset(instruction_offset, EVENT_OPCODE_ADD_UNIT_END);
        } else if (opcode == EVENT_OPCODE_ADD_UNIT_END) {
        }
        /* ---- Waits and synchronization ---- */
        else if (opcode == EVENT_OPCODE_WAIT_FOR_INSTRUCTION) {
            /* Yield until no thread runs the named task. The outer wait stays
             * a label loop: as a C loop its notes change the operand register
             * allocation (see the WORLD twin). */
            operand_3 = (operand_2 << 8) + operand_1;
            threads = g_thread_contexts;
        wait_matching_task:
            thread_yield();
            for (work_index = 1; work_index < 0x10; work_index++) {
                /* Typed indexing reverses the target addu operands; keep the 32-bit address sum. */
                thread = (thread_t*)((work_index * sizeof(thread_t)) + (u32)threads);
                if (thread->is_running != 0 && thread->task_id == operand_3) {
                    break;
                }
            }
            if (work_index != 0x10) {
                goto wait_matching_task;
            }
        } else if (opcode == EVENT_OPCODE_YIELD) {
            thread_yield();
        } else if (opcode == EVENT_OPCODE_WAIT) {
            if (tutorial_is_current_event() != 0 && first_operand >= 3) {
                thread_wait_frames(first_operand / system_get_vsync_interval());
            } else {
                thread_wait_frames(first_operand);
            }
        } else if (opcode == EVENT_OPCODE_TUTORIAL_BUTTON_WAIT) {
            do {
                thread_yield();
            } while (!(first_operand & g_script_unfiltered_controller_input));
        } else if (opcode == EVENT_OPCODE_END_SOUND) {
            sound_stop_weather_music();
        }
        /* ---- Miscellaneous unit and display commands ---- */
        else if (opcode == EVENT_OPCODE_UNIT_SHADOW) {
            shadow_runtime_id = script_resolve_runtime_id(first_operand);
            if (shadow_runtime_id != EVENT_RUNTIME_ID_NONE) {
                if (operand_3 != 0) {
                    sprite_shadow_hide_by_runtime_id(shadow_runtime_id);
                } else {
                    sprite_shadow_show_by_runtime_id(shadow_runtime_id);
                }
            }
        } else if (opcode == EVENT_OPCODE_DEBUG_PAUSE) {
            script_print_debug_message();
        } else if (opcode == EVENT_OPCODE_WAIT_VALUE) {
            script_opcode_7e_wait_value((const event_wait_value_parameters_t*)parameters);
        } else if (opcode == EVENT_OPCODE_EVTCHR_PALETTE) {
            evtchr_palette_runtime_id = script_resolve_runtime_id(first_operand);
            if (evtchr_palette_runtime_id != EVENT_RUNTIME_ID_NONE) {
                sprite_evtchr_set_palette_by_runtime_id(evtchr_palette_runtime_id, operand_3, operand_4);
            }
        } else if (opcode == EVENT_OPCODE_LOAD_ATTACK_GRAPHICS) {
            script_opcode_82_load_attack_graphics();
        } else if (opcode == EVENT_OPCODE_CHANGE_STATS) {
            script_opcode_83_change_stats((const event_change_stats_parameters_t*)parameters);
        } else if (opcode == EVENT_OPCODE_PLAY_TUNE) {
            music_play_tune(operand_1);
        } else if (opcode == EVENT_OPCODE_UNLOCK_DATE) {
            if ((script_get_variable(EVENT_SCRIPT_VAR_SUPPRESS_PROGRESS_EFFECTS) == 0) && (operand_1 != 0)) {
                date_month = script_get_variable(EVENT_SCRIPT_VAR_MONTH);
                script_opcode_85_unlock_date(g_brave_story_treasure_acquisition_date_bits, operand_1 - 1, date_month,
                    script_get_variable(EVENT_SCRIPT_VAR_DAY));
            }
        } else if (opcode == EVENT_OPCODE_TEMP_WEAPON) {
            thrown_item_runtime_id = script_resolve_runtime_id(first_operand);
            if (thrown_item_runtime_id != EVENT_RUNTIME_ID_NONE) {
                sprite_palette_set_thrown_item_by_runtime_id(operand_3, thrown_item_runtime_id);
            }
        } else if (opcode == EVENT_OPCODE_ARROW) {
            arrow_target_runtime_id = script_resolve_runtime_id(first_operand);
            if (arrow_target_runtime_id != EVENT_RUNTIME_ID_NONE) {
                operand_1 = unit_get_battle_id_by_runtime_id(arrow_target_runtime_id);
                arrow_shooter_runtime_id
                    = script_resolve_runtime_id(script_read_s16(instruction_offset + ((u8*)g_script_event_block) + 3));
                if (arrow_shooter_runtime_id != EVENT_RUNTIME_ID_NONE) {
                    arrow_shooter_battle_id = unit_get_battle_id_by_runtime_id(arrow_shooter_runtime_id);
                    g_thread_call_target = (void (*)(void))projectile_set_arrow_trajectory;
                    thread_call_on_main_stack(arrow_shooter_battle_id, &g_unit_scratch_coords, operand_1);
                }
            }
        } else if (opcode == EVENT_OPCODE_MAP_FREEZE) {
            battle_map_freeze();
        } else if (opcode == EVENT_OPCODE_MAP_UNFREEZE) {
            battle_map_unfreeze();
        } else if (opcode == EVENT_OPCODE_UNIT_ANIM_ROTATE) {
            script_opcode_8c_unit_anim_rotate((const event_unit_anim_rotate_parameters_t*)parameters);
        } else if (opcode == EVENT_OPCODE_WAIT_GRAPHIC_PRINT) {
            thread_yield();
            do {
                thread_yield();
                operand_1 = thread_find_running_by_task(THREAD_TASK_SHOW_GRAPHIC);
            } while (operand_1 != 0 && g_thread_contexts[operand_1].function_parameter_4 == 0);
        } else if (opcode == EVENT_OPCODE_LOCK_MENU_INPUT) {
            g_menu_input_lock_mode = operand_1;
        } else if (opcode == EVENT_OPCODE_TUTORIAL_COORDINATE_CONFIRM) {
            /* Wait for Circle, with no direction held, while the cursor is on tile
             * (operand_1, operand_2). */
            while (g_cursor_x != operand_1 || g_cursor_y != operand_2
                || ((confirm_buttons = g_script_unfiltered_controller_input) & PSX_PAD_CIRCLE) == 0
                || (confirm_buttons & (PSX_PAD_UP | PSX_PAD_RIGHT | PSX_PAD_DOWN | PSX_PAD_LEFT)) != 0) {
                thread_yield();
            }
        }
        /* ---- Map title, dates, teleports, and status ---- */
        else if (opcode == EVENT_OPCODE_SHOW_MAP_TITLE) {
            operand_4 = thread_resolve_id(THREAD_SLOT_FIRST_IDLE);
            overlay_request_companion(COMPANION_OVERLAY_ATTACK);
            battle_map_title_load_graphic();
            thread_start(operand_4, battle_map_title_thread);
            thread_set_parameters(operand_4, (s8)operand_1, (s8)operand_2, (s8)operand_3);
            thread_wait_until_inactive(operand_4);
        } else if (opcode == EVENT_OPCODE_SET_DATE_ADVANCE) {
            if (script_get_variable(EVENT_SCRIPT_VAR_SUPPRESS_PROGRESS_EFFECTS) == 0) {
                script_set_variable(EVENT_SCRIPT_VAR_DATE_ADVANCE, first_operand);
            }
        } else if (opcode == EVENT_OPCODE_TELEPORT_OUT) {
            script_opcode_94_teleport_out(first_operand, 1);
        } else if (opcode == EVENT_OPCODE_TELEPORT_IN) {
            script_opcode_98_teleport_in(first_operand, 1);
        } else if (opcode == EVENT_OPCODE_INFLICT_STATUS) {
            operand_1 = thread_resolve_id(THREAD_SLOT_FIRST_IDLE);
            thread_start(operand_1, &script_inflict_status_thread);
            thread_set_parameters(operand_1, (s32)parameters, 0, 0);
        } else if (opcode == EVENT_OPCODE_UNKNOWN_95) {
        } else {
            break;
        }

        instruction_offset += 1 + g_script_event_instruction_sizes[opcode];
    }

    thread_exit_current();
}
