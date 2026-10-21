/* The event script interpreter that runs as the event thread. */

#include "fft/battle_map.h"
#include "fft/battle_map_light.h"
#include "fft/battle_map_mesh.h"
#include "fft/battle_map_palette.h"
#include "fft/battle_state.h"
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
#include "fft/script.h"
#include "fft/script_instruction.h"
#include "fft/script_variable.h"
#include "fft/sound.h"
#include "fft/sprite.h"
#include "fft/sprite_sheet.h"
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

/* WORLD map-command operand pairs; each instruction stores both halfwords together. */
typedef struct script_command_args {
    s16 arg1;
    s16 arg2;
} script_command_args_t;

typedef struct script_music_switch {
    s16 volume;
    s16 fade;
} script_music_switch_t;

/* map-command and music state */
extern script_command_args_t g_script_map_command_80_args;
extern script_command_args_t g_script_map_command_83_args;
extern script_music_switch_t g_script_music_switch;
extern s16 g_screen_sound_current_music_track_index;
extern s16 g_screen_sound_music_switch_request;
extern s16 g_screen_sound_music_unload_slot_request;
extern s32 g_screen_sound_music_volume_transition_request;

/* WORLD scenario interpreter.
 *
 * One cooperative thread runs this loop: fetch the opcode and its first four
 * operand bytes, dispatch through the else-if chain below, then advance by the
 * opcode's encoded length. The chain order is the target's compare order and
 * must not be re-sorted. Handlers that seek (jumps) `continue` without the
 * length step; EventEnd may `goto restart_script` for a chained scenario.
 *
 * BATTLE carries the same interpreter (script_execute_event); the two
 * are kept structurally identical so differences stay visible.
 *
 * The native inner-call trampoline intentionally forwards varying argument
 * counts through its () ABI. */
