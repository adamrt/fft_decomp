/* Run native event instructions for walking, sprite changes, effect playback and scripted unit creation. */

#include "fft/battle/battle_state.h"
#include "fft/battle/deploy.h"
#include "fft/battle/move.h"
#include "fft/effect/effect_script.h"
#include "fft/game/battle_unit.h"
#include "fft/gfx/portrait.h"
#include "fft/gfx/sprite.h"
#include "fft/gfx/sprite_sheet.h"
#include "fft/map/map.h"
#include "fft/map/map_unit.h"
#include "fft/script/script.h"
#include "fft/script/script_instruction.h"
#include "fft/script/script_variable.h"
#include "fft/sys/overlay.h"
#include "fft/sys/sound.h"
#include "fft/sys/system.h"
#include "fft/sys/thread.h"
#include "psx/libgpu.h"
#include "psx/libgte.h"
#include "psx/types.h"

static s16 s_script_debug_paused;

/* SCRIPT_OPCODE_DEBUG_PAUSE (0x72)
 * DebugPause event instruction (0x72): print the pause message with the debug font, yield once and
 * set g_script_debug_paused, which nothing reads. */
void script_opcode_debug_pause(void) {
    FntPrint("\n\nPAUSE\n\n");
    thread_yield();
    s_script_debug_paused = 1;
}

/* SCRIPT_OPCODE_WALK_TO (0x28)
 * WalkTo: start the unit walking to the tile at the signed speed. A terrain_cost_flag of 1 makes
 * the path respect terrain costs; 0 ignores them. */
void script_opcode_walk_to(const event_walk_to_parameters_t* parameters) {
    s32 id = script_read_s16(parameters->unit_id_le);
    map_tile_coordinates_t coordinates;

    id = script_resolve_map_unit_id(id);
    if (id != EVENT_MAP_UNIT_ID_NONE) {
        coordinates.x = parameters->x;
        coordinates.y = parameters->y;
        coordinates.layer = parameters->elevation;
        move_start_walk_to(id, &coordinates,
            MOVE_PATHFIND_IGNORE_TERRAIN_COST - parameters->terrain_cost_flag * MOVE_PATHFIND_IGNORE_TERRAIN_COST, 3,
            script_read_s16(parameters->speed_le));
    }
}

/* SCRIPT_OPCODE_MIRROR_SPRITE (0x68)
 * Mirror the unit sprite for mode 1 and restore it for every other mode. */
void script_opcode_mirror_sprite(const event_mirror_sprite_parameters_t* parameters) {
    s32 mode;
    s32 map_unit_id = script_read_s16(parameters->unit_id_le);

    mode = parameters->mode;
    map_unit_id = script_resolve_map_unit_id(map_unit_id);
    if (map_unit_id != EVENT_MAP_UNIT_ID_NONE) {
        if (mode == 1) {
            sprite_map_unit_set_horizontal_flip(map_unit_id);
        } else {
            sprite_map_unit_clear_horizontal_flip(map_unit_id);
        }
    }
}

/* SCRIPT_OPCODE_RESET_PALETTE (0x97)
 * ResetPalette: resolve the unit id operand and reapply the weather, time-of-day
 * palette modulation without tile effects, unless the unit is missing. */
void script_opcode_reset_palette(const event_unit_parameters_t* parameters) {
    s32 map_unit_id = script_resolve_map_unit_id(script_read_s16(parameters->unit_id_le));

    if (map_unit_id != EVENT_MAP_UNIT_ID_NONE) {
        sprite_map_unit_reset_palette(map_unit_id);
    }
}

/* SCRIPT_OPCODE_JUMP (0x70)
 * Jump: cancel the unit's accumulated event offset, then build its one-step path
 * of length_plus_one - 1 tiles in the encoded direction through
 * move_start_jump. The unit's Jump stat is 6 while that runs
 * and is restored afterwards. A unit id that does not resolve does nothing. */
