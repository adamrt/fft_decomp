#include "fft/battle_gfx.h"
#include "fft/battle_script.h"
#include "fft/battle_unit.h"
#include "fft/script.h"
#include "fft/sound.h"
#include "fft/thread.h"

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
            thread_id = battle_thread_resolve_id_after_current(THREAD_SLOT_FIRST_IDLE);
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
