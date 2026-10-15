/* Walk/jump instructions, effect playback and block threads. */

#include "fft/battle.h"
#include "fft/battle_effect.h"
#include "fft/battle_gfx.h"
#include "fft/battle_move.h"
#include "fft/battle_script.h"
#include "fft/battle_unit.h"
#include "fft/event_attack.h"
#include "fft/main.h"
#include "fft/menu.h"
#include "fft/script.h"
#include "fft/sound.h"
#include "fft/thread.h"
#include "fft/unit.h"
#include "psx/libgpu.h"

/* DebugPause: print "PAUSE" to the debug font stream, yield one frame and set
 * g_script_debug_paused, which no code reads. The script continues. */
void script_print_debug_message(void) {
    FntPrint(g_script_pause_message);
    thread_yield();
    g_script_debug_paused = 1;
}

/* Start movement to the encoded tile with the signed speed and terrain-cost flag. */
void script_walkto_event_instruction(const event_walk_to_parameters_t* parameters) {
    s32 id;
    map_tile_coordinates_t coordinates;

    id = script_read_s16(parameters->unit_id_le);
    id = script_resolve_runtime_id(id);
    if (id != EVENT_RUNTIME_ID_NONE) {
        coordinates.x = parameters->x;
        coordinates.y = parameters->y;
        coordinates.layer = parameters->elevation;
        move_start_unit_walk_to(id, &coordinates,
            MOVE_PATHFIND_IGNORE_TERRAIN_COST - parameters->terrain_cost_flag * MOVE_PATHFIND_IGNORE_TERRAIN_COST, 3,
            script_read_s16(parameters->speed_le));
    }
}

/* Mirror the unit sprite for mode 1 and restore it for every other mode. */
void script_mirrorsprite_event_instruction(const event_mirror_sprite_parameters_t* parameters) {
    s32 mode;
    s32 runtime_id;

    runtime_id = script_read_s16(parameters->unit_id_le);
    mode = parameters->mode;
    runtime_id = script_resolve_runtime_id(runtime_id);
    if (runtime_id != EVENT_RUNTIME_ID_NONE) {
        if (mode == 1) {
            battle_unit_set_horizontal_flip_flag(runtime_id);
        } else {
            battle_unit_clear_horizontal_flip_flag(runtime_id);
        }
    }
}

/* ResetPalette: resolve the unit id operand and reapply the weather, time-of-day
 * palette modulation without tile effects, unless the unit is missing. */
void script_resetpalette_event_instruction(const event_unit_parameters_t* parameters) {
    s32 runtime_id = script_resolve_runtime_id(script_read_s16(parameters->unit_id_le));
    if (runtime_id != EVENT_RUNTIME_ID_NONE) {
        battle_gfx_apply_weather_time_tile_palette_mod_by_runtime_id(runtime_id);
    }
}

/* Jump: cancel the unit's accumulated event offset, then build its one-step path
 * of length_plus_one - 1 tiles in the encoded direction through
 * battle_unit_prepare_rider_dismount. The unit's Jump stat is 6 while that runs
 * and is restored afterwards. A unit id that does not resolve does nothing. */
void script_jump_event_instruction(const event_jump_parameters_t* parameters) {
    SVECTOR vec;
    s32 runtime_id;
    SVECTOR* coords;
    unit_stats_t* stats;
    u8 saved;

    runtime_id = script_resolve_runtime_id(script_read_s16(parameters->unit_id_le));
    if (runtime_id != EVENT_RUNTIME_ID_NONE) {
        coords = battle_unit_get_event_offset_by_runtime_id(runtime_id);
        setVector(&vec, -coords->vx, -coords->vy, -coords->vz);
        battle_unit_add_event_offset_by_runtime_id(runtime_id, &vec);
        stats = battle_unit_get_stats_from_battle_id(battle_unit_get_battle_id_by_runtime_id(runtime_id));
        saved = stats->jump;
        stats->jump = 6;
        battle_unit_prepare_rider_dismount(runtime_id, parameters->direction, parameters->length_plus_one - 1);
        stats->jump = saved;
    }
}

/* Yield at least once, then wait until the resolved unit stops moving. */
void script_waitwalk_event_instruction(s32 unit_id) {
    s32 runtime_id;

    runtime_id = script_resolve_runtime_id(unit_id);
    if (runtime_id != EVENT_RUNTIME_ID_NONE) {
        do {
            thread_yield();
        } while (move_is_unit_moving_by_runtime_id(runtime_id) != 0);
    }
}

/* Enter ATTACK.OUT when script variable 0x1fc is clear: mark the transition,
 * load companion executable 0xd, then hand off to its scenario-music,
 * formation-sprite, and portrait entry points. */
void script_load_attack_graphics_event_instruction(void) {
    if (script_get_variable(EVENT_SCRIPT_VAR_SUPPRESS_PROGRESS_EFFECTS) == 0) {
        g_script_attack_entry_mode = SCRIPT_ATTACK_ENTRY_PRELOADED;
        menu_request_open_companion_executable(COMPANION_OVERLAY_ATTACK);
        thread_yield();
        attack_load_scenario_conditionals();
        attack_gfx_build_formation_sprites();
        attack_gfx_load_portraits();
    }
}