void script_opcode_jump(const event_jump_parameters_t* parameters) {
    SVECTOR vec;
    s32 map_unit_id = script_resolve_map_unit_id(script_read_s16(parameters->unit_id_le));
    const SVECTOR* coords;
    battle_unit_t* battle_unit;
    u8 saved;

    if (map_unit_id != EVENT_MAP_UNIT_ID_NONE) {
        coords = map_unit_get_event_offset(map_unit_id);
        setVector(&vec, -coords->vx, -coords->vy, -coords->vz);
        map_unit_add_event_offset(map_unit_id, &vec);
        battle_unit = battle_unit_get(map_unit_get_battle_unit_id(map_unit_id));
        saved = battle_unit->jump;
        battle_unit->jump = 6;
        move_start_jump(map_unit_id, parameters->direction, parameters->length_plus_one - 1);
        battle_unit->jump = saved;
    }
}

/* SCRIPT_OPCODE_WAIT_WALK (0x29)
 * Yield at least once, then wait until the resolved unit stops moving. */
void script_opcode_wait_walk(s32 unit_id) {
    s32 map_unit_id = script_resolve_map_unit_id(unit_id);

    if (map_unit_id != EVENT_MAP_UNIT_ID_NONE) {
        do {
            thread_yield();
        } while (move_map_unit_is_moving(map_unit_id) != 0);
    }
}

/* SCRIPT_OPCODE_LOAD_ATTACK_GRAPHICS (0x82)
 * Preload ATTACK.OUT when SCRIPT_VAR_SUPPRESS_PROGRESS_EFFECTS is clear:
 * load companion executable 0xd in preload mode, look up the pending
 * scenario's special portrait and unit, and build the formation sprites and
 * portraits, so the later ATTACK entry skips reloading the overlay, sprites
 * and portraits. */
void script_opcode_load_attack_graphics(void) {
    if (script_variable_get(SCRIPT_VAR_SUPPRESS_PROGRESS_EFFECTS) == 0) {
        g_script_attack_entry_mode = SCRIPT_ATTACK_ENTRY_PRELOADED;
        overlay_companion_load(COMPANION_OVERLAY_ATTACK);
        thread_yield();
        scenario_load_setup();
        deploy_load_sprites();
        portrait_party_load();
    }
}

/* Plays the current effect through the inner-subroutine caller, then yields
 * to other threads until the animation continue check reports done. */
void script_play_effect_and_wait(void) {
    g_thread_call_target = effect_play_request;
    thread_call_on_main_stack();

    do {
        thread_yield();
        g_thread_call_target = (void (*)(void))battle_animation_get_wait_state;
    } while (thread_call_on_main_stack());
}

/* SCRIPT_OPCODE_EFFECT_END (0x8b)
 * Thread body of EffectEnd: play the effect that Effect and EffectStart prepared
 * and exit when it has finished. */
void script_opcode_effect_end_thread(void) {
    thread_current_set_task(THREAD_TASK_PLAY_EFFECT);
    script_play_effect_and_wait();
    thread_current_exit();
}

/* SCRIPT_OPCODE_BLOCK_START (0x2a)
 * Thread body of BlockStart: run the instructions up to BlockEnd in this thread, then exit.
 * Only a subset of the unit, wait, sound and effect instructions run here; any other
 * instruction in the block is skipped. */
