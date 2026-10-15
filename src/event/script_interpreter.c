/* Event execution and loading, s16 reads and writes, the camera and map light threads, darkness, earthquake, sprite
 * moves, matrix copies, camera focus, unit filters, teleport, warp, rotation, staged status data, inflict status, stat
 * changes and marching units. */

#include "fft/battle.h"
#include "fft/battle_camera.h"
#include "fft/battle_effect.h"
#include "fft/battle_event.h"
#include "fft/battle_gfx.h"
#include "fft/battle_map.h"
#include "fft/battle_menu.h"
#include "fft/battle_move.h"
#include "fft/battle_script.h"
#include "fft/battle_status.h"
#include "fft/battle_unit.h"
#include "fft/camera.h"
#include "fft/event_attack.h"
#include "fft/gfx.h"
#include "fft/main.h"
#include "fft/map.h"
#include "fft/memory.h"
#include "fft/menu.h"
#include "fft/script.h"
#include "fft/sound.h"
#include "fft/status.h"
#include "fft/thread.h"
#include "fft/unit.h"
#include "fft/world.h"
#include "psx/libgte.h"
#include "psx/pad.h"
#include "psx/suzuki.h"
#include "psx/types.h"

static void camera_thread(void);
static void map_light_thread(void);
static void map_wait_for_refresh(void);
static void script_blue_remove_all_enemy_units(void);
static void script_blue_remove_unit(s32 unit_id);
static void script_change_stats(const event_change_stats_parameters_t* parameters);
static void script_color_screen_thread(void);
static void script_color_unit_event_instruction(const event_color_unit_parameters_t* parameters);
static void script_earthquake_start(void);
static void script_face_tile(const event_face_tile_parameters_t* parameters);
static void script_map_darkness(void);
static void script_march_units(const event_march_parameters_t* parameters);
static void script_set_units_movement_effect_suppression(const event_movement_effect_parameters_t* parameters);
static void script_set_units_palette_update_suppression(s32 event_unit_id, s32 enable);
static void script_teleportin_event_instruction(s32 unit_id, s32 unused);
static void script_teleportout_event_instruction(s32 unit_id, s32 remove);
static void status_inflict_by_event_unit_id(s32 event_unit_id, s32 status, s32 which, s32 removal_only);
static void status_queue_current_status_graphics(s32 battle_id, s32 enabled);
static void unit_clear_status_staging_data(void);
static s32 unit_is_moving(s32 runtime_id);
static void unit_update_staged_status_data(u16 unit_id, u16 restore);

/* Empty. EVENT_OPCODE_ADD_UNIT calls it before it looks the unit up. */
static void script_add_unit_stub(void) {
}