/* Plays the current effect through the inner-subroutine caller, then yields
 * to other threads until the animation continue check reports done. */
void script_play_effect_and_wait(void) {
    g_thread_call_target = effect_call_play;
    thread_call_on_main_stack();

    do {
        thread_yield();
        g_thread_call_target = (void (*)(void))battle_state_get_animation_continue_check;
    } while (thread_call_on_main_stack());
}

/* Thread body of EffectEnd: play the effect that Effect and EffectStart prepared
 * and exit when it has finished. */
void script_play_effect_thread(void) {
    thread_set_current_task_id(THREAD_TASK_PLAY_EFFECT);
    script_play_effect_and_wait();
    thread_exit_current();
}

/* Run the unit instructions inside an event block, then exit its thread. */
void script_block_start_thread(void) {
    /* Preserve the target's unused 40-byte stack area. */
    s32 unused[10];
    u8* script;
    u8* parameters;
    s16 first_halfword;
    s16 runtime_id;
    s32 thread_id;

    g_thread_contexts[g_thread_current_id].task_id = THREAD_TASK_EVENT_BLOCK;
    script = (u8*)thread_get_current_parameter_1_variant();
    parameters = script + 1;
    while (script[0] != EVENT_OPCODE_BLOCK_END) {
        first_halfword = script_read_s16(parameters);
        if (script[0] == EVENT_OPCODE_CALL_FUNCTION) {
            if (script[1] == EVENT_CALL_FUNCTION_MUTE_TEXT_AUDIO_CUE) {
                script_set_variable(EVENT_SCRIPT_VAR_MUTE_TEXT_AUDIO_CUE, 1);
            } else if (script[1] == EVENT_CALL_FUNCTION_UNMUTE_TEXT_AUDIO_CUE) {
                script_set_variable(EVENT_SCRIPT_VAR_MUTE_TEXT_AUDIO_CUE, 0);
            }
        } else if (script[0] == EVENT_OPCODE_SOUND_EFFECT) {
            g_sound_pending_sfx_id = first_halfword;
        } else if (script[0] == EVENT_OPCODE_WARP_UNIT) {
            script_warp_unit((const event_warp_unit_parameters_t*)parameters);
        } else if (script[0] == EVENT_OPCODE_WAIT_VALUE) {
            script_wait_value_event_instruction((const event_wait_value_parameters_t*)parameters);
        } else if (script[0] == EVENT_OPCODE_DEBUG_PAUSE) {
            script_print_debug_message();
        } else if (script[0] == EVENT_OPCODE_WALK_TO) {
            g_thread_call_target = (void (*)(void))script_walkto_event_instruction;
            thread_call_on_main_stack(parameters);
        } else if (script[0] == EVENT_OPCODE_WALK_TO_ANIM) {
            runtime_id = script_resolve_runtime_id(first_halfword);
            if (runtime_id != EVENT_RUNTIME_ID_NONE) {
                g_unit_pending_animation_ids[runtime_id] = script_read_s16(script + 3);
            }
            g_unit_was_moving_latch[runtime_id] = 0;
        } else if (script[0] == EVENT_OPCODE_JUMP) {
            script_jump_event_instruction((const event_jump_parameters_t*)parameters);
        } else if (script[0] == EVENT_OPCODE_WAIT_WALK) {
            script_waitwalk_event_instruction(first_halfword);
        } else if (script[0] == EVENT_OPCODE_UNIT_ANIM) {
            script_unit_anim((const event_unit_anim_parameters_t*)parameters);
        } else if (script[0] == EVENT_OPCODE_MIRROR_SPRITE) {
            script_mirrorsprite_event_instruction((const event_mirror_sprite_parameters_t*)(script + 1));
        } else if (script[0] == EVENT_OPCODE_RESET_PALETTE) {
            script_resetpalette_event_instruction((const event_unit_parameters_t*)(script + 1));
        } else if (script[0] == EVENT_OPCODE_SET_FIRST_UNIT) {
            runtime_id = script_resolve_runtime_id(first_halfword);
            if (runtime_id != EVENT_RUNTIME_ID_NONE) {
                battle_unit_move_to_head(runtime_id);
            }
        } else if (script[0] == EVENT_OPCODE_ERASE) {
            runtime_id = script_resolve_runtime_id(first_halfword);
            if (runtime_id != EVENT_RUNTIME_ID_NONE) {
                battle_gfx_reset_unit_graphic_trigger(runtime_id);
            }
        } else if (script[0] == EVENT_OPCODE_DRAW) {
            runtime_id = script_resolve_runtime_id(first_halfword);
            if (runtime_id != EVENT_RUNTIME_ID_NONE) {
                battle_unit_draw_and_activate_by_runtime_id(runtime_id);
            }
        } else if (script[0] == EVENT_OPCODE_WAIT_UNIT_READY) {
            runtime_id = script_resolve_runtime_id(first_halfword);
            if (runtime_id != EVENT_RUNTIME_ID_NONE) {
                script_wait_for_unit_ready(runtime_id);
            }
        } else if (script[0] == EVENT_OPCODE_WAIT) {
            thread_wait_frames(script_read_s16(script + 1));
        } else if (script[0] == EVENT_OPCODE_ROTATE_UNIT) {
            script_rotate_unit((const event_rotate_unit_parameters_t*)(script + 1));
        } else if (script[0] == EVENT_OPCODE_FACE_UNIT) {
            script_face_unit((const event_face_unit_parameters_t*)(script + 1), 1);
        } else if (script[0] == EVENT_OPCODE_SPRITE_MOVE || script[0] == EVENT_OPCODE_SPRITE_MOVE_BETA) {
            thread_id = thread_resolve_id_after_current(THREAD_SLOT_FIRST_IDLE);
            if (script[0] == EVENT_OPCODE_SPRITE_MOVE) {
                thread_start(thread_id, script_sprite_move);
            } else {
                thread_start(thread_id, script_sprite_move_beta);
            }
            g_thread_contexts[thread_id].function_parameter_1 = (s32)(script + 1);
            g_thread_contexts[thread_id].task_id = THREAD_TASK_SPRITE_MOVE;
            g_thread_contexts[thread_id].task_words[0] = script_resolve_runtime_id(first_halfword);
        } else if (script[0] == EVENT_OPCODE_WAIT_SPRITE_MOVE) {
            script_waitspritemove_event_instruction(first_halfword);
        } else if (script[0] == EVENT_OPCODE_WAIT_ROTATE_UNIT) {
            script_waitrotateunit_and_waitrotateall_event_instruction(script[1]);
        } else if (script[0] == EVENT_OPCODE_WAIT_ROTATE_ALL) {
            script_waitrotateunit_and_waitrotateall_event_instruction(-1);
        } else if (script[0] == EVENT_OPCODE_UNIT_ANIM_ROTATE) {
            script_unit_animation_rotate_event_instruction((const event_unit_anim_rotate_parameters_t*)(script + 1));
        } else if (script[0] == EVENT_OPCODE_COLOR_UNIT) {
            runtime_id = script_resolve_runtime_id(first_halfword);
            if (runtime_id != EVENT_RUNTIME_ID_NONE) {
                battle_gfx_apply_unit_palette_modulation_to_units(
                    script[3], script[7], runtime_id, (s8)script[4], (s8)script[5], (s8)script[6]);
            }
        }
        script += 1 + g_script_event_instruction_sizes[script[0]];
        parameters = script + 1;
    }
    thread_exit_current();
}