void script_opcode_block_start_thread(void) {
    /* Preserve the target's unused 40-byte stack area. */
    s32 unused[10];
    u8* script;
    u8* parameters;
    s16 first_halfword;
    s16 map_unit_id;
    s32 thread_id;

    g_thread_contexts[g_thread_current_id].task_id = THREAD_TASK_EVENT_BLOCK;
    script = (u8*)thread_current_get_parameter_1();
    parameters = script + 1;
    while (script[0] != SCRIPT_OPCODE_BLOCK_END) {
        first_halfword = script_read_s16(parameters);
        if (script[0] == SCRIPT_OPCODE_CALL_FUNCTION) {
            if (script[1] == EVENT_CALL_FUNCTION_MUTE_TEXT_AUDIO_CUE) {
                script_variable_set(SCRIPT_VAR_MUTE_TEXT_AUDIO_CUE, 1);
            } else if (script[1] == EVENT_CALL_FUNCTION_UNMUTE_TEXT_AUDIO_CUE) {
                script_variable_set(SCRIPT_VAR_MUTE_TEXT_AUDIO_CUE, 0);
            }
        } else if (script[0] == SCRIPT_OPCODE_SOUND_EFFECT) {
            g_sound_pending_sfx_id = first_halfword;
        } else if (script[0] == SCRIPT_OPCODE_WARP_UNIT) {
            script_opcode_warp_unit((const event_warp_unit_parameters_t*)parameters);
        } else if (script[0] == SCRIPT_OPCODE_WAIT_VALUE) {
            script_opcode_wait_value((const event_wait_value_parameters_t*)parameters);
        } else if (script[0] == SCRIPT_OPCODE_DEBUG_PAUSE) {
            script_opcode_debug_pause();
        } else if (script[0] == SCRIPT_OPCODE_WALK_TO) {
            g_thread_call_target = (void (*)(void))script_opcode_walk_to;
            thread_call_on_main_stack(parameters);
        } else if (script[0] == SCRIPT_OPCODE_WALK_TO_ANIM) {
            map_unit_id = script_resolve_map_unit_id(first_halfword);
            if (map_unit_id != EVENT_MAP_UNIT_ID_NONE) {
                g_unit_pending_animation_ids[map_unit_id] = script_read_s16(script + 3);
            }
            /* Retail bug: this store runs even for EVENT_MAP_UNIT_ID_NONE (2000), far past the
             * 21-entry array. */
            g_unit_was_moving_latch[map_unit_id] = 0;
        } else if (script[0] == SCRIPT_OPCODE_JUMP) {
            script_opcode_jump((const event_jump_parameters_t*)parameters);
        } else if (script[0] == SCRIPT_OPCODE_WAIT_WALK) {
            script_opcode_wait_walk(first_halfword);
        } else if (script[0] == SCRIPT_OPCODE_UNIT_ANIM) {
            script_opcode_unit_anim((const event_unit_anim_parameters_t*)parameters);
        } else if (script[0] == SCRIPT_OPCODE_MIRROR_SPRITE) {
            script_opcode_mirror_sprite((const event_mirror_sprite_parameters_t*)(script + 1));
        } else if (script[0] == SCRIPT_OPCODE_RESET_PALETTE) {
            script_opcode_reset_palette((const event_unit_parameters_t*)(script + 1));
        } else if (script[0] == SCRIPT_OPCODE_SET_FIRST_UNIT) {
            map_unit_id = script_resolve_map_unit_id(first_halfword);
            if (map_unit_id != EVENT_MAP_UNIT_ID_NONE) {
                map_unit_move_to_list_head(map_unit_id);
            }
        } else if (script[0] == SCRIPT_OPCODE_ERASE) {
            map_unit_id = script_resolve_map_unit_id(first_halfword);
            if (map_unit_id != EVENT_MAP_UNIT_ID_NONE) {
                map_unit_hide(map_unit_id);
            }
        } else if (script[0] == SCRIPT_OPCODE_DRAW) {
            map_unit_id = script_resolve_map_unit_id(first_halfword);
            if (map_unit_id != EVENT_MAP_UNIT_ID_NONE) {
                map_unit_show(map_unit_id);
            }
        } else if (script[0] == SCRIPT_OPCODE_WAIT_UNIT_READY) {
            map_unit_id = script_resolve_map_unit_id(first_halfword);
            if (map_unit_id != EVENT_MAP_UNIT_ID_NONE) {
                script_wait_unit_ready(map_unit_id);
            }
        } else if (script[0] == SCRIPT_OPCODE_WAIT) {
            thread_wait_frames(script_read_s16(script + 1));
        } else if (script[0] == SCRIPT_OPCODE_ROTATE_UNIT) {
            script_opcode_rotate_unit((const event_rotate_unit_parameters_t*)(script + 1));
        } else if (script[0] == SCRIPT_OPCODE_FACE_UNIT) {
            script_opcode_face_units((const event_face_unit_parameters_t*)(script + 1), 1);
        } else if (script[0] == SCRIPT_OPCODE_SPRITE_MOVE || script[0] == SCRIPT_OPCODE_SPRITE_MOVE_BETA) {
            thread_id = thread_resolve_next_slot(THREAD_SLOT_FIRST_IDLE);
            if (script[0] == SCRIPT_OPCODE_SPRITE_MOVE) {
                thread_start(thread_id, script_opcode_sprite_move_thread);
            } else {
                thread_start(thread_id, script_opcode_sprite_move_beta_thread);
            }
            g_thread_contexts[thread_id].function_parameter_1 = (s32)(script + 1);
            g_thread_contexts[thread_id].task_id = THREAD_TASK_SPRITE_MOVE;
            g_thread_contexts[thread_id].task_words[SPRITE_MOVE_THREAD_WORD_MAP_UNIT_ID]
                = script_resolve_map_unit_id(first_halfword);
        } else if (script[0] == SCRIPT_OPCODE_WAIT_SPRITE_MOVE) {
            script_opcode_wait_sprite_move(first_halfword);
        } else if (script[0] == SCRIPT_OPCODE_WAIT_ROTATE_UNIT) {
            script_opcode_wait_rotate(script[1]);
        } else if (script[0] == SCRIPT_OPCODE_WAIT_ROTATE_ALL) {
            script_opcode_wait_rotate(-1);
        } else if (script[0] == SCRIPT_OPCODE_UNIT_ANIM_ROTATE) {
            script_opcode_unit_anim_rotate((const event_unit_anim_rotate_parameters_t*)(script + 1));
        } else if (script[0] == SCRIPT_OPCODE_COLOR_UNIT) {
            map_unit_id = script_resolve_map_unit_id(first_halfword);
            if (map_unit_id != EVENT_MAP_UNIT_ID_NONE) {
                const event_unit_color_t* color = &((const event_color_unit_parameters_t*)(script + 1))->color;

                sprite_palette_apply_modulation_to_units(
                    color->mode, color->duration, map_unit_id, color->rgb.r, color->rgb.g, color->rgb.b);
            }
        }
        script += 1 + g_script_event_instruction_sizes[script[0]];
        parameters = script + 1;
    }
    thread_current_exit();
}

