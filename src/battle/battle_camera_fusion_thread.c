/* Runs an event camera fusion: seeds one track per camera component from the
 * script's CAMERA instructions up to CAMERA_FUSION_END, then steps every track
 * each frame until track 3 finishes.
 *
 * Instruction-for-instruction twin of world_camera_fusion_thread
 * (0x800f14b4); only the callee and global bindings differ. The seed loop
 * reads each track through a `track` pointer taken first in the body: that
 * makes loop.c discover the track giv before the script-variable and values
 * givs, which gives the target's induction-increment order. */
#include "fft/battle.h"
#include "fft/battle_camera.h"
#include "fft/event.h"
#include "fft/main_heap.h"
#include "fft/script_variables.h"
#include "fft/thread.h"
#include "psx/types.h"

/* One track per camera component; 7 * 0xa4 == 0x47c, the allocation size the
 * target passes to battle_menu_alloc_memory. */
#define BATTLE_CAMERA_FUSION_COMPONENT_COUNT 7

/* Sentinel operand meaning "keep the component's current value". */
#define BATTLE_CAMERA_FUSION_HOLD 10000

void battle_camera_fusion_thread(void) {
    battle_camera_track_t* tracks;
    battle_camera_track_t* track;
    battle_camera_key_t* key;
    u8* script;
    u8* cursor;
    s32 values[BATTLE_CAMERA_FUSION_COMPONENT_COUNT];
    s32 key_count;
    s32 key_index;
    s32 i;
    s16 value;
    s32 component;
    s32 opcode;

    battle_thread_set_current_task_id(NATIVE_THREAD_TASK_CAMERA);
    tracks = (battle_camera_track_t*)battle_menu_alloc_memory(
        sizeof(battle_camera_track_t) * BATTLE_CAMERA_FUSION_COMPONENT_COUNT);
    script = (u8*)battle_thread_get_current_parameter_1();
    battle_clear_menu_render_buffer(tracks, sizeof(battle_camera_track_t) * BATTLE_CAMERA_FUSION_COMPONENT_COUNT);

    cursor = script;
    key_count = 0;
    for (;;) {
        opcode = *cursor;
        if (opcode == EVENT_OPCODE_CAMERA) {
            key_count++;
        }
        if (opcode == EVENT_OPCODE_CAMERA_FUSION_END) {
            break;
        }
        cursor += g_battle_script_event_instruction_sizes[opcode] + 1;
    }
    key_count++;

    cursor = script;
    battle_camera_store_yaw_band(g_battle_script_variables[EVENT_SCRIPT_VAR_CAMERA_YAW]);

    for (key_index = 0; key_index < key_count; key_index++) {
        if (key_index == 0) {
            for (i = 0; i < BATTLE_CAMERA_FUSION_COMPONENT_COUNT; i++) {
                track = &tracks[i];
                track->keys[key_index].value = g_battle_script_variables[g_battle_camera_script_variable_indices[i]];
                values[i] = track->keys[key_index].value;
                if (i < 3) {
                    track->keys[key_index].value = track->keys[key_index].value / 1024;
                }
                track->keys[key_index].time = 0;
                track->key_count = key_count;
            }
        } else {
            for (;;) {
                if (*cursor == EVENT_OPCODE_FOCUS) {
                    battle_script_focus(cursor + 1);
                } else if (*cursor == EVENT_OPCODE_UNKNOWN_73) {
                    battle_script_apply_relative_camera(cursor + 1, values);
                } else if (*cursor == EVENT_OPCODE_FOCUS_SPEED) {
                    battle_script_focus_speed(cursor + 1, values, values + 3);
                } else if (*cursor == EVENT_OPCODE_CAMERA) {
                    for (i = 0; i < BATTLE_CAMERA_FUSION_COMPONENT_COUNT; i++) {
                        value = battle_script_load_halfword(cursor + i + i + 1);
                        if (value == BATTLE_CAMERA_FUSION_HOLD) {
                            if (i < 3) {
                                value = values[i] / 1024;
                            } else {
                                value = values[i];
                            }
                        }
                        key = &tracks[i].keys[key_index];
                        key->value = value;
                        key->time = battle_script_load_halfword(cursor + 15);
                        key->time += tracks[i].keys[key_index - 1].time;
                        component = key->value;
                        values[i] = component;
                        if (i < 3) {
                            values[i] = component << 10;
                        }
                        if (i == 4) {
                            battle_camera_store_yaw_band(key->value);
                        }
                    }
                    break;
                }
                cursor += g_battle_script_event_instruction_sizes[*cursor] + 1;
            }
            cursor += g_battle_script_event_instruction_sizes[*cursor] + 1;
        }
    }

    for (;;) {
        track = tracks;
        for (key_index = 0; key_index < 3; key_index++, track++) {
            /* The target tests this flag with lhu; the s16 declaration gives lh. */
            if (*(u16*)&g_battle_camera_position_locked == 0) {
                g_battle_script_variables[g_battle_camera_script_variable_indices[key_index]]
                    = battle_camera_step_track(track, key_index);
            }
        }
        for (key_index = 3; key_index < BATTLE_CAMERA_FUSION_COMPONENT_COUNT; key_index++, track++) {
            g_battle_script_variables[g_battle_camera_script_variable_indices[key_index]]
                = battle_camera_step_track(track, key_index);
        }
        if (tracks[3].finished == 1) {
            break;
        }
        battle_script_clear_indexed_variable_bit0();
        battle_thread_yield();
    }

    battle_menu_free_memory(tracks);
    battle_thread_exit_current();
}