/*
 * Event thread started by the AddUnitStart instruction: walks the following
 * script instructions through AddUnitEnd, loading each AddUnit's graphics and
 * processing LoadAttackGraphics before waiting for pending loads. WORLD binds
 * the thread parameter accessor to its own copy.
 */
void script_add_unit_start_thread(void) {
    s32 index;
    s32 slot;
    const u8* script;
    unit_stats_t* unit;
    s32 runtime_id;
    s16 unit_id;
    u8 opcode;
    u8 flag;

    g_thread_contexts[g_thread_current_id].task_id = THREAD_TASK_ADD_UNIT;
    slot = 0;
    script = (const u8*)thread_get_current_parameter_1_variant();
    while (1) {
        unit_id = script_read_s16(script + 1);
        opcode = script[0];
        flag = script[3];
        if (opcode == EVENT_OPCODE_ADD_UNIT_END) {
            break;
        }
        if (opcode == EVENT_OPCODE_LOAD_ATTACK_GRAPHICS) {
            script_load_attack_graphics_event_instruction();
        } else if (opcode == EVENT_OPCODE_ADD_UNIT) {
            unit = battle_unit_find_stats_by_entd_unit_id(unit_id, &index);
            if (index == -2 || index >= 0) {
                for (slot = 0; slot < EVENT_UNIT_SLOT_COUNT; slot++) {
                    if (battle_unit_get_stats_from_battle_id(slot) == unit) {
                        break;
                    }
                }
                runtime_id = battle_unit_get_runtime_id_by_battle_id(slot);
                if (runtime_id == -1) {
                    unit_save_staged_status_data(slot);
                    battle_gfx_load_unit_graphics_by_battle_id(slot, flag);
                    g_unit_graphics_load_pending = 1;
                } else {
                    if (flag == 0) {
                        battle_unit_draw_and_activate_by_runtime_id(runtime_id);
                    }
                    if (unit_save_staged_status_data(slot) != 0) {
                        g_thread_call_target = (void (*)(void))battle_unit_update_graphics_by_runtime_id;
                        thread_call_on_main_stack(runtime_id);
                    }
                }
            }
            while (1) {
                thread_yield();
                if (g_unit_graphics_load_pending == 0) {
                    break;
                }
                if (g_main_debug_display_enabled != 0) {
                    FntPrint(g_text_reading_character_message, slot);
                }
            }
        }
        slot++;
        script += g_script_event_instruction_sizes[script[0]] + 1;
    }
    thread_exit_current();
}