void script_execute_event(void) {
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
    battle_map_tile_t* tile;
    unit_stats_t* copy_destination;
    u8* effect_bytes;
    const event_effect_instruction_t* effect_instruction;
    thread_t* thread;
    thread_t* threads;

    thread_current_task_set(THREAD_TASK_EXECUTE_EVENT);
    if (tutorial_is_current_event() != 0) {
        gfx_screen_overlay_fade_in();
    }

    /* Under player control, let helper slots 2..13 and the camera settle first. */
    if (g_menu_input_mode == MENU_INPUT_PLAYER) {
        for (work_index = 2; work_index < 0xE; work_index++) {
            thread_inactive_wait(work_index);
        }
        camera_idle_wait();
    } else {
        g_unit_view_hovered_stats_rect.y = 0xAA;
        g_unit_view_attack_caster_stats_rect.y = 0xAA;
        g_unit_view_attack_target_stats_rect.y = 0xAA;
        g_unit_view_right_stats_rect.y = 0xAA;
    }

    /* Before the battle starts, snapshot every unit's statuses; after, apply the staged inflictions. */
    unit_status_staging_clear();
    if (script_variable_get(EVENT_SCRIPT_VAR_BATTLE_NOT_STARTED) != 0) {
        unit_status_staging_update(0, 0);
    } else {
        script_status_commands_process();
    }
    script_variable_set(EVENT_SCRIPT_VAR_DISABLED_MENU_ACTIONS, 0);
    script_variable_set(EVENT_SCRIPT_VAR_DISABLED_CONTROLLER_INPUTS, 0);
    script_variable_set(EVENT_SCRIPT_VAR_FORCED_CONTROLLER_INPUTS, 0);

    /* A chained event re-enters here with the next event block loaded. */
restart_script:
    script_event_speed_set(1U);
    instruction_offset = 0;
    camera_yaw_normalize();
    {
        s32 empty_unit_id = EVENT_UNIT_SLOT_EMPTY;
        /* Keep the shared fill value available before the countdown initializer. */
        for (work_index = 3; work_index >= 0; work_index--) {
            g_script_unit_slots[work_index] = empty_unit_id;
        }
    }

    for (;;) {
        /* Fetch eagerly; handlers reuse the operand locals as scratch afterwards. */
        instruction = (u8*)g_script_event_block;
        instruction += instruction_offset;
        parameters = instruction + 1;
        operand_1 = instruction[1];
        operand_2 = instruction[2];
        operand_3 = instruction[3];
        operand_4 = instruction[4];
        opcode = instruction[0];
        first_operand = script_s16_read(parameters);

        /* ---- Control: NOPs, conditions, and script variable arithmetic ---- */
        if (opcode == EVENT_OPCODE_CAMERA_DISABLED) {
        } else if (opcode == EVENT_OPCODE_NOP) {
        } else if ((opcode - EVENT_OPCODE_COMPARE_LESS_EQUAL) < 6U) {
            script_condition_run(opcode);
        } else if ((opcode - EVENT_OPCODE_ADD_IMMEDIATE) < 0xFU) {
            operand_1 = (u16)first_operand;
            script_variable_command_run(
                opcode, operand_1, script_s16_read(instruction_offset + ((u8*)g_script_event_block) + 3) & 0xFFFF, 0);
        } else if (opcode == EVENT_OPCODE_REQUEST_MAP_JUMP_IN) {
            g_event_map_jump_in_request = operand_1;
            thread_frames_wait(2);
        }
        /* ---- Scenario progression: DismissUnit, CallFunction, BlueRemoveUnit, EventEnd ---- */
        else if (opcode == EVENT_OPCODE_DISMISS_UNIT) {
            if (script_variable_get(EVENT_SCRIPT_VAR_SUPPRESS_PROGRESS_EFFECTS) == 0) {
                script_opcode_7a_dismiss_unit(first_operand);
            }
        } else if (opcode == EVENT_OPCODE_CALL_FUNCTION) {
            /* Sequential selector tests, not switch alternatives: selectors 6
             * and 14 reuse operand_1 as a loop index and later tests see the
             * changed value. That target-visible fallthrough is preserved. */
            thread_frames_wait(4);
            if (script_variable_get(EVENT_SCRIPT_VAR_SUPPRESS_PROGRESS_EFFECTS) == 0) {
                if (operand_1 == EVENT_CALL_FUNCTION_JOIN_UNITS_SILENTLY) {
                    if (script_variable_get(EVENT_SCRIPT_VAR_CURRENT_EVENT) < EVENT_ID_NEW_GAME_INVENTORY_END) {
                        item_new_game_inventory_init();
                    }
                    party_event_units_join();
                }
                if (operand_1 == EVENT_CALL_FUNCTION_OPEN_SAVE_MENU) {
                    /* Stop helper slots 2..14 around the CARD overlay's save screen. */
                    operand_1 = 2;
                    do {
                        thread_parameters_set(operand_1, 0U, 0, 1);
                        g_thread_contexts[operand_1].task_id = THREAD_TASK_STOP_REQUEST;
                        thread_inactive_wait(operand_1);
                        operand_1 += 1;
                    } while (operand_1 < 0xF);

                    if (script_variable_get(EVENT_SCRIPT_VAR_CURRENT_EVENT) != EVENT_ID_EPILOGUE) {
                        script_variable_set(EVENT_SCRIPT_VAR_NEXT_SCENARIO, 1);
                    }
                    script_variable_set(EVENT_SCRIPT_VAR_SAVE_IN_PROGRESS, 1);
                    overlay_companion_request(COMPANION_OVERLAY_CARD);
                    g_overlay_companion_overlay_state = COMPANION_OVERLAY_STATE_CARD_SAVE;
                    do {
                        thread_yield();
                    } while (g_overlay_companion_overlay_state != COMPANION_OVERLAY_STATE_IDLE);
                    script_variable_set(EVENT_SCRIPT_VAR_NEXT_SCENARIO, 0);

                    operand_1 = 2;
                    do {
                        thread_parameters_set(operand_1, 0U, 0, 1);
                        g_thread_contexts[operand_1].task_id = THREAD_TASK_STOP_REQUEST;
                        thread_inactive_wait(operand_1);
                        operand_1 += 1;
                    } while (operand_1 < 0xF);
                }
            }

            /* The remaining selectors run even when progress effects are suppressed. */
            if ((operand_1 == EVENT_CALL_FUNCTION_ADVANCE_YEAR)
                || (operand_1 == EVENT_CALL_FUNCTION_ADVANCE_YEAR_ALTERNATE)) {
                calendar_year_advance();
            }
            if (operand_1 == EVENT_CALL_FUNCTION_INCREASE_BLUE_TEAM_BRAVE) {
                unit_increase_blue_team_brave_by_10();
            }
            if (operand_1 == EVENT_CALL_FUNCTION_REMOVE_ALL_ENEMY_UNITS) {
                script_blue_remove_all_enemy_units();
                thread_frames_wait(0x78);
            }
            if (operand_1 == EVENT_CALL_FUNCTION_REQUEST_CAMERA_YAW_WRAP) {
                g_camera_wrap_yaw_pending = 1;
            }
            if (operand_1 == EVENT_CALL_FUNCTION_DISABLE_DEBUG_DISPLAY) {
                g_debug_display_enabled = 0;
                g_debug_display_unread_flag = 0;
            }
            if (operand_1 == EVENT_CALL_FUNCTION_MUTE_TEXT_AUDIO_CUE) {
                script_variable_set(EVENT_SCRIPT_VAR_MUTE_TEXT_AUDIO_CUE, 1);
            }
            if (operand_1 == EVENT_CALL_FUNCTION_UNMUTE_TEXT_AUDIO_CUE) {
                script_variable_set(EVENT_SCRIPT_VAR_MUTE_TEXT_AUDIO_CUE, 0);
            }
            if (operand_1 == EVENT_CALL_FUNCTION_ADVANCE_MAP_DESTRUCTION_AND_WAIT) {
                script_map_destruction_signal();
                if (g_battle_map_destruction_wait_state != 0) {
                    do {
                        thread_yield();
                    } while (g_battle_map_destruction_wait_state != 0);
                }
            }
            if (operand_1 == EVENT_CALL_FUNCTION_PLAY_ALTIMA_MUSIC) {
                g_screen_sound_current_music_track_index = 0;
                g_sound_music_track_1_id = MUSIC_ALTIMA;
                g_sound_music_track_2_id = 0;
                music_tracks_play(MUSIC_ALTIMA, MUSIC_NONE);
            }
            if (operand_1 == EVENT_CALL_FUNCTION_COPY_UNIT_8_POSITION_TO_UNIT_9_AND_RELOAD_GRAPHICS) {
                unit = unit_stats_get(7);
                copy_destination = unit_stats_get(8);
                copy_destination->x = unit->x;
                copy_destination->position.y = unit->position.y;
                copy_destination->position.higher_elevation = unit->position.higher_elevation;
                sprite_unit_graphics_load_start(8, 1);
                g_unit_graphics_load_pending = 1;
                do {
                    thread_yield();
                } while (g_unit_graphics_load_pending != 0);
                thread_yield();
                thread_yield();
            }
            if (operand_1 == EVENT_CALL_FUNCTION_HIDE_BETHLA_REGION_UNITS) {
                /* The region is two tile rectangles: x 0..3, y 0..10 and x 4..6, y 2..7. */
                region_runtime_id = 0;
                do {
                    if (unit_has_runtime_id(region_runtime_id) != 0) {
                        operand_1 = unit_runtime_battle_id_get(region_runtime_id);
                        if (operand_1 != -1U) {
                            unit = unit_stats_get(operand_1);
                            if ((unit->x < 4U) && (unit->position.y < 0xBU)) {
                                unit_runtime_hide(region_runtime_id);
                            }
                            if (((unit->x - 4) < 3U) && ((unit->position.y - 2) < 6U)) {
                                unit_runtime_hide(region_runtime_id);
                            }
                        }
                    }
                    region_runtime_id += 1;
                } while (region_runtime_id < UNIT_SLOT_COUNT);
            }
            if (operand_1 == EVENT_CALL_FUNCTION_COPY_UNIT_2_POSITION_AND_FACING_TO_UNIT_8_AND_RELOAD_GRAPHICS) {
                unit = unit_stats_get(1);
                copy_destination = unit_stats_get(7);
                copy_destination->x = unit->x;
                copy_destination->position.y = unit->position.y;
                copy_destination->position.higher_elevation = unit->position.higher_elevation;
                copy_destination->position.facing = unit->position.facing;
                sprite_unit_graphics_load_start(7, 1);
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
                draw_runtime_id = script_runtime_id_resolve(CHARACTER_IDENTITY_ZALERA);
                if (draw_runtime_id != EVENT_RUNTIME_ID_NONE) {
                    status_current_graphics_queue(
                        unit_runtime_battle_id_get(draw_runtime_id), STATUS_DISPLAY_CHANGE_REMOVE);
                    g_thread_call_target = (void (*)(void))unit_runtime_graphics_update;
                    thread_call_on_main_stack(draw_runtime_id);
                }
            }
        } else if (opcode == EVENT_OPCODE_BLUE_REMOVE_UNIT) {
            script_opcode_99_blue_remove_unit(first_operand);
        } else if ((opcode == EVENT_OPCODE_END) || (opcode == EVENT_OPCODE_END_2)) {
            /* Let the map, the event-block threads and any file loads finish first. */
            battle_map_refresh_wait();
            do {
                thread_yield();
            } while (thread_task_find(THREAD_TASK_EVENT_BLOCK) != 0);
            do {
                thread_yield();
            } while (file_load_state_get() != 0);
            thread_yield();

            /* A tutorial restores the player's saved options and closes its battle. */
            if (tutorial_is_current_event() != 0) {
                gfx_screen_overlay_fade_out();
                g_game_options = g_game_options_saved;
                script_variable_set(EVENT_SCRIPT_VAR_SUPPRESS_PROGRESS_EFFECTS, 0);
                battle_state_enter_close_battle_to_world(0x20);
            }
            if (g_menu_input_mode == MENU_INPUT_PLAYER) {
                for (work_index = 2; work_index < 0xE; work_index++) {
                    thread_inactive_wait(work_index);
                }
            }

            operand_1 = script_variable_get(EVENT_SCRIPT_VAR_CURRENT_EVENT);
            if (operand_1 != EVENT_ID_OLAN_WOUNDED) {
                if (operand_1 != EVENT_ID_BEOWULF_INVITATION) {
                    sprite_evtchr_init_slots();
                }
            }
            script_variable_set(EVENT_SCRIPT_VAR_BATTLE_NOT_STARTED, 0);

            /* With player input, an event that has no finish operation and is not one of the
             * common events 0x190..0x1a9 tries to chain into the event its conditions select. */
            event_flags = (g_scenario_event_finish_operations[operand_1] & ~0x0CFF) >> 8;
            if (g_menu_input_mode == MENU_INPUT_PLAYER && event_flags == 0
                && (operand_1 - EVENT_ID_COMMON_FIRST) >= 0x1AU && operand_1 != 0) {
                g_scenario_condition_unit_battle_id = 0xFF;
                scenario_conditions_run();
                if (script_next_event_load() != 0) {
                    goto restart_script;
                }
            }

            /* Otherwise the event ends: replay the staged statuses and store each shown unit's facing. */
            work_index = 0;
            unit_facing_quadrants_snap();
            unit_status_staging_update(0, 1);
            for (; work_index < UNIT_SLOT_COUNT; work_index++) {
                if (unit_has_runtime_id(work_index) != 0) {
                    operand_1 = unit_runtime_battle_id_get(work_index);
                    if (operand_1 >= 0) {
                        unit = unit_stats_get(operand_1);
                        unit->position.facing = unit_runtime_facing_quadrant_get(work_index);
                    }
                }
            }

            camera_yaw_normalize();
            camera_tilt_zoom_init();
            thread_current_exit();
        }
        /* ---- Flow control: forward and backward jumps ---- */
        else if (opcode == EVENT_OPCODE_UNKNOWN_DA) {
        } else if (opcode == EVENT_OPCODE_JUMP_FORWARD_IF_ZERO) {
            if (g_script_variables[EVENT_SCRIPT_VAR_COMPARISON_RESULT] == 0) {
                instruction_offset = script_jump_target_find(
                    instruction_offset + 2, EVENT_OPCODE_FORWARD_TARGET, EVENT_OPCODE_FORWARD_TARGET_EXIT, operand_1);
                continue;
            }
        } else if (opcode == EVENT_OPCODE_JUMP_FORWARD) {
            instruction_offset
                = script_jump_target_find(instruction_offset + 2, EVENT_OPCODE_FORWARD_TARGET, -1, operand_1);
            continue;
        } else if (opcode == EVENT_OPCODE_FORWARD_TARGET) {
        } else if (opcode == EVENT_OPCODE_FORWARD_TARGET_EXIT) {
            break;
        } else if (opcode == EVENT_OPCODE_JUMP_BACK) {
            instruction_offset = script_jump_target_find(instruction_offset, EVENT_OPCODE_BACK_TARGET, -1, operand_1);
            continue;
        } else if (opcode == EVENT_OPCODE_BACK_TARGET) {
        } else if (opcode == EVENT_OPCODE_UNKNOWN_D8) {
        } else if (opcode == EVENT_OPCODE_BACK_TARGET_BARRIER) {
        } else if (opcode == EVENT_OPCODE_UNKNOWN_DC) {
            g_script_opcode_dc_value = 0xD;
        }
        /* ---- Dialogue and portraits ---- */
        else if (opcode == EVENT_OPCODE_DISPLAY_MESSAGE) {
            operand_1 = thread_slot_resolve(operand_1);
            thread_start(operand_1, message_dialog_thread);
            message_window_width_measure(
                instruction_offset, operand_2, &g_thread_contexts[operand_1].task_words[DIALOG_THREAD_WORD_TEXT_WIDTH]);

            /* Hand the dialog thread its parameters; operands 3-4 hold a one-based text id. */
            g_thread_contexts[operand_1].function_parameter_1 = operand_2;
            g_thread_contexts[operand_1].function_parameter_2 = ((operand_4 << 8) + operand_3) - 1;
            dialogue_cursor = (u8*)g_script_event_block;
            dialogue_cursor += instruction_offset;
            message_instruction = (event_display_message_instruction_t*)dialogue_cursor;
            operand_4 = message_instruction->unit_id_le[1];
            operand_3 = message_instruction->unit_id_le[0];
            g_thread_contexts[operand_1].function_parameter_3 = (operand_4 << 8) + operand_3;
            g_thread_contexts[operand_1].function_parameter_4 = message_instruction->portrait_selector;
            g_thread_contexts[operand_1].task_words[DIALOG_THREAD_WORD_X_OFFSET]
                = script_s16_read(message_instruction->x_offset_le);
            g_thread_contexts[operand_1].task_words[DIALOG_THREAD_WORD_Y_OFFSET] = script_s16_read(
                ((event_display_message_instruction_t*)(instruction_offset + (u8*)g_script_event_block))->y_offset_le);
            g_thread_contexts[operand_1].task_words[DIALOG_THREAD_WORD_ARROW_OFFSET] = script_s16_read(
                ((event_display_message_instruction_t*)(instruction_offset + (u8*)g_script_event_block))
                    ->arrow_offset_le);
            g_thread_contexts[operand_1].task_words[DIALOG_THREAD_WORD_WINDOW_OPTIONS]
                = ((event_display_message_instruction_t*)((u8*)g_script_event_block + instruction_offset))
                      ->window_options;
            thread_yield();
        } else if (opcode == EVENT_OPCODE_CHANGE_DIALOG) {
            /* Windows are allocated in pairs; resume the first dialog with this selector that awaits text. */
            for (work_index = 0; work_index < 6; work_index += 2) {
                operand_2 = g_menu_window_buffers[work_index].thread_id;
                if ((g_menu_window_buffers[work_index].dialog_selector == operand_1)
                    && (thread_is_running_asm(operand_2) == 1)
                    && (g_thread_contexts[operand_2].task_id == THREAD_TASK_DIALOG_AWAIT_TEXT)) {
                    operand_1 = script_s16_read(
                        ((event_change_dialog_instruction_t*)(instruction_offset + (u8*)g_script_event_block))
                            ->text_id_le);
                    /* 0xFFFF passes through unchanged; other text ids are one-based. */
                    if ((u16)operand_1 != 0xFFFF) {
                        operand_1 -= 1;
                    }
                    g_thread_contexts[operand_2].function_parameter_2 = operand_1;
                    g_thread_contexts[operand_2].function_parameter_4 = script_s16_read(
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
            g_script_map_command_80_args.arg1 = operand_1;
            g_script_map_command_80_args.arg2 = operand_2;
            thread_yield();
        } else if (opcode == EVENT_OPCODE_WAIT_3D_OBJECT) {
            g_3d_object_wait_status = 1;
            do {
                thread_yield();
            } while (g_3d_object_wait_status != 0);
        } else if (opcode == EVENT_OPCODE_USE_FIELD_OBJECT) {
            g_event_field_object_use_request = 1;
            g_script_map_command_83_args.arg1 = operand_1;
            g_script_map_command_83_args.arg2 = operand_2;
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
            operand_1 = script_runtime_id_resolve(first_operand);
            if (operand_1 != EVENT_RUNTIME_ID_NONE) {
                unit_list_head_move(operand_1);
            }
        } else if (opcode == EVENT_OPCODE_UNIT_ANIM) {
            script_opcode_11_unit_anim((const event_unit_anim_parameters_t*)parameters);
        } else if (opcode == EVENT_OPCODE_MARCH) {
            script_opcode_80_march((const event_march_parameters_t*)parameters);
        } else if (opcode == EVENT_OPCODE_SET_UNIT_MOVEMENT_EFFECT_SUPPRESSION) {
            script_opcode_81_set_unit_movement_effect_suppression(
                (const event_movement_effect_parameters_t*)parameters);
        } else if (opcode == EVENT_OPCODE_WAIT_UNIT_READY) {
            operand_1 = script_runtime_id_resolve(first_operand);
            if (operand_1 != EVENT_RUNTIME_ID_NONE) {
                script_unit_ready_wait(operand_1);
            }
        }
        /* ---- Map state, weather, and time of day ---- */
        else if (opcode == EVENT_OPCODE_CHANGE_MAP_BETA) {
            g_event_pending_map_jump_out_id = first_operand;
            thread_frames_wait(2);
            battle_map_script_light_reset();
        } else if (opcode == EVENT_OPCODE_CHANGE_MAP) {
            g_event_pending_map_jump_out_2_id = first_operand;
            thread_frames_wait(2);
            battle_map_script_light_reset();
        } else if (opcode == EVENT_OPCODE_SET_DAYTIME) {
            g_event_pending_map_state = operand_1;
            thread_frames_wait(2);
            battle_map_refresh_wait();
        } else if (opcode == EVENT_OPCODE_WEATHER) {
            g_event_weather_request = first_operand;
            thread_frames_wait(2);
            battle_map_refresh_wait();
        } else if (opcode == EVENT_OPCODE_RELOAD_MAP_STATE) {
            g_event_map_reload_state = 1;
            do {
                thread_yield();
            } while (g_event_map_reload_state != 0);
        } else if (opcode == EVENT_OPCODE_APPEND_MAP_STATE) {
            g_event_map_append_state = 1;
            do {
                thread_yield();
            } while (g_event_map_append_state != 0);
        } else if (opcode == EVENT_OPCODE_PAUSE) {
            script_opcode_16_pause();
        } else if (opcode == EVENT_OPCODE_RESUME_WAITING_DIALOG) {
            for (work_index = 1; work_index < 17; ++work_index) {
                thread = &g_thread_contexts[work_index];
                if (thread->is_running && thread->task_id == THREAD_TASK_WAIT_FOR_RESUME
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
            g_event_effect_target_runtime_id = script_runtime_id_resolve(
                ((const event_effect_instruction_t*)((u8*)g_script_event_block + instruction_offset))->target_unit_id);
            /* Keep the base load and byte advance separate for the original register assignment. */
            effect_bytes = (u8*)g_script_event_block;
            effect_bytes += instruction_offset;
            effect_instruction = (const event_effect_instruction_t*)effect_bytes;
            g_event_effect_target_position.x = effect_instruction->x;
            g_event_effect_target_position.y = effect_instruction->y;
            g_event_effect_target_mode = effect_instruction->target_mode;
            script_variable_set(EVENT_SCRIPT_VAR_CAMERA_ROLL, 0);
            thread_frames_wait(2);
        } else if (opcode == EVENT_OPCODE_EFFECT_START) {
            do {
                thread_yield();
                g_thread_call_target = (void (*)(void))effect_request_load_poll;
            } while (thread_call_on_main_stack() != 0);
        } else if (opcode == EVENT_OPCODE_EFFECT_END) {
            thread_start(thread_slot_resolve(THREAD_SLOT_FIRST_IDLE), &script_opcode_8b_effect_end_thread);
        }
        /* ---- Camera ---- */
        else if (opcode == EVENT_OPCODE_FOCUS) {
            camera_yaw_band_store(g_script_variables[EVENT_SCRIPT_VAR_CAMERA_YAW]);
            script_focus((event_focus_parameters_t*)parameters);
        } else if (opcode == EVENT_OPCODE_FOCUS_SPEED) {
            script_variables = g_script_variables;
            script_focus_speed(parameters, script_variables + EVENT_SCRIPT_VAR_CAMERA_X,
                script_variables + EVENT_SCRIPT_VAR_CAMERA_TILT);
        } else if (opcode == EVENT_OPCODE_RELATIVE_CAMERA) {
            script_relative_camera_apply(
                (event_camera_values_t*)parameters, g_script_variables + EVENT_SCRIPT_VAR_CAMERA_X);
        } else if (opcode == EVENT_OPCODE_CAMERA) {
            operand_1 = thread_slot_resolve(THREAD_SLOT_FIRST_IDLE);
            thread_start(operand_1, camera_thread);
            g_thread_contexts[operand_1].function_parameter_1 = (s32)parameters;
        } else if (opcode == EVENT_OPCODE_CAMERA_FUSION_START) {
            operand_1 = thread_slot_resolve(THREAD_SLOT_FIRST_IDLE);
            thread_start(operand_1, camera_fusion_thread);
            g_thread_contexts[operand_1].function_parameter_1 = (s32)parameters;
            instruction_offset = script_instruction_offset_find(instruction_offset, EVENT_OPCODE_CAMERA_FUSION_END);
        } else if (opcode == EVENT_OPCODE_CAMERA_FUSION_END) {
        } else if (opcode == EVENT_OPCODE_CAMERA_SPEED_CURVE) {
            g_camera_speed_curve = operand_1;
        } else if (opcode == EVENT_OPCODE_UNKNOWN_74) {
            script_opcode_74_unknown(parameters);
        }
        /* ---- Unit movement and facing ---- */
        else if (opcode == EVENT_OPCODE_WALK_TO) {
            /* WORLD ignores WalkTo; BATTLE starts the unit's movement here. */
        } else if (opcode == EVENT_OPCODE_JUMP) {
            script_opcode_70_jump((const event_jump_parameters_t*)parameters);
        } else if (opcode == EVENT_OPCODE_WAIT_WALK) {
            script_opcode_29_wait_walk(first_operand);
        } else if (opcode == EVENT_OPCODE_WALK_TO_ANIM) {
            operand_1 = script_runtime_id_resolve(first_operand);
            if (operand_1 != EVENT_RUNTIME_ID_NONE) {
                g_unit_pending_animation_ids[operand_1]
                    = script_s16_read(instruction_offset + ((u8*)g_script_event_block) + 3);
                g_unit_was_moving_latch[operand_1] = 0;
            }
        } else if (opcode == EVENT_OPCODE_WAIT_ALL_UNIT_MOVEMENT) {
            do {
                thread_yield();
            } while (unit_is_moving(-1) != 0);
        } else if (opcode == EVENT_OPCODE_WAIT_FILE_LOAD) {
            do {
                thread_yield();
            } while (file_load_state_get() != 0);
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
            operand_1 = thread_slot_resolve(THREAD_SLOT_FIRST_IDLE);
            thread_start(operand_1, script_opcode_2a_block_start_thread);
            g_thread_contexts[operand_1].function_parameter_1 = (s32)parameters;
            instruction_offset = script_instruction_offset_find(instruction_offset, EVENT_OPCODE_BLOCK_END);
        } else if (opcode == EVENT_OPCODE_BLOCK_END) {
        } else if (opcode == EVENT_OPCODE_UNKNOWN_23) {
        } else if (opcode == EVENT_OPCODE_UNKNOWN_24) {
        } else if (opcode == EVENT_OPCODE_EARTHQUAKE_END) {
            thread_frames_wait(1);
            thread_suspend(thread_task_find(THREAD_TASK_EARTHQUAKE));
        } else if (opcode == EVENT_OPCODE_SET_SPEAKER_PAIR) {
            g_message_speaker_swap_unit_a = first_operand;
            g_message_speaker_swap_unit_b = script_s16_read(instruction_offset + ((u8*)g_script_event_block) + 3);
        } else if (opcode == EVENT_OPCODE_MAP_DARKNESS) {
            operand_1 = thread_slot_resolve(THREAD_SLOT_FIRST_IDLE);
            thread_start(operand_1, &script_opcode_1a_map_darkness);
            g_thread_contexts[operand_1].function_parameter_1 = (s32)parameters;
        } else if (opcode == EVENT_OPCODE_COLOR_SCREEN) {
            operand_1 = thread_slot_resolve(THREAD_SLOT_FIRST_IDLE);
            thread_start(operand_1, script_opcode_3e_color_screen_thread);
            g_thread_contexts[operand_1].function_parameter_1 = (s32)parameters;
        } else if (opcode == EVENT_OPCODE_EARTHQUAKE_START) {
            operand_1 = thread_slot_resolve(THREAD_SLOT_FIRST_IDLE);
            thread_start(operand_1, &script_opcode_41_earthquake_start);
            g_thread_contexts[operand_1].function_parameter_1 = (s32)parameters;
        } else if (opcode == EVENT_OPCODE_SPRITE_MOVE || opcode == EVENT_OPCODE_SPRITE_MOVE_BETA) {
            operand_1 = thread_next_slot_resolve(THREAD_SLOT_FIRST_IDLE);
            if (opcode == EVENT_OPCODE_SPRITE_MOVE) {
                thread_start(operand_1, &script_sprite_move);
            } else {
                thread_start(operand_1, &script_sprite_move_beta);
            }
            g_thread_contexts[operand_1].function_parameter_1 = (s32)parameters;
            g_thread_contexts[operand_1].task_id = THREAD_TASK_SPRITE_MOVE;
            g_thread_contexts[operand_1].task_words[SPRITE_MOVE_THREAD_WORD_RUNTIME_ID]
                = script_runtime_id_resolve(first_operand);
        } else if (opcode == EVENT_OPCODE_WAIT_SPRITE_MOVE) {
            script_opcode_6f_wait_sprite_move(first_operand);
        } else if (opcode == EVENT_OPCODE_MAP_LIGHT) {
            operand_1 = thread_slot_resolve(THREAD_SLOT_FIRST_IDLE);
            thread_start(operand_1, battle_map_light_thread);
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
                parameters[0], parameters[4], (s8)parameters[1], (s8)parameters[2], (s8)parameters[3]);
        } else if (opcode == EVENT_OPCODE_COLOR_UNIT) {
            script_opcode_32_color_unit((const event_color_unit_parameters_t*)parameters);
        } else if (opcode == EVENT_OPCODE_RESUME_UNIT_PALETTE_UPDATES) {
            script_unit_palette_updates_suppress((u16)first_operand, 0);
        } else if (opcode == EVENT_OPCODE_SUPPRESS_UNIT_PALETTE_UPDATES) {
            script_unit_palette_updates_suppress((u16)first_operand, 1);
        } else if (opcode == EVENT_OPCODE_COLOR_FIELD) {
            battle_map_color_field(
                parameters[0], parameters[4], (s8)parameters[1], (s8)parameters[2], (s8)parameters[3]);
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
            script_event_speed_set(operand_1);
        } else if (opcode == EVENT_OPCODE_DARK_SCREEN) {
            g_unit_panel_hide_numeric_values = 0;
            operand_1 = thread_slot_resolve(THREAD_SLOT_FIRST_IDLE);
            thread_start(operand_1, &script_dark_screen_thread);
            thread_parameters_set(operand_1, 0, (s32)parameters, 0);
        } else if (opcode == EVENT_OPCODE_REMOVE_DARK_SCREEN) {
            operand_1 = thread_task_find(THREAD_TASK_DARK_SCREEN_HOLD);
            if (operand_1 != 0) {
                g_thread_contexts[operand_1].task_id = THREAD_TASK_DARK_SCREEN;
            }
        } else if (opcode == EVENT_OPCODE_DISPLAY_CONDITIONS) {
            operand_3 = thread_slot_resolve(THREAD_SLOT_FIRST_IDLE);
            if ((operand_3 == 4) && (operand_1 == EVENT_DISPLAY_CONDITION_APPLY_POST_BATTLE_UNIT_CHANGES)) {
                thread_inactive_wait(2);
                operand_3 = 2;
            }
            /* Keep the cached second operand as an allocator input at this join;
             * otherwise GCC exchanges the target s5/s6 operand lifetimes. */
            __asm__("" : : "r"(operand_2));
            thread_start(operand_3, &script_conditions_display);
            thread_parameters_set(operand_3, operand_1, operand_2, 0);
        } else if (opcode == EVENT_OPCODE_SHOW_GRAPHIC) {
            operand_3 = thread_slot_resolve(THREAD_SLOT_FIRST_IDLE);
            thread_start(operand_3, &script_graphic_show);
            thread_parameters_set(operand_3, operand_1, 0, 0);
        }
        /* ---- Sound and music ---- */
        else if (opcode == EVENT_OPCODE_SOUND_EFFECT) {
            g_sound_pending_sfx_id = first_operand;
        } else if (opcode == EVENT_OPCODE_BG_SOUND) {
            operand_1 = thread_slot_resolve(THREAD_SLOT_FIRST_IDLE);
            thread_start(operand_1, sound_background_thread_run);
            thread_parameters_set(operand_1, (s32)parameters, 0, 0);
        } else if (opcode == EVENT_OPCODE_EDIT_BG_SOUND) {
            operand_1 = thread_slot_resolve(THREAD_SLOT_FIRST_IDLE);
            thread_start(operand_1, sound_background_fade_thread_run);
            thread_parameters_set(operand_1, (s32)parameters, 0, 0);
        } else if (opcode == EVENT_OPCODE_SWITCH_TRACK) {
            g_screen_sound_music_switch_request = operand_1;
            g_script_music_switch.volume = operand_2;
            g_script_music_switch.fade = operand_3;
            thread_yield();
        } else if (opcode == EVENT_OPCODE_FADE_SOUND) {
            /* The target MIPS variable shift uses the low five count bits.
             * Explicit C masking adds an instruction; counts >=32 retain the
             * original C shift limitation rather than an invented input restriction. */
            g_screen_sound_music_volume_transition_request = operand_2 << (operand_1 + 0x10);
            thread_yield();
        } else if (opcode == EVENT_OPCODE_END_TRACK) {
            g_screen_sound_music_unload_slot_request = operand_1;
            thread_yield();
        }
        /* ---- EVTCHR and reserved VRAM slots ---- */
        else if (opcode == EVENT_OPCODE_LOAD_EVTCHR) {
            operand_2 = thread_slot_resolve(THREAD_SLOT_FIRST_IDLE);
            thread_start(operand_2, gfx_load_evtchr_thread);
            thread_parameters_set(
                operand_2, operand_1, script_s16_read(instruction_offset + ((u8*)g_script_event_block) + 2), 0);
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
            g_event_pending_unit_vram_copy = (script_runtime_id_resolve(first_operand) << 8) | operand_3;
            thread_yield();
        } else if (opcode == EVENT_OPCODE_RELEASE_RESERVED_VRAM_SLOT) {
            g_event_pending_reserved_vram_release_slot = operand_1;
            thread_yield();
        } else if (opcode == EVENT_OPCODE_UNKNOWN_E0) {
        }
        /* ---- Adding and removing units ---- */
        else if (opcode == EVENT_OPCODE_REMOVE_UNIT) {
            s16 normalized_unit_id = script_runtime_id_resolve(first_operand);
            if (normalized_unit_id != EVENT_RUNTIME_ID_NONE) {
                removed_runtime_id = normalized_unit_id;
                while (unit_is_moving(removed_runtime_id) != 0) {
                    thread_yield();
                }
                /* Retail repeats the check made above. */
                if (removed_runtime_id != EVENT_RUNTIME_ID_NONE) {
                    operand_1 = unit_runtime_battle_id_get(removed_runtime_id);
                    unit_runtime_remove(removed_runtime_id);
                    if (operand_1 >= 0) {
                        unit_set_off_field(operand_1);
                    }
                }
            }
        } else if (opcode == EVENT_OPCODE_DRAW) {
            draw_runtime_id = script_runtime_id_resolve(first_operand);
            if (draw_runtime_id != EVENT_RUNTIME_ID_NONE) {
                unit_runtime_show(draw_runtime_id);
                if (script_variable_get(EVENT_SCRIPT_VAR_CURRENT_EVENT) == EVENT_ID_LIMBERRY_CEMETERY_BATTLE) {
                    status_current_graphics_queue(
                        unit_runtime_battle_id_get(draw_runtime_id), STATUS_DISPLAY_CHANGE_ADD);
                    g_thread_call_target = (void (*)(void))unit_runtime_graphics_update;
                    thread_call_on_main_stack(draw_runtime_id);
                }
            }
        } else if (opcode == EVENT_OPCODE_ERASE) {
            erased_runtime_id = script_runtime_id_resolve(first_operand);
            if (erased_runtime_id != EVENT_RUNTIME_ID_NONE) {
                unit_runtime_hide(erased_runtime_id);
            }
        } else if (opcode == EVENT_OPCODE_ADD_GHOST_UNIT) {
            ghost_parameters = (u8*)g_script_event_block + instruction_offset + 3;
            /* An empty countdown that retail keeps; it has no effect. */
            ghost_runtime_id = 3;
            do {
                --ghost_runtime_id;
            } while (ghost_runtime_id >= 0);

            /* Take the first free runtime slot; with none free, `continue` reruns this instruction unyielding. */
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
                continue;
            }
        } else if (opcode == EVENT_OPCODE_UNKNOWN_75) {
        } else if (opcode == EVENT_OPCODE_ADD_UNIT) {
            script_opcode_45_add_unit();
            /* Only a present unit or one off the field is added; crystals, treasure and disabled units are not. */
            appearance_unit = unit_entd_find_stats(first_operand, &added_unit_state);
            if ((added_unit_state == -2) || (added_unit_state >= 0)) {
                for (added_battle_id = 0; added_battle_id < UNIT_SLOT_COUNT; added_battle_id++) {
                    if (unit_stats_get(added_battle_id) == appearance_unit) {
                        break;
                    }
                }

                /* Not shown yet: load its graphics. Already shown: draw it unless operand 3 is set,
                 * and refresh its graphics when staging removed statuses. */
                operand_1 = unit_battle_runtime_id_get(added_battle_id);
                if (operand_1 == -1) {
                    unit_status_staging_save(added_battle_id);
                    sprite_unit_graphics_load_start(added_battle_id, operand_3);
                    g_unit_graphics_load_pending = 1U;
                } else {
                    if (operand_3 == 0) {
                        unit_runtime_show(operand_1);
                    }
                    if (unit_status_staging_save(added_battle_id) != 0) {
                        g_thread_call_target = (void (*)(void))unit_runtime_graphics_update;
                        thread_call_on_main_stack(operand_1);
                    }
                }
            }
        } else if (opcode == EVENT_OPCODE_WARP_UNIT) {
            script_opcode_5f_warp_unit((const event_warp_unit_parameters_t*)parameters);
        } else if (opcode == EVENT_OPCODE_UNKNOWN_61) {
        } else if (opcode == EVENT_OPCODE_SET_BATTLE_UNIT_POSITION) {
            unit = unit_stats_get(operand_1);
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
            } while (thread_task_find(THREAD_TASK_ADD_UNIT) != 0);
        } else if (opcode == EVENT_OPCODE_ADD_UNIT_START) {
            operand_1 = thread_slot_resolve(THREAD_SLOT_FIRST_IDLE);
            thread_start(operand_1, script_opcode_49_add_unit_start_thread);
            g_thread_contexts[operand_1].function_parameter_1 = (s32)parameters;
            instruction_offset = script_instruction_offset_find(instruction_offset, EVENT_OPCODE_ADD_UNIT_END);
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
            if (tutorial_is_current_event() != 0 && first_operand >= 3) {
                thread_frames_wait(first_operand / system_vsync_interval_get());
            } else {
                thread_frames_wait(first_operand);
            }
        } else if (opcode == EVENT_OPCODE_TUTORIAL_BUTTON_WAIT) {
            do {
                thread_yield();
            } while (!(first_operand & g_script_unfiltered_controller_input));
        } else if (opcode == EVENT_OPCODE_END_SOUND) {
            sound_all_sfx_stop();
        }
        /* ---- Miscellaneous unit and display commands ---- */
        else if (opcode == EVENT_OPCODE_UNIT_SHADOW) {
            shadow_runtime_id = script_runtime_id_resolve(first_operand);
            if (shadow_runtime_id != EVENT_RUNTIME_ID_NONE) {
                if (operand_3 != 0) {
                    sprite_runtime_shadow_hide(shadow_runtime_id);
                } else {
                    sprite_runtime_shadow_show(shadow_runtime_id);
                }
            }
        } else if (opcode == EVENT_OPCODE_DEBUG_PAUSE) {
            script_debug_message_print();
        } else if (opcode == EVENT_OPCODE_WAIT_VALUE) {
            script_opcode_7e_wait_value((const event_wait_value_parameters_t*)parameters);
        } else if (opcode == EVENT_OPCODE_EVTCHR_PALETTE) {
            runtime_id_for_7f = script_runtime_id_resolve(first_operand);
            if (runtime_id_for_7f != EVENT_RUNTIME_ID_NONE) {
                sprite_evtchr_set_palette_by_runtime_id(runtime_id_for_7f, operand_3, operand_4);
            }
        } else if (opcode == EVENT_OPCODE_LOAD_ATTACK_GRAPHICS) {
            script_opcode_82_load_attack_graphics();
        } else if (opcode == EVENT_OPCODE_CHANGE_STATS) {
            script_opcode_83_change_stats((const event_change_stats_parameters_t*)parameters);
        } else if (opcode == EVENT_OPCODE_PLAY_TUNE) {
            music_tune_play(operand_1);
        } else if (opcode == EVENT_OPCODE_UNLOCK_DATE) {
            if ((script_variable_get(EVENT_SCRIPT_VAR_SUPPRESS_PROGRESS_EFFECTS) == 0) && (operand_1 != 0)) {
                date_month = script_variable_get(EVENT_SCRIPT_VAR_MONTH);
                script_opcode_85_unlock_date(g_brave_story_treasure_acquisition_date_bits, operand_1 - 1, date_month,
                    script_variable_get(EVENT_SCRIPT_VAR_DAY));
            }
        } else if (opcode == EVENT_OPCODE_TEMP_WEAPON) {
            palette_runtime_id = script_runtime_id_resolve(first_operand);
            if (palette_runtime_id != EVENT_RUNTIME_ID_NONE) {
                sprite_runtime_thrown_item_palette_set(operand_3, palette_runtime_id);
            }
        } else if (opcode == EVENT_OPCODE_ARROW) {
            /* WORLD only resolves the two units; BATTLE sets an arrow trajectory between them. */
            first_runtime_id = script_runtime_id_resolve(first_operand);
            if (first_runtime_id != EVENT_RUNTIME_ID_NONE) {
                unit_runtime_battle_id_get(first_runtime_id);
                second_runtime_id
                    = script_runtime_id_resolve(script_s16_read(instruction_offset + ((u8*)g_script_event_block) + 3));
                if (second_runtime_id != EVENT_RUNTIME_ID_NONE) {
                    unit_runtime_battle_id_get(second_runtime_id);
                }
            }
        } else if (opcode == EVENT_OPCODE_MAP_FREEZE) {
            battle_map_freeze();
        } else if (opcode == EVENT_OPCODE_MAP_UNFREEZE) {
            battle_map_unfreeze();
        } else if (opcode == EVENT_OPCODE_UNIT_ANIM_ROTATE) {
            script_opcode_8c_unit_anim_rotate((const event_unit_anim_rotate_parameters_t*)parameters);
        } else if (opcode == EVENT_OPCODE_WAIT_GRAPHIC_PRINT) {
            /* Wait until the ShowGraphic thread sets its fourth parameter or exits. */
            thread_yield();
            do {
                thread_yield();
                operand_1 = thread_task_find(THREAD_TASK_SHOW_GRAPHIC);
            } while (operand_1 != 0 && g_thread_contexts[operand_1].function_parameter_4 == 0);
        } else if (opcode == EVENT_OPCODE_LOCK_MENU_INPUT) {
            g_menu_input_lock_mode = operand_1;
        } else if (opcode == EVENT_OPCODE_TUTORIAL_COORDINATE_CONFIRM) {
            /* Wait for Circle, without a d-pad direction, while the cursor is on the given tile. */
            while (g_cursor_x != operand_1 || g_cursor_y != operand_2
                || ((initial_buttons = g_script_unfiltered_controller_input) & PSX_PAD_CIRCLE) == 0
                || (initial_buttons & PSX_PAD_DPAD_MASK) != 0) {
                thread_yield();
            }
        }
        /* ---- Map title, dates, teleports, and status ---- */
        else if (opcode == EVENT_OPCODE_SHOW_MAP_TITLE) {
            operand_4 = thread_slot_resolve(THREAD_SLOT_FIRST_IDLE);
            overlay_companion_request(COMPANION_OVERLAY_ATTACK);
            battle_map_title_load_graphic();
            thread_start(operand_4, attack_map_title_entrypoint);
            thread_parameters_set(operand_4, (s8)operand_1, (s8)operand_2, (s8)operand_3);
            thread_inactive_wait(operand_4);
        } else if (opcode == EVENT_OPCODE_SET_DATE_ADVANCE) {
            if (script_variable_get(EVENT_SCRIPT_VAR_SUPPRESS_PROGRESS_EFFECTS) == 0) {
                script_variable_set(EVENT_SCRIPT_VAR_DATE_ADVANCE, first_operand);
            }
        } else if (opcode == EVENT_OPCODE_TELEPORT_OUT) {
            script_opcode_94_teleport_out(first_operand, 1);
        } else if (opcode == EVENT_OPCODE_TELEPORT_IN) {
            script_opcode_98_teleport_in(first_operand, 1);
        } else if (opcode == EVENT_OPCODE_INFLICT_STATUS) {
            operand_1 = thread_slot_resolve(THREAD_SLOT_FIRST_IDLE);
            thread_start(operand_1, script_inflict_status_thread);
            thread_parameters_set(operand_1, (s32)parameters, 0, 0);
        } else if (opcode == EVENT_OPCODE_UNKNOWN_95) {
        } else {
            break;
        }

        /* Step over the opcode byte and its operands. */
        instruction_offset += 1 + g_script_event_instruction_sizes[opcode];
    }

    /* An unknown opcode or a ForwardTargetExit reached in sequence ends the thread. */
    thread_current_exit();
}