/*
 * SCRIPT_OPCODE_ADD_UNIT_START (0x49)
 * Event thread started by the AddUnitStart instruction: walks the following
 * script instructions through AddUnitEnd, running LoadAttackGraphics and each AddUnit.
 * A unit not yet on the field gets its status snapshot and its graphics loaded (the
 * instruction's flag passed on); one already on the field is drawn and activated unless the
 * flag is set, and its graphics refresh when the snapshot is new. Each AddUnit waits for the
 * graphics load to finish.
 */
void script_opcode_add_unit_start_thread(void) {
    s32 index;
    s32 slot;
    const u8* script;
    battle_unit_t* unit;
    s32 map_unit_id;
    s16 unit_id;
    u8 opcode;
    u8 flag;

    g_thread_contexts[g_thread_current_id].task_id = THREAD_TASK_ADD_UNIT;
    slot = 0;
    script = (const u8*)thread_current_get_parameter_1();
    while (1) {
        unit_id = script_read_s16(script + 1);
        opcode = script[0];
        flag = script[3];
        if (opcode == SCRIPT_OPCODE_ADD_UNIT_END) {
            break;
        }
        if (opcode == SCRIPT_OPCODE_LOAD_ATTACK_GRAPHICS) {
            script_opcode_load_attack_graphics();
        } else if (opcode == SCRIPT_OPCODE_ADD_UNIT) {
            unit = battle_unit_find_by_unit_id(unit_id, &index);
            if (index == -2 || index >= 0) {
                for (slot = 0; slot < BATTLE_UNIT_SLOT_COUNT; slot++) {
                    if (battle_unit_get(slot) == unit) {
                        break;
                    }
                }
                map_unit_id = map_unit_get_id_by_battle_unit_id(slot);
                if (map_unit_id == -1) {
                    battle_unit_status_staging_save(slot);
                    sprite_start_unit_graphics_load(slot, flag);
                    g_unit_graphics_load_pending = 1;
                } else {
                    if (flag == 0) {
                        map_unit_show(map_unit_id);
                    }
                    if (battle_unit_status_staging_save(slot) != 0) {
                        g_thread_call_target = (void (*)(void))map_unit_update_display_for_script;
                        thread_call_on_main_stack(map_unit_id);
                    }
                }
            }
            while (1) {
                thread_yield();
                if (g_unit_graphics_load_pending == 0) {
                    break;
                }
                if (g_debug_display_enabled != 0) {
                    FntPrint("reading charactor%d \n", slot);
                }
            }
        }
        slot++;
        script += g_script_event_instruction_sizes[script[0]] + 1;
    }
    thread_current_exit();
}