#ifdef FFT_BATTLE
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
 * Each pass fetches the opcode at instruction_offset in g_battle_event_block
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
void script_execute_event_variant(void) {
    s32 added_unit_state;
    u8* message_cursor;
    u8* effect_cursor;
    s32 finish_operation;
    s16 first_operand;
    s16 remove_unit_runtime_id;
    s16 reset_graphic_runtime_id;
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
                script_dismiss_unit_event_instruction(first_operand);
            }
        } else if (opcode == EVENT_OPCODE_CALL_FUNCTION) {
            /* These are sequential tests, not switch alternatives: selectors
             * 6 and 14 reuse operand_1 as a loop index, and later tests see
             * its changed value. Preserve that target-visible fallthrough. */
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
                D_800459D8 = 0;
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
                g_sound_music_track_index = 0;
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
                battle_gfx_load_unit_graphics_by_battle_id(8, 1U);
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
                } while (region_runtime_id < EVENT_UNIT_SLOT_COUNT);
            }
            if (operand_1 == EVENT_CALL_FUNCTION_COPY_UNIT_2_POSITION_AND_FACING_TO_UNIT_8_AND_RELOAD_GRAPHICS) {
                unit = battle_unit_get_stats_from_battle_id(1);
                copy_destination = battle_unit_get_stats_from_battle_id(7);
                copy_destination->x = unit->x;
                copy_destination->position.y = unit->position.y;
                copy_destination->position.higher_elevation = unit->position.higher_elevation;
                copy_destination->position.facing = unit->position.facing;
                battle_gfx_load_unit_graphics_by_battle_id(7, 1U);
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
        } else if (opcode == EVENT_OPCODE_END || opcode == EVENT_OPCODE_END_2) {
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
            if (operand_1 != EVENT_ID_OLAN_WOUNDED && operand_1 != EVENT_ID_BEOWULF_INVITATION) {
                battle_gfx_init_evtchr_vram_slots();
            }
            script_set_variable(EVENT_SCRIPT_VAR_BATTLE_NOT_STARTED, 0);
            /* An event with no after-event code, outside events
             * 0x190..0x1a9, checks the scenario conditions and chains into
             * the event they select. */
            finish_operation = (g_main_scenario_event_finish_operations[operand_1] & ~0x0CFF) >> 8;
            if (g_menu_input_mode == MENU_INPUT_PLAYER && finish_operation == 0
                && (operand_1 - EVENT_ID_COMMON_FIRST) >= 0x1AU && operand_1 != 0) {
                g_script_condition_unit_battle_id = 0xFF;
                script_run_scenario_conditions();
                if (script_load_next_event() != 0) {
                    goto restart_script;
                }
            }
            /* Otherwise the event ends: replay the staged statuses and copy
             * each displayed unit's facing quadrant to its battle record. */
            work_index = 0;
            battle_unit_snap_all_facings_to_quadrants();
            unit_update_staged_status_data(0, 1);
            for (; work_index < EVENT_UNIT_SLOT_COUNT; work_index++) {
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
            g_script_opcode_dc_value = 0xD;
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
            message_cursor = (u8*)g_script_event_block;
            message_cursor += instruction_offset;
            g_thread_contexts[operand_1].task_words[5]
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
            script_load_portrait_colors_event_instruction(operand_1);
        }
        /* ---- Map objects ---- */
        else if (opcode == EVENT_OPCODE_USE_3D_OBJECT) {
            g_battle_3d_object_use_request = 1;
            g_battle_event_3d_object_id = operand_1;
            g_battle_event_3d_object_state = operand_2;
            thread_yield();
        } else if (opcode == EVENT_OPCODE_WAIT_3D_OBJECT) {
            g_3d_object_wait_status = 1;
            do {
                thread_yield();
            } while (g_3d_object_wait_status != 0);
        } else if (opcode == EVENT_OPCODE_USE_FIELD_OBJECT) {
            g_battle_field_object_use_request = 1;
            g_battle_event_map_command_83_arg1 = operand_1;
            g_battle_event_map_command_83_arg2 = operand_2;
            thread_yield();
        } else if (opcode == EVENT_OPCODE_WAIT_FIELD_OBJECT) {
            g_field_object_wait_status = 1;
            do {
                thread_yield();
            } while (g_field_object_wait_status != 0);
        }
        /* ---- Unit sprites and animation ---- */
        else if (opcode == EVENT_OPCODE_MIRROR_SPRITE) {
            script_mirrorsprite_event_instruction((const event_mirror_sprite_parameters_t*)parameters);
        } else if (opcode == EVENT_OPCODE_RESET_PALETTE) {
            script_resetpalette_event_instruction((const event_unit_parameters_t*)parameters);
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
            g_map_reload_state = 1;
            do {
                thread_yield();
            } while (g_map_reload_state != 0);
        } else if (opcode == EVENT_OPCODE_APPEND_MAP_STATE) {
            g_map_append_state = 1;
            do {
                thread_yield();
            } while (g_map_append_state != 0);
        } else if (opcode == EVENT_OPCODE_PAUSE) {
            script_pause_event_instruction();
        } else if (opcode == EVENT_OPCODE_RESUME_WAITING_DIALOG) {
            for (work_index = 1; work_index < 16; work_index++) {
                thread = &g_thread_contexts[work_index];
                if (thread->is_running != 0 && thread->task_id == THREAD_TASK_WAIT_FOR_RESUME
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
            effect_cursor = (u8*)g_script_event_block;
            effect_cursor += instruction_offset;
            g_battle_event_effect_target_runtime_id = script_resolve_runtime_id(effect_cursor[3]);
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
            script_opcode_74_event_instruction(parameters);
        }
        /* ---- Unit movement and facing ---- */
        else if (opcode == EVENT_OPCODE_WALK_TO) {
            g_thread_call_target = (void (*)(void))script_walkto_event_instruction;
            thread_call_on_main_stack(parameters);
        } else if (opcode == EVENT_OPCODE_JUMP) {
            script_jump_event_instruction((const event_jump_parameters_t*)parameters);
        } else if (opcode == EVENT_OPCODE_WAIT_WALK) {
            script_waitwalk_event_instruction(first_operand);
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
            script_waitrotateunit_and_waitrotateall_event_instruction(first_operand);
        } else if (opcode == EVENT_OPCODE_WAIT_ROTATE_ALL) {
            script_waitrotateunit_and_waitrotateall_event_instruction(-1);
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
            thread_start(operand_1, &script_color_screen_thread);
            g_thread_contexts[operand_1].function_parameter_1 = (s32)parameters;
        } else if (opcode == EVENT_OPCODE_EARTHQUAKE_START) {
            operand_1 = thread_resolve_id(THREAD_SLOT_FIRST_IDLE);
            thread_start(operand_1, &script_earthquake_start);
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
            g_thread_contexts[operand_1].task_words[0] = script_resolve_runtime_id(first_operand);
        } else if (opcode == EVENT_OPCODE_WAIT_SPRITE_MOVE) {
            script_waitspritemove_event_instruction(first_operand);
        } else if (opcode == EVENT_OPCODE_MAP_LIGHT) {
            operand_1 = thread_resolve_id(THREAD_SLOT_FIRST_IDLE);
            thread_start(operand_1, &map_light_thread);
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
                parameters[0], parameters[4], ((s8)parameters[1]), ((s8)parameters[2]), ((s8)parameters[3]));
        } else if (opcode == EVENT_OPCODE_COLOR_UNIT) {
            script_color_unit_event_instruction((const event_color_unit_parameters_t*)parameters);
        } else if (opcode == EVENT_OPCODE_RESUME_UNIT_PALETTE_UPDATES) {
            script_set_units_palette_update_suppression((u16)first_operand, 0);
        } else if (opcode == EVENT_OPCODE_SUPPRESS_UNIT_PALETTE_UPDATES) {
            script_set_units_palette_update_suppression((u16)first_operand, 1);
        } else if (opcode == EVENT_OPCODE_COLOR_FIELD) {
            battle_map_color_field(
                parameters[0], parameters[4], ((s8)parameters[1]), ((s8)parameters[2]), ((s8)parameters[3]));
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
            thread_start(operand_1, &sound_run_background_sfx_thread);
            thread_set_parameters(operand_1, (s32)parameters, 0, 0);
        } else if (opcode == EVENT_OPCODE_EDIT_BG_SOUND) {
            operand_1 = thread_resolve_id(THREAD_SLOT_FIRST_IDLE);
            thread_start(operand_1, &sound_run_background_fade_thread);
            thread_set_parameters(operand_1, (s32)parameters, 0, 0);
        } else if (opcode == EVENT_OPCODE_SWITCH_TRACK) {
            g_sound_music_switch_request = operand_1;
            g_battle_event_music_switch_volume = operand_2;
            g_battle_event_music_switch_fade = operand_3;
            thread_yield();
        } else if (opcode == EVENT_OPCODE_FADE_SOUND) {
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
                    operand_1 = battle_unit_get_battle_id_by_runtime_id(work_index);
                    battle_unit_remove_by_runtime_id(work_index);
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
            reset_graphic_runtime_id = script_resolve_runtime_id(first_operand);
            if (reset_graphic_runtime_id != EVENT_RUNTIME_ID_NONE) {
                battle_gfx_reset_unit_graphic_trigger(reset_graphic_runtime_id);
            }
        } else if (opcode == EVENT_OPCODE_ADD_GHOST_UNIT) {
            ghost_parameters = instruction_offset + ((u8*)g_script_event_block) + 3;
            /* An empty four-pass countdown precedes the search for a free
             * runtime slot. */
            ghost_runtime_id = 3;
            do {
                ghost_runtime_id -= 1;
            } while (ghost_runtime_id >= 0);
            for (ghost_runtime_id = 0; ghost_runtime_id < EVENT_UNIT_SLOT_COUNT; ghost_runtime_id++) {
                if (battle_unit_has_runtime_id(ghost_runtime_id) == 0) {
                    g_script_unit_slots[ghost_parameters[0]] = ghost_runtime_id;
                    break;
                }
            }
            if (ghost_runtime_id != EVENT_UNIT_SLOT_COUNT) {
                battle_script_add_ghost_unit_event_instruction(ghost_parameters[1], ghost_parameters[2],
                    ghost_parameters[3], ghost_parameters[4], first_operand, g_script_unit_slots[ghost_parameters[0]],
                    ghost_parameters[5]);
                g_unit_graphics_load_pending = 1U;
            } else {
                /* No free runtime slot: the instruction is retried at once,
                 * with no yield in between. */
                continue;
            }
        } else if (opcode == EVENT_OPCODE_UNKNOWN_75) {
        } else if (opcode == EVENT_OPCODE_ADD_UNIT) {
            script_add_unit_stub();
            added_unit = battle_unit_find_stats_by_entd_unit_id(first_operand, &added_unit_state);
            if ((added_unit_state == -2) || (added_unit_state >= 0)) {
                for (added_battle_id = 0; added_battle_id < EVENT_UNIT_SLOT_COUNT; added_battle_id++) {
                    if (battle_unit_get_stats_from_battle_id(added_battle_id) == added_unit)
                        break;
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
            /* Direct simulation slot, not the encoded identity used by WarpUnit.
             * Operand byte 1 is unused. The target neither guards slots >=21
             * nor synchronizes the renderer's position here. */
            unit = battle_unit_get_stats_from_battle_id(operand_1);
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
            thread_start(operand_1, script_add_unit_start_thread);
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
            if (script_is_tutorial_event_slot() != 0 && first_operand >= 3) {
                thread_wait_frames(first_operand / battle_state_get_animation_speed());
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
            script_wait_value_event_instruction((const event_wait_value_parameters_t*)parameters);
        } else if (opcode == EVENT_OPCODE_EVTCHR_PALETTE) {
            evtchr_palette_runtime_id = script_resolve_runtime_id(first_operand);
            if (evtchr_palette_runtime_id != EVENT_RUNTIME_ID_NONE) {
                battle_gfx_set_evtchr_palette_by_runtime_id(evtchr_palette_runtime_id, operand_3, operand_4);
            }
        } else if (opcode == EVENT_OPCODE_LOAD_ATTACK_GRAPHICS) {
            script_load_attack_graphics_event_instruction();
        } else if (opcode == EVENT_OPCODE_CHANGE_STATS) {
            script_change_stats((const event_change_stats_parameters_t*)parameters);
        } else if (opcode == EVENT_OPCODE_PLAY_TUNE) {
            main_sound_play_tune(operand_1);
        } else if (opcode == EVENT_OPCODE_UNLOCK_DATE) {
            if ((script_get_variable(EVENT_SCRIPT_VAR_SUPPRESS_PROGRESS_EFFECTS) == 0) && (operand_1 != 0)) {
                date_month = script_get_variable(EVENT_SCRIPT_VAR_MONTH);
                script_unlockdate_event_instruction(g_main_treasure_acquisition_date_bits, operand_1 - 1, date_month,
                    script_get_variable(EVENT_SCRIPT_VAR_DAY));
            }
        } else if (opcode == EVENT_OPCODE_TEMP_WEAPON) {
            thrown_item_runtime_id = script_resolve_runtime_id(first_operand);
            if (thrown_item_runtime_id != EVENT_RUNTIME_ID_NONE) {
                battle_gfx_set_thrown_item_palette_by_runtime_id(operand_3, thrown_item_runtime_id);
            }
        } else if (opcode == EVENT_OPCODE_ARROW) {
            arrow_target_runtime_id = script_resolve_runtime_id(first_operand);
            if (arrow_target_runtime_id != EVENT_RUNTIME_ID_NONE) {
                operand_1 = battle_unit_get_battle_id_by_runtime_id(arrow_target_runtime_id);
                arrow_shooter_runtime_id
                    = script_resolve_runtime_id(script_read_s16(instruction_offset + ((u8*)g_script_event_block) + 3));
                if (arrow_shooter_runtime_id != EVENT_RUNTIME_ID_NONE) {
                    arrow_shooter_battle_id = battle_unit_get_battle_id_by_runtime_id(arrow_shooter_runtime_id);
                    g_thread_call_target = (void (*)(void))battle_effect_set_arrow_trajectory;
                    thread_call_on_main_stack(arrow_shooter_battle_id, &g_unit_scratch_coords, operand_1);
                }
            }
        } else if (opcode == EVENT_OPCODE_MAP_FREEZE) {
            battle_map_freeze();
        } else if (opcode == EVENT_OPCODE_MAP_UNFREEZE) {
            battle_map_unfreeze();
        } else if (opcode == EVENT_OPCODE_UNIT_ANIM_ROTATE) {
            script_unit_animation_rotate_event_instruction((const event_unit_anim_rotate_parameters_t*)parameters);
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
                || ((confirm_buttons = g_script_unfiltered_controller_input) & PSX_PAD_CIRCLE) == 0
                || (confirm_buttons & (PSX_PAD_UP | PSX_PAD_RIGHT | PSX_PAD_DOWN | PSX_PAD_LEFT)) != 0) {
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
            script_teleportout_event_instruction(first_operand, 1);
        } else if (opcode == EVENT_OPCODE_TELEPORT_IN) {
            script_teleportin_event_instruction(first_operand, 1);
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
#endif

#ifdef FFT_WORLD
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
    s32 unit_lookup_status;
    s32 ghost_runtime_id;
    s32 region_runtime_id;
    s16 zalera_runtime_id;
    s32 month;
    s32 appearance_battle_id;
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
    event_display_message_instruction_t* dialogue_instruction;
    s32* event_words;
    u8* parameters;
    /* One unit pointer, one thread pointer and one index serve every handler;
     * separate ones per handler change the saved-register allocation. */
    unit_stats_t* unit;
    u8* instruction;
    u8* ghost_operands;
    map_tile_t* tile;
    unit_stats_t* copy_destination;
    u8* focus_instruction;
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
restart_event:
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
                script_dismiss_unit_event_instruction(first_operand);
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
                D_800459D8 = 0;
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
                } while (region_runtime_id < EVENT_UNIT_SLOT_COUNT);
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
                zalera_runtime_id = script_resolve_runtime_id(CHARACTER_IDENTITY_ZALERA);
                if (zalera_runtime_id != EVENT_RUNTIME_ID_NONE) {
                    status_queue_current_status_graphics(battle_unit_get_battle_id_by_runtime_id(zalera_runtime_id), 0);
                    g_thread_call_target = (void (*)(void))battle_unit_update_graphics_by_runtime_id;
                    thread_call_on_main_stack(zalera_runtime_id);
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
                    goto restart_event;
                }
            }
            work_index = 0;
            battle_unit_snap_all_facings_to_quadrants();
            unit_update_staged_status_data(0, 1);
            for (; work_index < EVENT_UNIT_SLOT_COUNT; work_index++) {
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
            dialogue_instruction = (event_display_message_instruction_t*)dialogue_cursor;
            operand_4 = dialogue_instruction->unit_id_le[1];
            operand_3 = dialogue_instruction->unit_id_le[0];
            g_thread_contexts[operand_1].function_parameter_3 = (operand_4 << 8) + operand_3;
            g_thread_contexts[operand_1].function_parameter_4 = dialogue_instruction->portrait_selector;
            g_thread_contexts[operand_1].task_words[2] = script_read_s16(dialogue_instruction->x_offset_le);
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
            script_load_portrait_colors_event_instruction(operand_1);
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
            script_mirrorsprite_event_instruction((const event_mirror_sprite_parameters_t*)parameters);
        } else if (opcode == EVENT_OPCODE_RESET_PALETTE) {
            script_resetpalette_event_instruction((const event_unit_parameters_t*)parameters);
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
            script_pause_event_instruction();
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
            focus_instruction = (u8*)g_script_event_block;
            focus_instruction += instruction_offset;
            g_event_effect_target_x = focus_instruction[4];
            g_event_effect_target_y = focus_instruction[5];
            g_event_effect_target_mode = focus_instruction[6];
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
            event_words = g_script_variables;
            script_focus_speed(
                parameters, event_words + EVENT_SCRIPT_VAR_CAMERA_X, event_words + EVENT_SCRIPT_VAR_CAMERA_TILT);
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
            script_opcode_74_event_instruction(parameters);
        }
        /* ---- unit movement and facing ---- */
        else if (opcode == EVENT_OPCODE_WALK_TO) {
        } else if (opcode == EVENT_OPCODE_JUMP) {
            script_jump_event_instruction((const event_jump_parameters_t*)parameters);
        } else if (opcode == EVENT_OPCODE_WAIT_WALK) {
            script_waitwalk_event_instruction(first_operand);
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
            script_waitrotateunit_and_waitrotateall_event_instruction(first_operand);
        } else if (opcode == EVENT_OPCODE_WAIT_ROTATE_ALL) {
            script_waitrotateunit_and_waitrotateall_event_instruction(-1);
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
            script_waitspritemove_event_instruction(first_operand);
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
            script_color_unit_event_instruction((const event_color_unit_parameters_t*)parameters);
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
            zalera_runtime_id = script_resolve_runtime_id(first_operand);
            if (zalera_runtime_id != EVENT_RUNTIME_ID_NONE) {
                battle_unit_draw_and_activate_by_runtime_id(zalera_runtime_id);
                if (script_get_variable(EVENT_SCRIPT_VAR_CURRENT_EVENT) == EVENT_ID_LIMBERRY_CEMETERY_BATTLE) {
                    status_queue_current_status_graphics(battle_unit_get_battle_id_by_runtime_id(zalera_runtime_id), 1);
                    g_thread_call_target = (void (*)(void))battle_unit_update_graphics_by_runtime_id;
                    thread_call_on_main_stack(zalera_runtime_id);
                }
            }
        } else if (opcode == EVENT_OPCODE_ERASE) {
            erased_runtime_id = script_resolve_runtime_id(first_operand);
            if (erased_runtime_id != EVENT_RUNTIME_ID_NONE) {
                battle_gfx_reset_unit_graphic_trigger(erased_runtime_id);
            }
        } else if (opcode == EVENT_OPCODE_ADD_GHOST_UNIT) {
            ghost_operands = (u8*)g_script_event_block + instruction_offset + 3;
            ghost_runtime_id = 3;
            do {
                --ghost_runtime_id;
            } while (ghost_runtime_id >= 0);
            for (ghost_runtime_id = 0; ghost_runtime_id < EVENT_UNIT_SLOT_COUNT; ghost_runtime_id++) {
                if (battle_unit_has_runtime_id(ghost_runtime_id) == 0) {
                    g_script_unit_slots[ghost_operands[0]] = ghost_runtime_id;
                    break;
                }
            }
            if (ghost_runtime_id != EVENT_UNIT_SLOT_COUNT) {
                battle_script_add_ghost_unit_event_instruction(ghost_operands[1], ghost_operands[2], ghost_operands[3],
                    ghost_operands[4], first_operand, g_script_unit_slots[ghost_operands[0]], ghost_operands[5]);
                g_unit_graphics_load_pending = 1U;
            } else {
                continue;
            }
        } else if (opcode == EVENT_OPCODE_UNKNOWN_75) {
        } else if (opcode == EVENT_OPCODE_ADD_UNIT) {
            script_add_unit_stub();
            appearance_unit = battle_unit_find_stats_by_entd_unit_id(first_operand, &unit_lookup_status);
            if ((unit_lookup_status == -2) || (unit_lookup_status >= 0)) {
                for (appearance_battle_id = 0; appearance_battle_id < EVENT_UNIT_SLOT_COUNT; appearance_battle_id++) {
                    if (battle_unit_get_stats_from_battle_id(appearance_battle_id) == appearance_unit) {
                        break;
                    }
                }
                operand_1 = battle_unit_get_runtime_id_by_battle_id(appearance_battle_id);
                if (operand_1 == -1) {
                    unit_save_staged_status_data(appearance_battle_id);
                    battle_gfx_load_unit_graphics_by_battle_id(appearance_battle_id, operand_3);
                    g_unit_graphics_load_pending = 1U;
                } else {
                    if (operand_3 == 0) {
                        battle_unit_draw_and_activate_by_runtime_id(operand_1);
                    }
                    if (unit_save_staged_status_data(appearance_battle_id) != 0) {
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
                thread_wait_frames(first_operand / battle_state_get_animation_speed());
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
            script_wait_value_event_instruction((const event_wait_value_parameters_t*)parameters);
        } else if (opcode == EVENT_OPCODE_EVTCHR_PALETTE) {
            runtime_id_for_7f = script_resolve_runtime_id(first_operand);
            if (runtime_id_for_7f != EVENT_RUNTIME_ID_NONE) {
                battle_gfx_set_evtchr_palette_by_runtime_id(runtime_id_for_7f, operand_3, operand_4);
            }
        } else if (opcode == EVENT_OPCODE_LOAD_ATTACK_GRAPHICS) {
            script_load_attack_graphics_event_instruction();
        } else if (opcode == EVENT_OPCODE_CHANGE_STATS) {
            script_change_stats((const event_change_stats_parameters_t*)parameters);
        } else if (opcode == EVENT_OPCODE_PLAY_TUNE) {
            main_sound_play_tune(operand_1);
        } else if (opcode == EVENT_OPCODE_UNLOCK_DATE) {
            if ((script_get_variable(EVENT_SCRIPT_VAR_SUPPRESS_PROGRESS_EFFECTS) == 0) && (operand_1 != 0)) {
                month = script_get_variable(EVENT_SCRIPT_VAR_MONTH);
                script_unlockdate_event_instruction(g_main_treasure_acquisition_date_bits, operand_1 - 1, month,
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
            script_unit_animation_rotate_event_instruction((const event_unit_anim_rotate_parameters_t*)parameters);
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
            script_teleportout_event_instruction(first_operand, 1);
        } else if (opcode == EVENT_OPCODE_TELEPORT_IN) {
            script_teleportin_event_instruction(first_operand, 1);
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
#endif

s32 script_load_next_event(void) {
    s32 task_id;
    s32 event_id;

    do {
        thread_yield();
        for (task_id = 4; task_id < 15; task_id++) {
            if (thread_find_running_by_task(task_id) != 0) {
                break;
            }
        }
    } while (task_id != 15);

    event_id = script_get_variable(EVENT_SCRIPT_VAR_CURRENT_EVENT);
    if (event_id == g_script_loaded_event_id) {
        return 0;
    }
    script_load_event(event_id);
    g_script_loaded_event_id = event_id;
    return 1;
}

/* Test one runtime ID; -1 scans all 21 event slots. */
static s32 unit_is_moving(s32 runtime_id) {
    s32 i;

    if (runtime_id != -1) {
        if (battle_move_is_unit_moving_by_runtime_id(runtime_id) != 0) {
            return 1;
        }
    } else {
        for (i = 0; i < BATTLE_UNIT_SLOT_COUNT; i++) {
            if ((battle_unit_has_runtime_id(i) != 0) && (battle_move_is_unit_moving_by_runtime_id(i) != 0)) {
                return 1;
            }
        }
    }
    return 0;
}

s16 script_read_s16(const void* source) {
    const u8* bytes = source;

    return (bytes[0] | (bytes[1] << 8));
}

/* Store a 16-bit value at an unaligned script address, low byte first. */
static void script_write_s16(u8* destination, s16 value) {
    destination[0] = value;
    destination[1] = value >> 8;
}

/* Yield until this scene's pending map refresh clears. */
static void map_wait_for_refresh(void) {
    if (g_map_refresh_pending != 0) {
        do {
            thread_yield();
        } while (g_map_refresh_pending != 0);
    }
}

/* Clear bit 0 of the zoom variable selected by the camera component-index table. */
void camera_clear_zoom_variable_low_bit(void) {
    g_script_variables[g_camera_script_variable_indices[EVENT_CAMERA_COMPONENT_ZOOM]] &= ~1;
}

#define ABS(x) (x < 0 ? -(x) : x)

/*
 * The event CAMERA instruction (0x19) thread.
 * Instruction-for-instruction twin of camera_thread (0x800f9438).
 *
 * The script supplies seven camera-component targets and a frame count; 10000
 * keeps a component's current value. The packed speed-curve byte selects the
 * interpolation mode (bits 0-1), the component group left linear (bits 2-3)
 * and the easing weight (bits 4-7). Translation components (0-2) use a finer
 * fixed point than the rotation components.
 *
 * Allocation-sensitive spellings: ABS() as a conditional expression keeps the
 * target's copy-then-negate sequences; the explicit shift sign extension keeps
 * CSE from reusing a 16-bit copy of angle for yaw; accel is shared by the
 * mode 1 and late mode 2 paths while first and elapsed are block-local, which
 * gives the target's s0-s3 assignment.
 */
static void camera_thread(void) {
    s32 target[EVENT_CAMERA_COMPONENT_COUNT + 1];
    s32 start[EVENT_CAMERA_COMPONENT_COUNT];
    s32 i;
    s32 j;
    s32* variable;
    s32 group;
    s32 ease;
    s32 mode;
    s32 ramp;
    s32 weight;
    s32 duration;
    s32 half;
    s32 first;
    s32 accel;
    s32 elapsed;
    s32 delta;
    s32 decel;
    s32 base;
    s32 value;
    s32 angle;
    s16 yaw;
    s32 yaw_delta;
    const event_camera_parameters_t* parameters;
    const u8* script;

    mode = g_camera_speed_curve & EVENT_CAMERA_SPEED_CURVE_MODE_MASK;
    group = (g_camera_speed_curve & EVENT_CAMERA_SPEED_CURVE_GROUP_MASK) >> EVENT_CAMERA_SPEED_CURVE_GROUP_SHIFT;
    ease = (g_camera_speed_curve & EVENT_CAMERA_SPEED_CURVE_EASING_MASK) >> EVENT_CAMERA_SPEED_CURVE_EASING_SHIFT;
    thread_set_current_task_id(THREAD_TASK_CAMERA);
    parameters = (const event_camera_parameters_t*)thread_get_current_parameter_1();
    script = parameters->values.position_le[0];
    for (i = 0; i < EVENT_CAMERA_COMPONENT_COUNT + 1; i++) {
        target[i] = script_read_s16(script);
        script += 2;
    }

    if (g_camera_wrap_yaw_pending != 0) {
        g_camera_wrap_yaw_pending = 0;
        angle = script_get_variable(EVENT_SCRIPT_VAR_CAMERA_YAW);
        yaw_delta = ((angle << 16) >> 16) - target[EVENT_CAMERA_COMPONENT_YAW];
        yaw = angle;
        if (ABS(yaw_delta + ONE) < ABS(yaw_delta)) {
            yaw = angle + ONE;
        }
        script_set_variable(EVENT_SCRIPT_VAR_CAMERA_YAW, yaw);
    }

    for (i = 0; i < EVENT_CAMERA_COMPONENT_COUNT; i++) {
        start[i] = g_script_variables[g_camera_script_variable_indices[i]];
    }
    for (i = 0; i < EVENT_CAMERA_POSITION_COMPONENT_COUNT; i++) {
        start[i] &= ~0xFFF;
        start[i] /= 4;
    }

    duration = target[EVENT_CAMERA_COMPONENT_COUNT];
    for (i = 0; i < target[EVENT_CAMERA_COMPONENT_COUNT]; i++) {
        weight = 16 - ease;
        ramp = (duration - i) * i;
        for (j = 0; j < EVENT_CAMERA_COMPONENT_COUNT; j++) {
            variable = &g_script_variables[g_camera_script_variable_indices[j]];
            if (target[j] == CAMERA_FUSION_HOLD_SENTINEL) {
                continue;
            }
            if (g_camera_position_locked != 0 && j < EVENT_CAMERA_POSITION_COMPONENT_COUNT) {
                continue;
            }
            if (j < EVENT_CAMERA_POSITION_COMPONENT_COUNT) {
                delta = (target[j] << 8) - start[j];
                base = start[j];
                if (ABS(delta) < 0x600) {
                    continue;
                }
            } else {
                delta = (target[j] - start[j]) << 6;
                base = start[j] << 6;
                if (ABS(delta) < 0x60) {
                    continue;
                }
            }
            if (mode == 0 || (group == 0 && j >= EVENT_CAMERA_COMPONENT_TILT)
                || (group == 1 && j < EVENT_CAMERA_POSITION_COMPONENT_COUNT)) {
                value = mul_div_s64(delta, i, duration) + base;
                if (j < EVENT_CAMERA_POSITION_COMPONENT_COUNT) {
                    value *= 4;
                }
            } else if (mode == 1) {
                decel = delta * weight;
                accel = delta * ease * 2;
                first = mul_div_s64(accel + decel + decel, i, duration * 32);
                value = mul_div_s64(accel, ramp, duration * duration * 32) + first + base;
                if (j < EVENT_CAMERA_POSITION_COMPONENT_COUNT) {
                    value *= 4;
                }
            } else {
                delta *= 2;
                half = i * 2;
                if (half < duration) {
                    decel = delta * weight;
                    value = mul_div_s64(
                        mul_div_s64(delta * ease * 2, half, duration) + decel + decel, half, duration * 32);
                    if (j < EVENT_CAMERA_POSITION_COMPONENT_COUNT) {
                        value += base * 4;
                    } else {
                        value = value / 4 + base;
                    }
                } else {
                    decel = delta * weight;
                    accel = delta * ease * 2;
                    elapsed = half - duration;
                    value = mul_div_s64(accel + mul_div_s64(accel, duration - elapsed, duration) + decel + decel,
                        elapsed, duration * 32);
                    if (j < EVENT_CAMERA_POSITION_COMPONENT_COUNT) {
                        value += delta;
                        value += base * 4;
                    } else {
                        value = (value + delta) / 4 + base;
                    }
                }
            }
            if (j >= EVENT_CAMERA_COMPONENT_TILT) {
                if (value > 0) {
                    if ((value & 0xFF) > 0x20) {
                        value += 0x40;
                    }
                } else if ((value & 0xFF) < 0x20) {
                    value -= 0x40;
                }
                value /= 64;
            }
            *variable = value;
        }
        camera_clear_zoom_variable_low_bit();
        thread_yield();
    }

    if (g_camera_position_locked == 0) {
        for (i = 0; i < EVENT_CAMERA_POSITION_COMPONENT_COUNT; i++) {
            if (target[i] != CAMERA_FUSION_HOLD_SENTINEL) {
                g_script_variables[g_camera_script_variable_indices[i]] = target[i] << 10;
            }
        }
    }
    for (i = EVENT_CAMERA_COMPONENT_TILT; i < EVENT_CAMERA_COMPONENT_COUNT; i++) {
        if (target[i] != CAMERA_FUSION_HOLD_SENTINEL) {
            g_script_variables[g_camera_script_variable_indices[i]] = target[i];
        }
    }
    camera_clear_zoom_variable_low_bit();
    thread_yield();
    thread_exit_current();
}

/* Thread body of MapDarkness: start the ambient-light blend described by the
 * operands (blend mode, red, green, blue as s8, duration index), then keep the
 * thread alive under the MapDarkness task id for
 * g_script_map_darkness_durations[duration index] frames (1 to 256). WORLD's copy
 * also calls the BATTLE blend at 0x800934d0, which is WLDCORE data there. */
static void script_map_darkness(void) {
    event_unit_color_t* parameters;

    thread_set_current_task_id(THREAD_TASK_MAP_DARKNESS);
    parameters = (event_unit_color_t*)thread_get_current_parameter_1();
    battle_map_start_ambient_light_blend(
        parameters->mode, parameters->duration, parameters->rgb.r, parameters->rgb.g, parameters->rgb.b);
    thread_wait_frames(g_script_map_darkness_durations[parameters->duration]);
    thread_exit_current();
}

/* Two independent frame counters apply the two shake pulses. The task loops
 * until another instruction tears it down. */
static void script_earthquake_start(void) {
    event_earthquake_parameters_t* parameters;
    s32 first_counter;
    s32 second_counter;

    thread_set_current_task_id(THREAD_TASK_EARTHQUAKE);
    parameters = (event_earthquake_parameters_t*)thread_get_current_parameter_1();
    first_counter = 0;
    second_counter = 0;
    for (;;) {
        thread_yield();
        first_counter++;
        if (first_counter == parameters->pulses[0].frames) {
            g_camera_shake_z_offset += parameters->pulses[0].z_delta;
            first_counter = 0;
        }
        second_counter++;
        if (second_counter == parameters->pulses[1].frames) {
            g_camera_shake_z_offset += parameters->pulses[1].z_delta;
            second_counter = 0;
        }
    }
}

/* Advance the event screen-color fade by two frames per step. */
static void script_color_screen_thread(void) {
    struct {
        s32 start_and_end[6];
        s32 current[3];
    } color;
    u8* parameters;
    s32 mode;
    s32 i;
    s32 j;
    s32 duration;

    thread_set_current_task_id(THREAD_TASK_COLOR_SCREEN);
    parameters = (u8*)thread_get_current_parameter_1();
    /* Separate record and RGB cursors drop the original argument-register copy. */
    mode = *parameters++;
    for (i = 0; i < 6; i++) {
        color.start_and_end[i] = *parameters++;
    }
    /* The RGB scan leaves the cursor at duration_le. */
    duration = script_read_s16(parameters);
    for (i = 0; i < duration; i += 2) {
        thread_yield();
        thread_yield();
        for (j = 0; j < 3; j++) {
            s32 end = color.start_and_end[j + 3];
            s32 start = color.start_and_end[j];
            color.current[j] = (end - start) * i / duration + start;
        }
        battle_gfx_start_screen_color_modulation_fade(mode, color.current[0], color.current[1], color.current[2], 2);
    }
    for (j = 0; j < 3; j++) {
        color.current[j] = color.start_and_end[j + 3];
    }
    battle_gfx_start_screen_color_modulation_fade(mode, color.current[0], color.current[1], color.current[2], 2);
    thread_yield();
    thread_yield();
    thread_exit_current();
}

/*
 * Sprite-move worker called by script_sprite_move (mode 0) and
 * script_sprite_move_beta (mode 1) with the script cursor: a unit
 * 16-bit value, three 16-bit destination coordinates, an easing byte, a weight byte and a
 * 16-bit value that is a frame count (mode 0) or a speed (mode 1). pos[0] is the
 * start, pos[1] the destination, pos[2] the current position and pos[3] the
 * negated previous step, re-applied each frame so the unit's coordinate is
 * rewritten rather than accumulated. Rows are 16 bytes apart in the target.
 *
 * WORLD builds it with the divcheck profile, which adds the checked-division
 * guard after SquareRoot0; BATTLE builds the same source without it.
 */
/* The parameters are copied into locals in this order: it is what places the
 * s0 copy before the s2 copy in the prologue. */
static void script_run_sprite_move(const event_sprite_move_parameters_t* arg, s32 use_speed) {
    s32 pos[4][4];
    SVECTOR vec;
    /* Indexed destination fields change allocation and shorten the target by four bytes. */
    const u8* parameters;
    s32 mode;
    SVECTOR* coords;
    s32 runtime_id;
    s32 weight;
    s32 curve;
    s32 total;
    s32 i;
    s32 j;
    s32 delta;
    s32 weighted;
    s32 scaled;
    s32 step;
    s32 mid;
    s32 elapsed;
    s32 half;

    parameters = arg->unit_id_le;
    mode = use_speed;
    runtime_id = script_resolve_runtime_id(script_read_s16(parameters));
    if (runtime_id == EVENT_RUNTIME_ID_NONE) {
        return;
    }
    g_thread_contexts[g_thread_current_id].task_words[0] = runtime_id;
    coords = battle_unit_get_event_offset_by_runtime_id(runtime_id);
    parameters += sizeof(arg->unit_id_le);
    pos[0][0] = coords->vx;
    pos[0][1] = coords->vy;
    pos[0][2] = coords->vz;
    pos[3][0] = -coords->vx;
    pos[3][1] = -coords->vy;
    pos[3][2] = -coords->vz;
    for (i = 0; i < 3; i++) {
        pos[1][i] = script_read_s16(parameters);
        parameters += sizeof(arg->destination_le[0]);
    }
    curve = *parameters++;
    weight = *parameters++;
    total = script_read_s16(parameters);
    if (mode == 1) {
        for (j = 0; j < 3; j++) {
            pos[2][j] = ((pos[1][j] - pos[0][j]) * (pos[1][j] - pos[0][j])) << 4;
        }
        total = SquareRoot0(pos[2][0] + pos[2][1] + pos[2][2]) / total;
    }
    for (i = 0; i < 3; i++) {
        pos[0][i] <<= 8;
        pos[1][i] <<= 8;
    }
    for (i = 1; i < total; i++) {
        thread_yield();
        for (j = 0; j < 3; j++) {
            delta = pos[1][j] - pos[0][j];
            if (curve == 0) {
                pos[2][j] = mul_div_s64(delta, i, total) + pos[0][j];
            } else if (curve == 1) {
                weighted = delta * (16 - weight);
                scaled = (delta * weight) * 2;
                step = mul_div_s64(scaled + weighted + weighted, i, total * 32);
                step += mul_div_s64(scaled, (total - i) * i, (total * 32) * total);
                pos[2][j] = step + pos[0][j];
            } else if (curve == 3) {
                weighted = delta * (16 - weight);
                step = mul_div_s64((delta * weight) * 2, i, total);
                step = mul_div_s64(step + weighted + weighted, i, total * 32);
                pos[2][j] = step + pos[0][j];
            } else if (curve == 2) {
                mid = total / 2;
                half = delta + ((u32)delta >> 31);
                if (mid == 0) {
                    mid = 1;
                }
                if (i * 2 < total) {
                    weighted = delta * (16 - weight);
                    step = mul_div_s64((delta * weight) * 2, i, mid);
                    step = mul_div_s64(step + weighted + weighted, i, mid * 64);
                    pos[2][j] = step + pos[0][j];
                } else {
                    weighted = delta * (16 - weight);
                    scaled = (delta * weight) * 2;
                    elapsed = i - mid;
                    step = mul_div_s64(scaled, mid - elapsed, mid);
                    step = mul_div_s64(scaled + step + weighted + weighted, elapsed, mid * 64);
                    /* Use: one extra reference lifts half above j and total for the target's s6/s7/fp. */
                    __asm__("" : : "r"(half));
                    pos[2][j] = step + pos[0][j] + (half >> 1);
                }
            }
        }
        if (battle_unit_has_runtime_id(runtime_id) == 0) {
            thread_exit_current();
        }
        vec.vx = pos[3][0];
        vec.vy = pos[3][1];
        vec.vz = pos[3][2];
        battle_unit_add_event_offset_by_runtime_id(runtime_id, &vec);
        vec.vx = pos[2][0] / 256;
        vec.vy = pos[2][1] / 256;
        vec.vz = pos[2][2] / 256;
        battle_unit_add_event_offset_by_runtime_id(runtime_id, &vec);
        pos[3][0] = -vec.vx;
        pos[3][1] = -vec.vy;
        pos[3][2] = -vec.vz;
    }
    if (battle_unit_has_runtime_id(runtime_id) == 0) {
        thread_exit_current();
    }
    vec.vx = pos[3][0];
    vec.vy = pos[3][1];
    vec.vz = pos[3][2];
    battle_unit_add_event_offset_by_runtime_id(runtime_id, &vec);
    vec.vx = pos[1][0] / 256;
    vec.vy = pos[1][1] / 256;
    vec.vz = pos[1][2] / 256;
    battle_unit_add_event_offset_by_runtime_id(runtime_id, &vec);
    if (total == 1) {
        thread_yield();
    }
}

/* Thread body of SpriteMove: run the sprite move with the last operand taken as a
 * frame count. */
void script_sprite_move(void) {
    thread_set_current_task_id(THREAD_TASK_SPRITE_MOVE);
    script_run_sprite_move((const event_sprite_move_parameters_t*)thread_get_current_parameter_1(), 0);
    thread_exit_current();
}

/* Thread body of SpriteMoveBeta: run the sprite move with the last operand taken
 * as a speed. */
void script_sprite_move_beta(void) {
    thread_set_current_task_id(THREAD_TASK_SPRITE_MOVE);
    script_run_sprite_move((const event_sprite_move_parameters_t*)thread_get_current_parameter_1(), 1);
    thread_exit_current();
}

/* WaitSpriteMove: yield until no thread is still running the sprite-move task
 * for the calling event's sprite. A unit id that does not resolve has nothing
 * to wait for. */
void script_waitspritemove_event_instruction(s32 unit_id) {
    s32 runtime_id;
    s32 i;

    runtime_id = script_resolve_runtime_id(unit_id);
    if (runtime_id == EVENT_RUNTIME_ID_NONE) {
        return;
    }
    for (;;) {
        for (i = 0; i < VARIANT_THREAD_TABLE_SLOT_COUNT; i++) {
            if (thread_is_running_asm(i) != 0 && g_thread_contexts[i].task_id == THREAD_TASK_SPRITE_MOVE
                && g_thread_contexts[i].task_words[0] == runtime_id) {
                break;
            }
        }
        if (i == VARIANT_THREAD_TABLE_SLOT_COUNT) {
            return;
        }
        thread_yield();
    }
}

static void script_copy_matrix(MATRIX* destination, const MATRIX* source) {
    mem_copy_bytes_nonempty(destination, source, sizeof(*destination));
}

/*
 * Scenario thread task 7: interpolates the map light color and its three
 * light vectors from their current values to six script-supplied targets over
 * the script's frame count, rebuilding the GTE light matrix each frame, then
 * commits the final values back to the live light state.
 */
static void map_light_thread(void) {
    union {
        struct {
            struct {
                s32 rotation[3];
                s32 scale[3];
            } start, target;
        } vectors;
        s32 values[12];
    } light_values;
    SVECTOR color;
    MATRIX rotation;
    MATRIX matrix;
    VECTOR translation;
    s32 step;
    s32 frames;
    const u8* script;
    s32* target;

    thread_set_current_task_id(THREAD_TASK_MAP_LIGHT);
    step = 0;
    script = (const u8*)thread_get_current_parameter_1();
    /* Indexed operand fields retain a separate base and add four bytes. */
    target = light_values.values;
    do {
        target[6] = script_read_s16(script);
        script += 2;
        step++;
        target++;
    } while (step < 6);
    /* The six-target scan leaves the cursor at duration_le. */
    frames = script_read_s16(script);
    step = 0;
    light_values.vectors.start.rotation[0] = g_script_light_angles.vx;
    light_values.vectors.start.rotation[1] = g_script_light_angles.vy;
    light_values.vectors.start.rotation[2] = g_script_light_angles.vz;
    light_values.vectors.start.scale[0] = g_script_light_scale.vx;
    light_values.vectors.start.scale[1] = g_script_light_scale.vy;
    light_values.vectors.start.scale[2] = g_script_light_scale.vz;
    for (; step <= frames; step++) {
        color.vx = (light_values.vectors.target.rotation[0] - light_values.vectors.start.rotation[0]) * step / frames
            + light_values.vectors.start.rotation[0];
        color.vy = (light_values.vectors.target.rotation[1] - light_values.vectors.start.rotation[1]) * step / frames
            + light_values.vectors.start.rotation[1];
        color.vz = (light_values.vectors.target.rotation[2] - light_values.vectors.start.rotation[2]) * step / frames
            + light_values.vectors.start.rotation[2];
        RotMatrix(&color, &rotation);
        MulMatrix(&rotation, &g_script_light_base_direction_matrix);
        script_copy_matrix(&matrix, &g_script_light_base_color_matrix);
        translation.vx = (light_values.vectors.target.scale[0] - light_values.vectors.start.scale[0]) * step / frames
            + light_values.vectors.start.scale[0];
        translation.vy = (light_values.vectors.target.scale[1] - light_values.vectors.start.scale[1]) * step / frames
            + light_values.vectors.start.scale[1];
        translation.vz = (light_values.vectors.target.scale[2] - light_values.vectors.start.scale[2]) * step / frames
            + light_values.vectors.start.scale[2];
        ScaleMatrixL(&matrix, &translation);
        battle_map_light_matrix_command(MAP_LIGHT_MATRIX_COMMAND_SET, &matrix, &rotation, 0);
        thread_wait_frames(1);
    }
    mem_copy_bytes_nonempty(&g_script_light_angles, &color, sizeof(g_script_light_angles));
    mem_copy_bytes_nonempty(&g_script_light_scale, &translation, sizeof(g_script_light_scale));
    thread_exit_current();
}

/* Store the containing ONE-wide band in [-0x4000, 0x4000); leave the result unchanged outside it. */
void camera_store_yaw_band(s32 value) {
    s32 lower_bound;

    for (lower_bound = -0x4000; lower_bound < 0x3FF8; lower_bound += ONE) {
        if (value < lower_bound + ONE && value >= lower_bound) {
            g_camera_yaw_band = lower_bound;
        }
    }
}

/* Returns the yaw a Focus instruction gives the camera for a pair of units.
 *
 * The flat-pitch camera-block masks of the two units' tiles are combined; each of
 * the four low bits rules out one entry of g_camera_diagonal_yaws. Of the
 * remaining yaws the one with the smallest angular distance from the current yaw
 * (taken modulo a full turn of 0x1000) wins, and the result is the yaw script
 * variable moved by that distance in the shorter direction, so the variable is
 * not wrapped.
 *
 * With all four directions blocked no candidate replaces the initial distance
 * 0x8000, and the result is the yaw variable plus 0x8000 truncated to s16. */
static s16 camera_pick_nearest_unblocked_yaw(s32 runtime_id_a, s32 runtime_id_b) {
    s32 yaw;
    s32 mask;
    s32 raw;
    /* Removing the binding changes four loop-register assignments. */
    register s32 bit __asm__("$7");
    s32 best;
    s32 sign;
    s32 i;
    s32 delta;
    s32 wrapped;
    s32 dist;

    yaw = camera_wrap_yaw_angle() & 0xFFF;
    unit_copy_map_coords_by_runtime_id(runtime_id_a, &g_unit_scratch_coords);
    /* The raw result is assigned before it is masked: combine then knows
     * nothing about the mask's high bits and the loop test keeps its sll 16. */
    mask = battle_map_get_tile_data_value(MAP_TILE_DATA_FLAT_CAMERA_BLOCK_MASK, g_unit_scratch_coords.vx,
        g_unit_scratch_coords.vz, g_unit_scratch_coords.vy);
    mask &= 0xFF;
    unit_copy_map_coords_by_runtime_id(runtime_id_b, &g_unit_scratch_coords);
    mask = (mask
               | battle_map_get_tile_data_value(MAP_TILE_DATA_FLAT_CAMERA_BLOCK_MASK, g_unit_scratch_coords.vx,
                   g_unit_scratch_coords.vz, g_unit_scratch_coords.vy))
        & 0xF;
    bit = 1;
    best = 0x8000;
    sign = 1;
    for (i = 0; i < 4; i++) {
        if ((s16)(bit & mask) == 0) {
            /* Distance to this diagonal directly (delta) and the other way round (wrapped). */
            delta = g_camera_diagonal_yaws[i] - yaw;
            raw = delta;
            if (delta > 0) {
                wrapped = delta - ONE;
            } else {
                wrapped = delta + ONE;
            }
            delta = abs(delta);
            wrapped = abs(wrapped);

            if (wrapped < delta) {
                dist = wrapped;
            } else {
                dist = delta;
            }
            if (dist < best) {
                best = dist;
                if (raw > 0) {
                    sign = -1;
                    if (delta < wrapped) {
                        sign = 1;
                    }
                } else {
                    sign = 1;
                    if (delta < wrapped) {
                        sign = -1;
                    }
                }
            }
        }
        bit <<= 1;
    }
    return g_script_variables[EVENT_SCRIPT_VAR_CAMERA_YAW] + sign * best;
}

/* Rewrite the following Camera operands, skipping an intervening FocusSpeed instruction. */
void script_apply_relative_camera(event_camera_values_t* parameters, const s32* camera_values) {
    u8* source;
    const s32* deltas;
    u8* destination;
    s32 index;
    s32 delta;
    s32 value;

    source = parameters->position_le[0];
    deltas = camera_values;
    destination = ((event_camera_instruction_t*)(parameters + 1))->parameters.values.position_le[0];
    if (((event_camera_instruction_t*)(parameters + 1))->opcode == EVENT_OPCODE_FOCUS_SPEED) {
        destination = ((event_camera_instruction_t*)((event_focus_speed_instruction_t*)(parameters + 1) + 1))
                          ->parameters.values.position_le[0];
    }

    for (index = 0; index < EVENT_CAMERA_COMPONENT_COUNT; index++) {
        delta = *deltas;
        if (index < EVENT_CAMERA_POSITION_COMPONENT_COUNT) {
            delta = delta / 1024;
        }
        if (script_read_s16(source) == CAMERA_FUSION_HOLD_SENTINEL) {
            value = CAMERA_FUSION_HOLD_SENTINEL;
        } else {
            value = delta + script_read_s16(source);
        }
        script_write_s16(destination, value);
        source += 2;
        destination += 2;
        deltas++;
    }
}

/* Patch the next camera instruction to frame two units.
 * A missing unit disables that instruction. */
void script_focus(event_focus_parameters_t* parameters) {
    s32 coords[2][4];
    s32 unit_a;
    s32 unit_b;
    s16 first;
    s16 second;
    u8 keep_angle;
    s32 i;
    u8* cursor;

    cursor = parameters->first_unit_id_le;
    first = script_read_s16(cursor);
    cursor += sizeof(parameters->first_unit_id_le);
    second = script_read_s16(cursor);
    cursor += sizeof(parameters->second_unit_id_le);
    keep_angle = *cursor;
    cursor++;
    while (*cursor != EVENT_OPCODE_CAMERA) {
        cursor += g_script_event_instruction_sizes[*cursor] + 1;
    }
    cursor = ((event_camera_instruction_t*)cursor)->parameters.values.position_le[0];
    unit_a = first;
    unit_b = second;
    first = script_resolve_runtime_id(unit_a);
    second = script_resolve_runtime_id(unit_b);
    if (first == EVENT_RUNTIME_ID_NONE || second == EVENT_RUNTIME_ID_NONE) {
        ((event_camera_instruction_t*)(cursor - offsetof(event_camera_instruction_t, parameters)))->opcode
            = EVENT_OPCODE_CAMERA_DISABLED;
        main_system_handle_malloc_exception(7, 0);
        return;
    }
    unit_copy_world_coords_to_vector(first, (VECTOR*)coords[0]);
    unit_copy_world_coords_to_vector(second, (VECTOR*)coords[1]);
    coords[0][1] += 0x20;
    coords[1][1] += 0x20;
    if (unit_a == CHARACTER_IDENTITY_ALTIMA_SECOND_FORM) {
        coords[0][1] -= 0x50;
        coords[1][1] -= 0x50;
    }
    if (unit_b == CHARACTER_IDENTITY_ALTIMA_FIRST_FORM) {
        coords[0][1] -= 0x20;
        coords[1][1] -= 0x20;
    }
    for (i = 0; i < 3; i++) {
        coords[0][i] = (coords[0][i] + coords[1][i]) * 2;
        script_write_s16(cursor, coords[0][i]);
        cursor += sizeof(((event_camera_values_t*)0)->position_le[0]);
    }
    if (keep_angle == 0) {
        cursor += sizeof(((event_camera_values_t*)0)->position_le[0]);
        script_write_s16(cursor, camera_pick_nearest_unblocked_yaw(first, second));
    }
}

/* FocusSpeed event instruction: derives the camera move duration from the
 * distance between the current and requested position/rotation and the speed.
 *
 * The original leaves rotation_sum uninitialized (s6 is never set). */
void script_focus_speed(u8* parameters, s32* position, s32* rotation) {
    s32 position_delta[4]; /* four slots: the target spaces the arrays 0x10 apart */
    s32 rotation_delta[4];
    s32 speed;
    s32 i;
    s32 value;
    s32 position_sum;
    s32 rotation_sum;

    /* A separate typed Camera cursor reorders the original argument saves. */
    speed = script_read_s16(((event_focus_speed_parameters_t*)parameters)->speed_le);
    parameters += sizeof(event_focus_speed_parameters_t);
    parameters = ((event_camera_instruction_t*)parameters)->parameters.values.position_le[0];
    position_sum = 0;
    for (i = 0; i < 3; i++) {
        value = script_read_s16(parameters + i * 2);
        if (value == CAMERA_FUSION_HOLD_SENTINEL) {
            position_delta[i] = 0;
        } else {
            position_delta[i] = value - position[i] / 1024;
        }
        value = script_read_s16(parameters + i * 2 + sizeof(((event_camera_values_t*)0)->position_le));
        if (value == CAMERA_FUSION_HOLD_SENTINEL) {
            rotation_delta[i] = 0;
        } else {
            rotation_delta[i] = value - rotation[i];
        }
        position_sum += position_delta[i] * position_delta[i];
        rotation_sum += rotation_delta[i] * rotation_delta[i];
    }
    if (position_sum * 2 < rotation_sum) {
        position_sum = rotation_sum / 2;
    }
    position_sum = SquareRoot0(position_sum) / speed;
    if (position_sum == 0) {
        position_sum = 1;
    }
    script_write_s16(((event_camera_parameters_t*)parameters)->duration_le, position_sum);
}

void script_noop_11(void) {
    s32 unused[12];
}

/* Resolve a single selector in place; groups keep their ID and set the filter mode. */
static s32 script_prepare_unit_filter(u16* unit_id, s32* filter_mode) {
    u32 id;

    id = *unit_id;
    if (id != 0 && id < EVENT_UNIT_GROUP_FIRST) {
        *unit_id = script_resolve_runtime_id(id);
        *filter_mode = 0;
        if (*unit_id == EVENT_RUNTIME_ID_NONE) {
            return 0;
        }
    } else {
        *filter_mode = id != 0 ? id - 0xfe : 1;
    }
    return 1;
}

/* Answer one of six unit queries about the runtime id in `in_id`, copying it to
 * `out_id` first.  `mode` selects the query:
 *
 *   0  the id is always accepted
 *   1  the unit exists
 *   2  the unit is on the blue team
 *   3  blue team and free of the check-set 8 statuses
 *   4  the unit is on a non-blue team
 *   5  non-blue team and free of the check-set 8 statuses, ignoring Undead
 *
 * Unsupported modes fall off the end without a defined return value.
 */
static s32 script_filter_unit_id_by_mode(u16* out_id, u16* in_id, s32* mode) {
    unit_stats_t* unit;
    s32 index;
    s32 i;
    s32 blocked;
    s32 query;
    u8 first;

    if (*mode == 0) {
        return 1;
    }
    *out_id = *in_id;
    if (battle_unit_has_runtime_id(*out_id) != 0) {
        index = battle_unit_get_battle_id_by_runtime_id(*out_id);
        if (index != -1) {
            unit = battle_unit_get_stats_from_battle_id(index);
        }
    }
    if (*mode == 1) {
        return battle_unit_has_runtime_id(*out_id);
    }
    if (battle_unit_has_runtime_id(*out_id) == 0) {
        return 0;
    }
    query = *mode;
    if (query == 2) {
        if (index == -1) {
            return 1;
        }
        return (unit->team_flags & BATTLE_TEAM_MASK) == BATTLE_TEAM_BLUE;
    }
    if (query == 3) {
        if (index == -1) {
            return 1;
        }
        for (i = 0; i < STATUS_BYTE_COUNT; i++) {
            if ((unit->status_sets.current[i]
                    & g_main_status_check_sets[MAIN_STATUS_CHECK_SET_SCRIPT_FILTER_EXCLUDED][i])
                != 0) {
                break;
            }
        }
        if ((unit->team_flags & BATTLE_TEAM_MASK) == BATTLE_TEAM_BLUE && i == STATUS_BYTE_COUNT) {
            return 1;
        }
        return 0;
    }
    if (query == 4) {
        if (index == -1) {
            return 0;
        }
        return (unit->team_flags & BATTLE_TEAM_MASK) != BATTLE_TEAM_BLUE;
    }
    if (query == 5) {
        if (index == -1) {
            return 0;
        }
        first = g_main_status_check_sets[MAIN_STATUS_CHECK_SET_SCRIPT_FILTER_EXCLUDED][0];
        for (i = 0; i < STATUS_BYTE_COUNT; i++) {
            if (i == 0) {
                s32 status = unit->status_sets.current[0] & (0xff ^ STATUS_BYTE_MASK(STATUS_ID_UNDEAD));

                blocked = first & status;
            } else {
                blocked = unit->status_sets.current[i]
                    & g_main_status_check_sets[MAIN_STATUS_CHECK_SET_SCRIPT_FILTER_EXCLUDED][i];
            }
            if (blocked != 0) {
                break;
            }
        }
        if ((unit->team_flags & BATTLE_TEAM_MASK) != BATTLE_TEAM_BLUE && i == STATUS_BYTE_COUNT) {
            return 1;
        }
        return 0;
    }
}

/* TeleportOut: play the teleport sound and secondary effect on the unit, raise
 * its hands, modulate its palette by +0x1f per channel, wait 60 frames, modulate
 * it by -0x1f and wait 60 frames more. Then, when `remove` is set, mark its
 * battle unit as not existing. A unit id that does not resolve does nothing. The
 * interpreter always passes remove = 1. */
static void script_teleportout_event_instruction(s32 unit_id, s32 remove) {
    s32 unit;
    s32 idx;

    unit = script_resolve_runtime_id(unit_id);
    if (unit == EVENT_RUNTIME_ID_NONE) {
        return;
    }
    g_sound_pending_sfx_id = MAIN_SFX_TELEPORT_OUT;
    g_thread_call_target = (void (*)(void))battle_effect_set_secondary_teleport_by_runtime_id;
    thread_call_on_main_stack(unit);
    idx = battle_unit_get_battle_id_by_runtime_id(unit);
    unit_set_animation(unit, BATTLE_UNIT_ANIMATION_RAISE_HANDS);
    battle_gfx_start_event_unit_fade_out(unit, 2, 0x1F, 0x1F, 0x1F);
    thread_wait_frames(0x3C);
    battle_gfx_start_event_unit_fade_out(unit, 2, -0x1F, -0x1F, -0x1F);
    thread_wait_frames(0x3C);
    if (remove != 0) {
        battle_unit_set_cannot_exist(idx);
    }
}

/* Start the teleport-in effect for an event unit. The interpreter supplies 1
 * as its second argument; this entry forwards only the unit ID. */
static void script_teleportin_event_instruction(s32 unit_id, s32 unused) {
    s32 runtime_id = script_resolve_runtime_id(unit_id);
    if (runtime_id != EVENT_RUNTIME_ID_NONE) {
        g_thread_call_target = (void (*)(void))battle_effect_set_secondary_teleport_by_runtime_id;
        thread_call_on_main_stack(runtime_id);
    }
}

/* CallFunction 4: tint blue (red and green modulated by -0x1f) every displayed
 * unit whose initial team is not the blue one, skipping units whose ENTD lookup
 * reports -2. */
static void script_blue_remove_all_enemy_units(void) {
    s32 i;
    s32 idx;
    s32 out_index;
    unit_stats_t* stats;
    unit_stats_t* found;

    for (i = 0; i < EVENT_UNIT_SLOT_COUNT; i++) {
        if (battle_unit_has_runtime_id(i) != 0) {
            idx = battle_unit_get_battle_id_by_runtime_id(i);
            if (idx != -1) {
                stats = battle_unit_get_stats_from_battle_id(idx);
                found = battle_unit_find_stats_by_entd_unit_id(stats->unit_id, &out_index);
                if ((found->initial_team_flags & BATTLE_TEAM_MASK) && out_index != -2) {
                    battle_gfx_start_event_unit_fade_out(i, 2, -0x1F, -0x1F, 0);
                }
            }
        }
    }
}

/* BlueRemoveUnit (event instruction 0x99): tints every non-player unit whose
 * ENTD unit id is unit_id blue (red and green modulated by -0x1f).
 *
 * The team test reads the current team byte (0x05), not the initial one at
 * 0x1ba. */
static void script_blue_remove_unit(s32 unit_id) {
    s32 runtime_id;
    s32 battle_index;
    unit_stats_t* unit;
    s32 found_battle_id[2];

    for (runtime_id = 0; runtime_id < EVENT_UNIT_SLOT_COUNT; runtime_id++) {
        if (battle_unit_has_runtime_id(runtime_id) == 0) {
            continue;
        }
        battle_index = battle_unit_get_battle_id_by_runtime_id(runtime_id);
        if (battle_index == -1) {
            continue;
        }
        unit = battle_unit_find_stats_by_entd_unit_id(
            battle_unit_get_stats_from_battle_id(battle_index)->unit_id, found_battle_id);
        if ((unit->team_flags & BATTLE_TEAM_MASK) == BATTLE_TEAM_BLUE) {
            continue;
        }
        if (found_battle_id[0] == -2) {
            continue;
        }
        if (unit->unit_id != unit_id) {
            continue;
        }
        battle_gfx_start_event_unit_fade_out(runtime_id, 2, -0x1F, -0x1F, 0);
    }
}

/*
 * WarpUnit event instruction: move an ENTD unit to a new tile and facing.
 *
 * Parameters: 0x00 ENTD unit id (16-bit value), 0x02 x, 0x03 y, 0x04 higher
 * elevation, 0x05 facing.
 */
void script_warp_unit(const event_warp_unit_parameters_t* instruction) {
    /* Separate instruction and placement pointers change register allocation. */
    union {
        const event_warp_unit_parameters_t* instruction;
        const event_unit_placement_t* placement;
    } cursor;
    unit_stats_t* unit;
    s32 battle_id;
    s32 runtime_id;
    s32 result;
    s16 unit_id;
    map_tile_coordinates_t tile;
    unit_animation_state_t* rotation_state;

    cursor.instruction = instruction;
    unit_id = script_read_s16(cursor.instruction->unit_id_le);
    cursor.placement = &cursor.instruction->placement;
    unit = battle_unit_find_stats_by_entd_unit_id(unit_id, &result);
    battle_id = 0;
    if (result != -2 && result < 0) {
        return;
    }
    for (; battle_id < EVENT_UNIT_SLOT_COUNT; battle_id++) {
        if (battle_unit_get_stats_from_battle_id(battle_id) == unit) {
            break;
        }
    }
    unit->x = cursor.placement->x;
    unit->position.y = cursor.placement->y;
    unit->position.higher_elevation = cursor.placement->elevation;
    unit->position.facing = cursor.placement->facing;
    runtime_id = battle_unit_get_runtime_id_by_battle_id(battle_id);
    if (runtime_id == -1) {
        return;
    }
    tile.x = unit->x;
    tile.layer = unit->position.higher_elevation;
    tile.y = unit->position.y;
    rotation_state = &g_unit_animation_states[runtime_id];
    rotation_state->target_facing = cursor.placement->facing * 4;
    rotation_state->delay = 0;
    rotation_state->rotating = 0;
    battle_unit_place_in_new_location(runtime_id, &tile, cursor.placement->facing);
    unit_set_facing(runtime_id, cursor.placement->facing * 4);
}

/* Set facing and animation immediately; the animation high byte also controls facing-update suppression. */
void script_unit_animation_rotate_event_instruction(const event_unit_anim_rotate_parameters_t* parameters) {
    u8 rotation;
    s16 animation;
    s16 call_runtime_id;
    s16 runtime_id;
    s32 index;
    unit_animation_state_t* rotation_state;

    runtime_id = script_read_s16(parameters->unit_id_le);
    rotation = parameters->rotation;
    animation = script_read_s16(parameters->animation_le);
    call_runtime_id = script_resolve_runtime_id(runtime_id);
    runtime_id = call_runtime_id;
    if (parameters->animation_le[1] == 0) {
        battle_unit_clear_facing_update_suppression(call_runtime_id);
    } else {
        battle_unit_set_facing_update_suppression(call_runtime_id);
    }
    index = runtime_id;
    rotation_state = &g_unit_animation_states[index];
    rotation_state->target_facing = rotation;
    rotation_state->delay = 0;
    rotation_state->rotating = 0;
    unit_set_facing(index, rotation);
    unit_set_animation(index, animation);
}

/* FACE_UNIT and FACE_EACH_OTHER event instructions:
 * turn two units toward each other.
 *
 * Parameters: first 16-bit unit id, second 16-bit unit id, rotate mode, speed index
 * into g_unit_rotation_speed_frames, and a delay multiplier. Every unit record of the second unit turns
 * toward the first; unless second_only is set (FaceUnit), the first unit also turns
 * toward the second. Successive second-unit records are staggered like FaceTile.
 * The angle is built in place (negate, then add) so combine cannot fold the two
 * constant additions into one. */
void script_face_unit(const event_face_unit_parameters_t* parameters, s32 second_only) {
    SVECTOR position;
    SVECTOR target;
    u16 unit_id;
    s32 filter_mode;
    u16 runtime_id;
    s32 i;
    s32 order;
    s32 facing;
    s32 reverse;
    s32 dx;
    s32 dy;
    unit_animation_state_t* state;

    order = 1;
    runtime_id = script_read_s16(parameters->first_unit_id_le);
    unit_id = script_read_s16(parameters->second_unit_id_le);
    if (script_prepare_unit_filter(&unit_id, &filter_mode) != 0) {
        runtime_id = script_resolve_runtime_id(runtime_id);
        if (runtime_id != EVENT_RUNTIME_ID_NONE) {
            for (i = 0; i < EVENT_UNIT_SLOT_COUNT; i++) {
                if (script_filter_unit_id_by_mode(&unit_id, &g_script_unit_face_slot_order[i], &filter_mode) != 0
                    && runtime_id != unit_id) {
                    unit_copy_world_coords(runtime_id, &position);
                    unit_copy_world_coords(unit_id, &target);
                    dx = target.vx - position.vx;
                    dy = target.vz - position.vz;
                    facing = -ratan2(dy, dx);
                    facing += 0xC00;
                    reverse = facing + 0x800;
                    facing &= 0xF00;
                    reverse &= 0xF00;
                    facing >>= 8;
                    reverse >>= 8;
                    if (second_only == 0) {
                        state = &g_unit_animation_states[runtime_id];
                        state->target_facing = facing;
                        state->rotate_mode = parameters->mode;
                        state->frames_per_step = g_unit_rotation_speed_frames[parameters->speed];
                        state->step_counter = 0;
                        state->rotating = 1;
                        state->delay = parameters->delay >> 2;
                    }
                    state = &g_unit_animation_states[unit_id];
                    state->target_facing = reverse;
                    state->rotate_mode = parameters->mode;
                    state->frames_per_step = g_unit_rotation_speed_frames[parameters->speed];
                    state->step_counter = 0;
                    state->rotating = 1;
                    state->delay = parameters->delay * order++ / 4;
                    if (filter_mode == 0) {
                        break;
                    }
                }
            }
        }
    }
}

/* RotateUnit event instruction: turn the addressed unit(s) to a facing.
 *
 * Parameters: 0x00 unit id (16-bit value), 0x02 facing code, 0x03 rotate mode,
 * 0x04 rotation speed (index into g_unit_rotation_speed_frames), 0x05 delay multiplier.
 * Facing codes 0x11-0x13 are relative to the unit's current facing, 0x10 is
 * relative to the camera quadrant, 0x14 restores the saved facing, and any
 * other value is absolute. Matching records are staggered by delay * n / 4.
 *
 * The restore arm carries its own copy of the filter_mode test, which is what
 * retail does: it reloads filter_mode and jumps past the absolute arm's own
 * reload, so cross-jumping merges only the branch and the loop increment. */
void script_rotate_unit(const event_rotate_unit_parameters_t* parameters) {
    u16 unit_id;
    s32 filter_mode;
    s32 i;
    s32 order;
    s32 facing;
    s32 saved_filter_mode;
    unit_animation_state_t* state;

    order = 1;
    unit_id = script_read_s16(parameters->unit_id_le);
    if (script_prepare_unit_filter(&unit_id, &filter_mode) != 0) {
        for (i = 0; i < EVENT_UNIT_SLOT_COUNT; i++) {
            if (script_filter_unit_id_by_mode(&unit_id, &g_script_unit_face_slot_order[i], &filter_mode) != 0) {
                facing = parameters->facing;
                if (facing >= EVENT_ROTATE_UNIT_FACING_RELATIVE_FIRST && facing < EVENT_ROTATE_UNIT_FACING_RESTORE) {
                    facing = facing & 0xF;
                    facing = battle_unit_get_facing_nibble_by_runtime_id(unit_id) + facing * 4;
                    facing = facing & 0xF;
                } else if (facing == EVENT_ROTATE_UNIT_FACING_CAMERA) {
                    facing = g_unit_rotation_speed_frames[4 + ((camera_wrap_yaw_angle() & 0xC00) >> 10)] * 4;
                } else if (facing == EVENT_ROTATE_UNIT_FACING_RESTORE) {
                    state = &g_unit_animation_states[unit_id];
                    if (state->initial_facing != 0xFF) {
                        state->rotate_mode = UNIT_ROTATE_MODE_SHORTEST;
                        state->target_facing = state->initial_facing;
                        state->frames_per_step = g_unit_rotation_speed_frames[parameters->speed];
                        state->step_counter = 0;
                        state->rotating = 1;
                        state->delay = parameters->delay * order++ / 4;
                    }
                    saved_filter_mode = filter_mode;
                    /* Keep the restore arm's stack reload before its loop exit. */
                    __asm__("" : "=r"(saved_filter_mode) : "0"(saved_filter_mode));
                    if (saved_filter_mode == 0) {
                        return;
                    }
                    continue;
                }
                state = &g_unit_animation_states[unit_id];
                state->target_facing = facing;
                state->rotate_mode = parameters->mode;
                state->frames_per_step = g_unit_rotation_speed_frames[parameters->speed];
                state->step_counter = 0;
                state->rotating = 1;
                state->delay = parameters->delay * order++ / 4;
                if (filter_mode == 0) {
                    return;
                }
            }
        }
    }
}

/* Queue the renderer status flags for each of a unit's current statuses on
 * the main stack; the Draw event instruction runs this only for the Zalera
 * event. */
static void status_queue_current_status_graphics(s32 battle_id, s32 enabled) {
    unit_stats_t* stats;
    s32 i;
    s32 mask;

    stats = battle_unit_get_stats_from_battle_id(battle_id);
    g_thread_call_target = (void (*)(void))battle_status_queue_runtime_graphics_flag_change;
    for (i = 0; i < STATUS_COUNT; i++) {
        mask = 0x80 >> (i % 8);
        if ((stats->status_sets.current[i / 8] & mask) != 0) {
            thread_call_on_main_stack(STATUS_HANDLER_INDEX(i), enabled, battle_id);
        }
    }
}

/* Resets every unit slot's status staging state and inflict-status mode to none. */
static void unit_clear_status_staging_data(void) {
    s32 i;
    for (i = 0; i < BATTLE_UNIT_SLOT_COUNT; i++) {
        g_unit_status_staging_data->state[i] = UNIT_STATUS_STAGING_NONE;
        g_unit_status_staging_data->inflict_status_mode[i] = UNIT_INFLICT_STATUS_MODE_NONE;
    }
}

/* Snapshot a unit into the event staging record and queue its status removal.
 *
 * Returns 0 when the slot is already staged. Otherwise the unit's statuses,
 * timers, HP and team are saved, the removal mask is chosen by the pending
 * staged-status variable and the unit type (monsters, Altima and the undead
 * jobs use their own mask), and the removal is applied on the main stack.
 *
 * Each mask branch repeats the whole store tail; cross-jumping merges the
 * copies, and the repeated tail uses give the strength-reduced status walker
 * the target's s1 over the stats pointer. */
s32 unit_save_staged_status_data(s32 battle_id) {
    unit_stats_t* stats;
    s32 i;
    s32 value;
    unit_status_staging_t* st;

    if (g_unit_status_staging_data->state[battle_id] == UNIT_STATUS_STAGING_NONE) {
        stats = battle_unit_get_stats_from_battle_id(battle_id);
        for (i = 0; i < STATUS_BYTE_COUNT; i++) {
            g_unit_status_staging_data->innate[battle_id][i] = stats->status_sets.innate[i];
            g_unit_status_staging_data->inflicted[battle_id][i] = stats->inflicted_status[i];
            g_unit_status_staging_data->current[battle_id][i] = stats->status_sets.current[i];
        }
        for (i = 0; i < BATTLE_TIMED_STATUS_COUNT; i++) {
            g_unit_status_staging_data->status_ct[battle_id][i] = stats->status_ct[i];
        }
        g_unit_status_staging_data->charged_ability_ct[battle_id] = stats->charged_ability_ct;
        g_unit_status_staging_data->death_counter[battle_id] = stats->death_counter;
        g_unit_status_staging_data->hp[battle_id] = stats->hp;
        g_unit_status_staging_data->team_flags[battle_id] = stats->team_flags;
        for (i = 0; i < STATUS_BYTE_COUNT; i++) {
            if (script_get_variable(EVENT_SCRIPT_VAR_BATTLE_NOT_STARTED) != 0) {
                if ((stats->primary_skillset >= SKILLSET_ID_MONSTER_FIRST && stats->primary_skillset < SKILLSET_ID_END)
                    || stats->character_identity == CHARACTER_IDENTITY_ALTIMA_FIRST_FORM
                    || stats->character_identity == CHARACTER_IDENTITY_ALTIMA_SECOND_FORM
                    || stats->job_id == JOB_ID_KNIGHT_UNDEAD || stats->job_id == JOB_ID_ARCHER_UNDEAD
                    || stats->job_id == JOB_ID_WIZARD_UNDEAD || stats->job_id == JOB_ID_TIME_MAGE_UNDEAD
                    || stats->job_id == JOB_ID_ORACLE_UNDEAD || stats->job_id == JOB_ID_SUMMONER_UNDEAD) {
                    value = stats->status_sets.current[i];
                    value &= g_unit_status_staging_masks[UNIT_STATUS_STAGING_MASK_PRE_BATTLE_MONSTER_OR_UNDEAD][i];
                    st = g_unit_status_staging_data;
                    stats->action.status_changes.removal[i] = value;
                    st->added[battle_id][i] = value;
                    st->removed[battle_id][i] = value;
                } else {
                    value = stats->status_sets.current[i];
                    value &= g_unit_status_staging_masks[UNIT_STATUS_STAGING_MASK_PRE_BATTLE][i];
                    st = g_unit_status_staging_data;
                    stats->action.status_changes.removal[i] = value;
                    st->added[battle_id][i] = value;
                    st->removed[battle_id][i] = value;
                }
            } else {
                value = stats->status_sets.current[i];
                value &= g_unit_status_staging_masks[UNIT_STATUS_STAGING_MASK_BATTLE_STARTED][i];
                st = g_unit_status_staging_data;
                stats->action.status_changes.removal[i] = value;
                st->added[battle_id][i] = value;
                st->removed[battle_id][i] = value;
            }
        }
        if (g_unit_status_staging_data->removed[battle_id][STATUS_BYTE_INDEX(STATUS_ID_JUMP)]
            & STATUS_BYTE_MASK(STATUS_ID_JUMP)) {
            g_unit_status_staging_data->jump_float_flags |= UNIT_STATUS_STAGING_JUMP_REMOVED;
        }
        if (g_unit_status_staging_data->removed[battle_id][STATUS_BYTE_INDEX(STATUS_ID_FLOAT)]
            & STATUS_BYTE_MASK(STATUS_ID_FLOAT)) {
            g_unit_status_staging_data->jump_float_flags |= UNIT_STATUS_STAGING_FLOAT_REMOVED;
        }
        if (stats->action.status_changes.removal[STATUS_BYTE_INDEX(STATUS_ID_CHARM)]
            & STATUS_BYTE_MASK(STATUS_ID_CHARM)) {
            stats->team_flags = stats->initial_team_flags;
        }
        /* WORLD's copy targets this BATTLE address too (see QUIRKS.md). */
        g_thread_call_target = (void (*)(void))battle_status_apply_unit_action_removal;
        thread_call_on_main_stack(stats);
        g_unit_status_staging_data->state[battle_id] = UNIT_STATUS_STAGING_SAVED;
        return 1;
    }
    return 0;
}

/*
 * Restore the saved unit state and replay status changes made during an event.
 *
 * The status masks track additions and removals relative to the snapshot.
 * Each changed status is applied through the main-thread dispatcher before
 * the unit graphics are refreshed. State 2 prevents applying the snapshot twice.
 */
static void unit_apply_staged_status_data(s32 battle_id, s32 runtime_id) {
    unit_stats_t* unit;
    s32 i;
    s32 byte_index;
    s32 mask;
    u8 inflict_status_mode;
    if (!(script_get_current_scenario_finish_operation() & 0x8000)
        && g_unit_status_staging_data->state[battle_id] == UNIT_STATUS_STAGING_SAVED) {
        unit = battle_unit_get_stats_from_battle_id(battle_id);
        for (i = 0; i < STATUS_BYTE_COUNT; i++) {
            g_unit_status_staging_data->added[battle_id][i]
                = unit->status_sets.current[i] ^ g_unit_status_staging_data->current[battle_id][i];
            g_unit_status_staging_data->removed[battle_id][i]
                |= g_unit_status_staging_data->added[battle_id][i] & g_unit_status_staging_data->current[battle_id][i];
            g_unit_status_staging_data->added[battle_id][i] &= unit->status_sets.current[i];
        }
        for (i = 0; i < STATUS_BYTE_COUNT; i++) {
            unit->status_sets.innate[i] = g_unit_status_staging_data->innate[battle_id][i];
            unit->inflicted_status[i] = g_unit_status_staging_data->inflicted[battle_id][i];
            unit->status_sets.current[i] = g_unit_status_staging_data->current[battle_id][i];
        }
        for (i = 0; i < BATTLE_TIMED_STATUS_COUNT; i++) {
            unit->status_ct[i] = g_unit_status_staging_data->status_ct[battle_id][i];
        }
        unit->hp = g_unit_status_staging_data->hp[battle_id];
        unit->team_flags = g_unit_status_staging_data->team_flags[battle_id];
        unit->death_counter = g_unit_status_staging_data->death_counter[battle_id];
        /* WORLD's copy targets this BATTLE address too (see QUIRKS.md). */
        g_thread_call_target = (void (*)(void))battle_status_queue_runtime_graphics_flag_change;
        for (i = 0; i < STATUS_COUNT; i++) {
            byte_index = i / 8;
            mask = 0x80 >> (i % 8);
            if (g_unit_status_staging_data->added[battle_id][byte_index] & mask) {
                thread_call_on_main_stack(STATUS_HANDLER_INDEX(i), 0, battle_id);
            }
            if (g_unit_status_staging_data->removed[battle_id][byte_index] & mask) {
                thread_call_on_main_stack(STATUS_HANDLER_INDEX(i), 1, battle_id);
            }
        }
        if (g_unit_status_staging_data->removed[battle_id][STATUS_BYTE_INDEX(STATUS_ID_JUMP)]
            & STATUS_BYTE_MASK(STATUS_ID_JUMP)) {
            g_unit_status_staging_data->jump_float_flags |= UNIT_STATUS_STAGING_JUMP_REMOVED;
        }
        if (g_unit_status_staging_data->removed[battle_id][STATUS_BYTE_INDEX(STATUS_ID_FLOAT)]
            & STATUS_BYTE_MASK(STATUS_ID_FLOAT)) {
            g_unit_status_staging_data->jump_float_flags |= UNIT_STATUS_STAGING_FLOAT_REMOVED;
        }
        inflict_status_mode = g_unit_status_staging_data->inflict_status_mode[battle_id];
        if (inflict_status_mode != UNIT_INFLICT_STATUS_MODE_NONE) {
            if (inflict_status_mode == UNIT_INFLICT_STATUS_MODE_POISON) {
                unit->action.status_changes.infliction[STATUS_BYTE_INDEX(STATUS_ID_POISON)]
                    |= STATUS_BYTE_MASK(STATUS_ID_POISON);
            } else if (inflict_status_mode == UNIT_INFLICT_STATUS_MODE_CRYSTAL) {
                unit->action.status_changes.infliction[STATUS_BYTE_INDEX(STATUS_ID_CRYSTAL)]
                    |= STATUS_BYTE_MASK(STATUS_ID_CRYSTAL);
            } else if (inflict_status_mode == UNIT_INFLICT_STATUS_MODE_REVIVE) {
                unit->hp = 1;
                unit->action.status_changes.infliction[STATUS_BYTE_INDEX(STATUS_ID_CRITICAL)]
                    |= STATUS_BYTE_MASK(STATUS_ID_CRITICAL);
                unit->action.status_changes.removal[STATUS_BYTE_INDEX(STATUS_ID_DEAD)]
                    |= STATUS_BYTE_MASK(STATUS_ID_DEAD);
            }
            g_thread_call_target = (void (*)(void))battle_status_resolve_unit_changes;
            thread_call_on_main_stack(battle_id, 1);
        }
        if (!(unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_CHARGING)]
                & (STATUS_BYTE_MASK(STATUS_ID_CHARGING) | STATUS_BYTE_MASK(STATUS_ID_JUMP)
                    | STATUS_BYTE_MASK(STATUS_ID_PERFORMING)))) {
            unit->charged_ability_ct = 0xff;
        } else {
            unit->charged_ability_ct = g_unit_status_staging_data->charged_ability_ct[battle_id];
        }
        g_unit_status_staging_data->state[battle_id] = UNIT_STATUS_STAGING_RESTORED;
        g_thread_call_target = (void (*)(void))battle_unit_update_graphics_by_runtime_id;
        thread_call_on_main_stack(runtime_id);
    }
}

/* Battle twin of unit_update_staged_status_data. Saves or restores the event status snapshot for every unit
 * record of a unit.
 *
 * With restore clear, each resolved battle unit is snapshotted and its graphics are
 * refreshed on the main stack; otherwise the snapshot is replayed through
 * battle_unit_apply_staged_status_data. Follow-up flags add 40-frame waits. */
static void unit_update_staged_status_data(u16 unit_id, u16 restore) {
    s32 filter_mode;
    s32 unit_index;
    s32 battle_id;
    s32 battle_unit_index;

    g_unit_status_staging_data = &g_ai_workspace_ptr->status_staging;
    if (g_menu_input_mode == MENU_INPUT_PLAYER && script_prepare_unit_filter(&unit_id, &filter_mode) != 0) {
        unit_index = 0;
        g_unit_status_staging_data->jump_float_flags = 0;
        for (; unit_index < BATTLE_UNIT_SLOT_COUNT; unit_index++) {
            if (script_filter_unit_id_by_mode(&unit_id, (u16*)&unit_index, &filter_mode) != 0) {
                battle_unit_index = battle_unit_get_battle_id_by_runtime_id(unit_id);
                if (battle_unit_index != -1) {
                    battle_unit_find_stats_by_entd_unit_id(
                        battle_unit_get_stats_from_battle_id(battle_unit_index)->unit_id, &battle_id);
                    if (battle_id >= 0) {
                        if (restore == 0) {
                            if (unit_save_staged_status_data(battle_unit_index) != 0) {
                                g_thread_call_target = (void (*)(void))battle_unit_update_graphics_by_runtime_id;
                                thread_call_on_main_stack(unit_id);
                            }
                        } else {
                            unit_apply_staged_status_data(battle_unit_index, unit_id);
                        }
                    }
                }
                if (filter_mode == 0) {
                    break;
                }
            }
        }
        if (g_unit_status_staging_data->jump_float_flags != 0) {
            thread_wait_frames(40);
        }
        if (g_unit_status_staging_data->jump_float_flags & UNIT_STATUS_STAGING_JUMP_REMOVED) {
            thread_wait_frames(40);
        }
        if (restore == 0) {
            g_thread_call_target = battle_gfx_reset_jumping_unit_graphic_triggers;
            thread_call_on_main_stack();
        }
    }
}

/* Apply the event status mode and stage its matching exit action. */
void script_inflict_status_thread(void) {
    event_inflict_status_parameters_t* parameters;
    s32 unit_id;
    s32 mode;
    s32 wait_frames;
    s32 battle_unit_index;
    s32 runtime_id;
    unit_stats_t* unit;
    s32 state;

    thread_set_current_task_id(THREAD_TASK_INFLICT_STATUS);
    parameters = (event_inflict_status_parameters_t*)thread_get_current_parameter_1();
    unit_id = script_read_s16(parameters->unit_id_le);
    mode = parameters->mode;
    wait_frames = script_read_s16(parameters->wait_frames_le);
    battle_unit_index = unit_get_battle_id_by_event_unit_id(unit_id);
    runtime_id = script_resolve_runtime_id(unit_id);
    if (runtime_id == EVENT_RUNTIME_ID_NONE) {
        thread_exit_current();
    }
    unit_update_staged_status_data(unit_id, 0);
    unit = battle_unit_get_stats_from_battle_id(battle_unit_index);
    if (unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_DEAD)] & STATUS_BYTE_MASK(STATUS_ID_DEAD)) {
        state = 0;
    } else {
        state = 2;
        if (unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_CRITICAL)] & STATUS_BYTE_MASK(STATUS_ID_CRITICAL)) {
            state = 1;
        }
    }
    if (mode == EVENT_INFLICT_STATUS_POISON) {
        status_inflict_by_event_unit_id(unit_id, STATUS_LSB_INDEX(STATUS_ID_POISON), 1, 1);
        g_unit_status_staging_data->inflict_status_mode[battle_unit_index] = UNIT_INFLICT_STATUS_MODE_POISON;
        g_thread_call_target = (void (*)(void))battle_unit_update_graphics_by_runtime_id;
        thread_call_on_main_stack(runtime_id);
        unit_set_animation(runtime_id, BATTLE_UNIT_ANIMATION_KNEEL);
    } else if (mode == EVENT_INFLICT_STATUS_CRYSTAL) {
        status_inflict_by_event_unit_id(unit_id, STATUS_LSB_INDEX(STATUS_ID_DEAD), 0, 1);
        status_inflict_by_event_unit_id(unit_id, STATUS_LSB_INDEX(STATUS_ID_CRITICAL), 0, 1);
        status_inflict_by_event_unit_id(unit_id, STATUS_LSB_INDEX(STATUS_ID_CRYSTAL), 1, 1);
        g_unit_status_staging_data->inflict_status_mode[battle_unit_index] = UNIT_INFLICT_STATUS_MODE_CRYSTAL;
        g_thread_call_target = (void (*)(void))battle_unit_update_graphics_by_runtime_id;
        thread_call_on_main_stack(runtime_id);
    } else if (mode == EVENT_INFLICT_STATUS_REVIVE) {
        if (state == 0) {
            g_unit_status_staging_data->inflict_status_mode[battle_unit_index] = UNIT_INFLICT_STATUS_MODE_REVIVE;
            status_inflict_by_event_unit_id(unit_id, STATUS_LSB_INDEX(STATUS_ID_DEAD), 0, 1);
            unit->hp = 1;
            status_inflict_by_event_unit_id(unit_id, STATUS_LSB_INDEX(STATUS_ID_CRITICAL), 1, 1);
            g_thread_call_target = (void (*)(void))battle_unit_update_graphics_by_runtime_id;
            thread_call_on_main_stack(runtime_id);
            g_sound_pending_sfx_id = MAIN_SFX_EVENT_REVIVE;
        } else if (state == 2) {
            g_thread_call_target = (void (*)(void))battle_unit_set_mounted_animation_by_runtime_id;
            thread_call_on_main_stack(runtime_id);
        }
    }
    if (wait_frames != 0) {
        thread_wait_frames(wait_frames);
    }
    thread_exit_current();
}

/* Event-script status change: inflicts (which != 0) or removes one status on the
 * unit an event unit id resolves to.
 *
 * The unit's pending infliction and removal bytes are cleared, the one status bit
 * is set and status_resolve_unit_changes runs on the main stack with
 * removal_only. Nothing happens when the id resolves to no unit on the field. */
static void status_inflict_by_event_unit_id(s32 event_unit_id, s32 status, s32 which, s32 removal_only) {
    s32 idx;
    unit_stats_t* unit;
    s32 i;
    s32 byte;
    s32 bit;

    idx = unit_get_battle_id_by_event_unit_id(event_unit_id);
    if (idx == 0x7D0) {
        return;
    }
    unit = battle_unit_get_stats_from_battle_id(idx);
    for (i = 0; i < STATUS_BYTE_COUNT; i++) {
        unit->action.status_changes.infliction[i] = 0;
        unit->action.status_changes.removal[i] = 0;
    }
    /* Action infliction/removal bytes are LSB-first; stored status sets use
     * STATUS_BYTE_MASK's MSB-first representation. */
    byte = status / 8;
    bit = 1 << (status - byte * 8);
    if (which != 0) {
        unit->action.status_changes.infliction[byte] = bit;
    } else {
        unit->action.status_changes.removal[byte] = bit;
    }
    g_thread_call_target = (void (*)(void))battle_status_resolve_unit_changes;
    thread_call_on_main_stack(idx, removal_only);
}

/* ChangeStats event instruction: add a signed delta to one stat of each
 * resolved unit.
 *
 * Parameters: 16-bit unit id, stat selector (event_change_stats_e), then a
 * 16-bit delta. HP clamps to max HP and the other stats to 0-100. */
static void script_change_stats(const event_change_stats_parameters_t* parameters) {
    u16 unit_id;
    s32 filter_mode;
    s32 unit_index;
    s32 battle_id;
    s32 value;
    s16 delta;
    s32 battle_unit_index;
    unit_stats_t* unit;

    delta = script_read_s16(parameters->delta_le);
    unit_id = script_read_s16(parameters->unit_id_le);
    if (script_prepare_unit_filter(&unit_id, &filter_mode) != 0) {
        for (unit_index = 0; unit_index < EVENT_UNIT_SLOT_COUNT; unit_index++) {
            if (script_filter_unit_id_by_mode(&unit_id, (u16*)&unit_index, &filter_mode) != 0) {
                battle_unit_index = battle_unit_get_battle_id_by_runtime_id(unit_id);
                if (battle_unit_index != -1) {
                    unit = battle_unit_find_stats_by_entd_unit_id(
                        battle_unit_get_stats_from_battle_id(battle_unit_index)->unit_id, &battle_id);
                    if (battle_id >= 0) {
                        if (parameters->stat == EVENT_CHANGE_STATS_HP) {
                            value = unit->hp + delta;
                            script_clamp_s32_to_range(&value, 0, unit->max_hp);
                            unit->hp = value;
                        }
                        if (parameters->stat == EVENT_CHANGE_STATS_CT) {
                            value = unit->ct + delta;
                            script_clamp_s32_to_range(&value, 0, UNIT_TURN_CT_THRESHOLD);
                            unit->ct = value;
                        }
                        if (parameters->stat == EVENT_CHANGE_STATS_BRAVE) {
                            value = unit->brave + delta;
                            script_clamp_s32_to_range(&value, 0, UNIT_BRAVE_FAITH_MAX);
                            unit->brave = value;
                        }
                        if (parameters->stat == EVENT_CHANGE_STATS_FAITH) {
                            value = unit->faith + delta;
                            script_clamp_s32_to_range(&value, 0, UNIT_BRAVE_FAITH_MAX);
                            unit->faith = value;
                        }
                    }
                }
                if (filter_mode == 0) {
                    break;
                }
            }
        }
    }
}

/*
 * UnitAnim event instruction.  Parameters: 0x00 unit id
 * (16-bit value), 0x02 animation id (16-bit value), 0x04 move-flag mode (0 unset,
 * 1 flip).  A unit id above 0xff selects a group, so the unit-state loop
 * runs over all 21 unit indices.
 */
void script_unit_anim(const event_unit_anim_parameters_t* parameters) {
    u16 runtime_id;
    s32 state;
    s32 unit_index;
    s32 animation;
    s32 move_flag_mode;

    runtime_id = script_read_s16(parameters->unit_id_le);
    if (script_prepare_unit_filter(&runtime_id, &state) == 0) {
        return;
    }
    for (unit_index = 0; unit_index < EVENT_UNIT_SLOT_COUNT; unit_index++) {
        if (script_filter_unit_id_by_mode(&runtime_id, (u16*)&unit_index, &state) != 0) {
            move_flag_mode = parameters->facing_update_mode;
            if (move_flag_mode == 0) {
                battle_unit_clear_facing_update_suppression(runtime_id);
            } else if (move_flag_mode == 1) {
                battle_unit_set_facing_update_suppression(runtime_id);
            }
            animation = script_read_s16(parameters->animation_le);
            unit_set_animation(runtime_id, animation);
            if (animation >= BATTLE_UNIT_ANIMATION_BOW_HIGH && animation <= BATTLE_UNIT_ANIMATION_BOW_LOW) {
                script_play_effect_and_wait();
            }
            if (state == 0) {
                return;
            }
        }
    }
}

/*
 * Event instruction: put the addressed unit(s) into their status idle
 * animation and wait.  Parameters: 0x00 unit id (16-bit value), 0x02 wait length
 * in frames.  A unit id above 0xff selects a group, so the unit-state loop
 * runs over all 21 unit indices.
 */
static void script_march_units(const event_march_parameters_t* parameters) {
    u16 runtime_id;
    s32 state;
    s32 unit_index;

    runtime_id = script_read_s16(parameters->unit_id_le);
    if (script_prepare_unit_filter(&runtime_id, &state) == 0) {
        return;
    }
    for (unit_index = 0; unit_index < EVENT_UNIT_SLOT_COUNT; unit_index++) {
        if (script_filter_unit_id_by_mode(&runtime_id, (u16*)&unit_index, &state) != 0) {
            battle_unit_set_status_animation_by_runtime_id(runtime_id);
            thread_wait_frames(parameters->wait_frames);
            if (state == 0) {
                return;
            }
        }
    }
}

/*
 * Event instruction: set or clear movement-effect suppression on the addressed unit(s).
 * Parameters: 0x00 unit id (16-bit value), 0x02 nonzero to set, zero to clear.
 * A unit id above 0xff selects a group, so the unit-state loop runs over all
 * 21 unit indices.
 */
static void script_set_units_movement_effect_suppression(const event_movement_effect_parameters_t* parameters) {
    u16 unit_id;
    s32 state;
    s32 unit_index;

    unit_id = script_read_s16(parameters->unit_id_le);
    if (script_prepare_unit_filter(&unit_id, &state) != 0) {
        for (unit_index = 0; unit_index < EVENT_UNIT_SLOT_COUNT; unit_index++) {
            if (script_filter_unit_id_by_mode(&unit_id, (u16*)&unit_index, &state) != 0) {
                if (parameters->suppressed != 0) {
                    battle_unit_set_movement_effect_suppression(unit_id);
                } else {
                    battle_unit_clear_movement_effect_suppression(unit_id);
                }
                if (state == 0) {
                    break;
                }
            }
        }
    }
}

/* ColorUnit: start a palette modulation on a unit or a unit group.
 *
 * Operands: unit id or group (16-bit value), blend mode, red, green, blue (s8 each)
 * and a time. A plain unit id colors that one unit; id 0 and the group ids from
 * EVENT_UNIT_GROUP_FIRST run all 21 unit ids through
 * script_filter_unit_id_by_mode. Nothing happens when a plain unit id does
 * not resolve. */
static void script_color_unit_event_instruction(const event_color_unit_parameters_t* parameters) {
    const event_unit_color_t* color;
    u16 unit_id;
    s32 out;
    s32 i;

    unit_id = script_read_s16(parameters->unit_id_le);
    if (script_prepare_unit_filter(&unit_id, &out) != 0) {
        color = &parameters->color;
        for (i = 0; i < EVENT_UNIT_SLOT_COUNT; i++) {
            if (script_filter_unit_id_by_mode(&unit_id, (u16*)&i, &out) != 0) {
                battle_gfx_apply_unit_palette_modulation_to_units(
                    color->mode, color->duration, unit_id, color->rgb.r, color->rgb.g, color->rgb.b);
                if (out == 0) {
                    return;
                }
            }
        }
    }
}

/* Event instructions 0x6c (enable 0) and 0x6d (enable 1): clear or set
 * palette-update suppression on the event unit, or on every unit its group
 * mode selects.
 *
 * 0x80149698 stores a0 directly as a 16-bit value. The interpreter passes a
 * zero-extended 32-bit value; signed argument normalization is not part of this ABI. */
static void script_set_units_palette_update_suppression(s32 event_unit_id, s32 enable) {
    u16 unit_id;
    s32 mode;
    s32 i;

    unit_id = event_unit_id;
    if (script_prepare_unit_filter(&unit_id, &mode) != 0) {
        for (i = 0; i < EVENT_UNIT_SLOT_COUNT; i++) {
            if (script_filter_unit_id_by_mode(&unit_id, (u16*)&i, &mode) != 0) {
                if (enable == 0) {
                    battle_unit_clear_palette_update_suppression(unit_id);
                } else {
                    battle_unit_set_palette_update_suppression(unit_id);
                }
                if (mode == 0) {
                    return;
                }
            }
        }
    }
}

/* Nonzero when the unit's facing, relative to the camera yaw quadrant, is
 * one of the six "away from camera" directions 7..12 (of 16). */
s32 unit_check_facing_relative_to_camera(s32 event_unit_id) {
    s32 runtime_id;
    s32 camera_facing;

    runtime_id = script_resolve_runtime_id(event_unit_id);
    camera_facing = camera_wrap_yaw_angle();
    camera_facing += 0x200;
    camera_facing &= 0xf00;
    /* Preserve the retail arithmetic shift after the masked camera angle. */
    camera_facing >>= 8;
    camera_facing += battle_unit_get_facing_nibble_by_runtime_id(runtime_id);
    camera_facing &= 0xf;
    camera_facing -= 7;
    return camera_facing < 6U;
}

/*
 * Event instruction 0x69 (FaceTile): turn the addressed unit(s) to face a tile.
 *
 * Parameters: 0x00 unit id (16-bit value), 0x02 tile x, 0x03 tile y, 0x04 unused,
 * 0x05 clockwise flag, 0x06 rotation speed (index into g_unit_rotation_speed_frames), 0x07
 * rotation time. Each matching unit's screen position is compared with the
 * tile center to pick one of 16 facings; the n-th matching unit gets
 * rotation time * n / 4.
 */
static void script_face_tile(const event_face_tile_parameters_t* parameters) {
    SVECTOR screen;
    SVECTOR tile;
    u16 unit_id;
    s32 state;
    s32 i;
    unit_animation_state_t* rotation_state;
    s32 order;
    s32 facing;
    s32 dx;
    s32 dy;

    order = 1;
    tile.vx = parameters->x * MAP_TILE_SIZE + (MAP_TILE_SIZE / 2);
    tile.vz = parameters->y * MAP_TILE_SIZE + (MAP_TILE_SIZE / 2);
    unit_id = script_read_s16(parameters->unit_id_le);
    if (script_prepare_unit_filter(&unit_id, &state) == 0) {
        return;
    }
    for (i = 0; i < EVENT_UNIT_SLOT_COUNT; i++) {
        if (script_filter_unit_id_by_mode(&unit_id, &g_script_unit_face_slot_order[i], &state) != 0) {
            unit_copy_world_coords(unit_id, &screen);
            dx = tile.vx - screen.vx;
            dy = tile.vz - screen.vz;
            facing = ((-ratan2(dy, dx) + 0xC00) & 0xF00) >> 8;
            rotation_state = &g_unit_animation_states[unit_id];
            rotation_state->target_facing = facing;
            rotation_state->rotate_mode = parameters->mode;
            rotation_state->frames_per_step = g_unit_rotation_speed_frames[parameters->speed];
            rotation_state->step_counter = 0;
            rotation_state->rotating = 1;
            rotation_state->delay = (parameters->delay * order++) / 4;
            if (state == 0) {
                return;
            }
        }
    }
}

/* WaitRotateUnit/All: resolved units yield at least once; -1 waits for all 21 slots. */
void script_waitrotateunit_and_waitrotateall_event_instruction(s32 unit_id) {
    s32 i;
    s32 index;

    if (unit_id == -1) {
        do {
            thread_yield();
            for (i = 0; i < EVENT_UNIT_SLOT_COUNT; i++) {
                if (g_unit_animation_states[i].rotating != 0) {
                    break;
                }
            }
        } while (i != EVENT_UNIT_SLOT_COUNT);
    } else {
        index = script_resolve_runtime_id(unit_id);
        if (index != EVENT_RUNTIME_ID_NONE) {
            do {
                thread_yield();
            } while (g_unit_animation_states[index].rotating != 0);
        }
    }
}
