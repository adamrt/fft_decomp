#include "fft/battle.h"

struct battle_script_jump_instruction;
extern void battle_script_jump_event_instruction(struct battle_script_jump_instruction* parameters);

/* BATTLE twin of world_block_start_thread. Thread body for an event BLOCK_START: runs the enclosed instructions up to
 * BLOCK_END on its own thread, dispatching the unit-movement, animation and
 * wait opcodes that may appear inside a block, then exits. */
void battle_block_start_thread(void) {
    /* Preserve the target's unused 40-byte stack area. */
    s32 unused[10];
    u8* script;
    u8* parameters;
    s16 first_halfword;
    s16 misc_id;
    s32 thread_id;

    g_battle_thread_contexts[g_battle_current_thread_id].task_id = NATIVE_THREAD_TASK_EVENT_BLOCK;
    script = battle_thread_get_current_parameter_1();
    parameters = script + 1;
    while (script[0] != EVENT_OPCODE_BLOCK_END) {
        first_halfword = battle_script_load_halfword(parameters);
        if (script[0] == EVENT_OPCODE_CALL_FUNCTION) {
            if (script[1] == EVENT_CALL_FUNCTION_MUTE_TEXT_AUDIO_CUE) {
                battle_script_set_variable(EVENT_SCRIPT_VAR_MUTE_TEXT_AUDIO_CUE, 1);
            } else if (script[1] == EVENT_CALL_FUNCTION_UNMUTE_TEXT_AUDIO_CUE) {
                battle_script_set_variable(EVENT_SCRIPT_VAR_MUTE_TEXT_AUDIO_CUE, 0);
            }
        } else if (script[0] == EVENT_OPCODE_SOUND_EFFECT) {
            g_sound_effect_id_to_play = first_halfword;
        } else if (script[0] == EVENT_OPCODE_WARP_UNIT) {
            battle_script_warp_unit(parameters);
        } else if (script[0] == EVENT_OPCODE_WAIT_VALUE) {
            battle_script_wait_value_event_instruction(parameters);
        } else if (script[0] == EVENT_OPCODE_UNKNOWN_72) {
            battle_script_print_debug_message();
        } else if (script[0] == EVENT_OPCODE_WALK_TO) {
            g_battle_thread_call_target = battle_script_walk_to_thread;
            battle_thread_call_on_main_stack(parameters);
        } else if (script[0] == EVENT_OPCODE_WALK_TO_ANIM) {
            misc_id = battle_get_misc_id(first_halfword);
            if (misc_id != EVENT_MISC_ID_NONE) {
                g_battle_unit_pending_animation_ids[misc_id] = battle_script_load_halfword(script + 3);
            }
            g_battle_unit_was_moving_latch[misc_id] = 0;
        } else if (script[0] == EVENT_OPCODE_JUMP) {
            battle_script_jump_event_instruction((struct battle_script_jump_instruction*)parameters);
        } else if (script[0] == EVENT_OPCODE_WAIT_WALK) {
            battle_script_waitwalk_event_instruction(first_halfword);
        } else if (script[0] == EVENT_OPCODE_UNIT_ANIM) {
            battle_script_unit_anim(parameters);
        } else if (script[0] == EVENT_OPCODE_MIRROR_SPRITE) {
            battle_script_mirrorsprite_event_instruction(script + 1);
        } else if (script[0] == EVENT_OPCODE_RESET_PALETTE) {
            battle_script_resetpalette_event_instruction(script + 1);
        } else if (script[0] == EVENT_OPCODE_SET_FIRST_MISC_UNIT) {
            misc_id = battle_get_misc_id(first_halfword);
            if (misc_id != EVENT_MISC_ID_NONE) {
                battle_unit_move_misc_unit_to_head(misc_id);
            }
        } else if (script[0] == EVENT_OPCODE_ERASE) {
            misc_id = battle_get_misc_id(first_halfword);
            if (misc_id != EVENT_MISC_ID_NONE) {
                battle_gfx_reset_unit_graphic_trigger(misc_id);
            }
        } else if (script[0] == EVENT_OPCODE_DRAW) {
            misc_id = battle_get_misc_id(first_halfword);
            if (misc_id != EVENT_MISC_ID_NONE) {
                battle_unit_animate_and_set_enemy_level_data_by_misc_id(misc_id);
            }
        } else if (script[0] == EVENT_OPCODE_UNKNOWN_12) {
            misc_id = battle_get_misc_id(first_halfword);
            if (misc_id != EVENT_MISC_ID_NONE) {
                battle_script_wait_for_unit_ready(misc_id);
            }
        } else if (script[0] == EVENT_OPCODE_WAIT) {
            battle_thread_wait_frames(battle_script_load_halfword(script + 1));
        } else if (script[0] == EVENT_OPCODE_ROTATE_UNIT) {
            battle_rotate_unit(script + 1);
        } else if (script[0] == EVENT_OPCODE_FACE_UNIT) {
            battle_script_face_unit(script + 1, 1);
        } else if (script[0] == EVENT_OPCODE_SPRITE_MOVE || script[0] == EVENT_OPCODE_SPRITE_MOVE_BETA) {
            thread_id = battle_thread_resolve_id_after_current(0x10);
            if (script[0] == EVENT_OPCODE_SPRITE_MOVE) {
                battle_thread_start(thread_id, battle_script_sprite_move);
            } else {
                battle_thread_start(thread_id, battle_script_sprite_move_beta);
            }
            g_battle_thread_contexts[thread_id].function_parameter_1 = (s32)(script + 1);
            g_battle_thread_contexts[thread_id].task_id = NATIVE_THREAD_TASK_SPRITE_MOVE;
            g_battle_thread_contexts[thread_id].task_words[0] = battle_get_misc_id(first_halfword);
        } else if (script[0] == EVENT_OPCODE_WAIT_SPRITE_MOVE) {
            battle_script_waitspritemove_event_instruction(first_halfword);
        } else if (script[0] == EVENT_OPCODE_WAIT_ROTATE_UNIT) {
            battle_script_waitrotateunit_and_waitrotateall_event_instruction(script[1]);
        } else if (script[0] == EVENT_OPCODE_WAIT_ROTATE_ALL) {
            battle_script_waitrotateunit_and_waitrotateall_event_instruction(-1);
        } else if (script[0] == EVENT_OPCODE_UNIT_ANIM_ROTATE) {
            battle_script_unit_animation_rotate_event_instruction(script + 1);
        } else if (script[0] == EVENT_OPCODE_COLOR_UNIT) {
            misc_id = battle_get_misc_id(first_halfword);
            if (misc_id != EVENT_MISC_ID_NONE) {
                battle_gfx_start_misc_unit_palette_modulation(
                    script[3], script[7], misc_id, (s8)script[4], (s8)script[5], (s8)script[6]);
            }
        }
        script += 1 + g_battle_script_event_instruction_sizes[script[0]];
        parameters = script + 1;
    }
    battle_thread_exit_current();
}
