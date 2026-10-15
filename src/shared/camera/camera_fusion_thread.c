#include "fft/battle_camera.h"
#include "fft/battle_menu.h"
#include "fft/gfx.h"
#include "fft/script.h"
#include "fft/thread.h"
#include "psx/stddef.h"
#include "psx/types.h"

/* Seed and advance one camera-fusion track per component. */
void camera_fusion_thread(void) {
    camera_track_t* tracks;
    camera_track_t* track;
    camera_key_t* key;
    u8* script;
    u8* cursor;
    s32 values[EVENT_CAMERA_COMPONENT_COUNT];
    s32 key_count;
    s32 key_index;
    s32 i;
    s16 value;
    s32 component;
    s32 opcode;

    thread_set_current_task_id(THREAD_TASK_CAMERA);
    tracks = menu_alloc_memory(sizeof(camera_track_t) * EVENT_CAMERA_COMPONENT_COUNT);
    script = (u8*)thread_get_current_parameter_1_variant();
    menu_clear_render_buffer(tracks, sizeof(camera_track_t) * EVENT_CAMERA_COMPONENT_COUNT);

    /* One key per CAMERA instruction up to the end marker, plus the starting key. */
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
        cursor += g_script_event_instruction_sizes[opcode] + 1;
    }
    key_count++;

    cursor = script;
    camera_store_yaw_band(g_script_variables[EVENT_SCRIPT_VAR_CAMERA_YAW]);

    /* Key 0 is the camera as it stands. Each later key is the next CAMERA
     * instruction, after the Focus, RelativeCamera and FocusSpeed instructions
     * in front of it have run. */
    for (key_index = 0; key_index < key_count; key_index++) {
        if (key_index == 0) {
            for (i = 0; i < EVENT_CAMERA_COMPONENT_COUNT; i++) {
                track = &tracks[i];
                track->keys[key_index].value = g_script_variables[g_camera_script_variable_indices[i]];
                values[i] = track->keys[key_index].value;
                if (i < EVENT_CAMERA_POSITION_COMPONENT_COUNT) {
                    track->keys[key_index].value = track->keys[key_index].value / 1024;
                }
                track->keys[key_index].time = 0;
                track->key_count = key_count;
            }
        } else {
            for (;;) {
                if (*cursor == EVENT_OPCODE_FOCUS) {
                    script_focus((event_focus_parameters_t*)(cursor + 1));
                } else if (*cursor == EVENT_OPCODE_RELATIVE_CAMERA) {
                    script_apply_relative_camera((event_camera_values_t*)(cursor + 1), values);
                } else if (*cursor == EVENT_OPCODE_FOCUS_SPEED) {
                    script_focus_speed(cursor + 1, values, values + EVENT_CAMERA_COMPONENT_TILT);
                } else if (*cursor == EVENT_OPCODE_CAMERA) {
                    for (i = 0; i < EVENT_CAMERA_COMPONENT_COUNT; i++) {
                        /* Indexed field access replaces retail's two additions with a shift. */
                        value
                            = script_read_s16(cursor + i + i + offsetof(event_camera_instruction_t, parameters.values));
                        if (value == CAMERA_FUSION_HOLD_SENTINEL) {
                            if (i < EVENT_CAMERA_POSITION_COMPONENT_COUNT) {
                                value = values[i] / 1024;
                            } else {
                                value = values[i];
                            }
                        }
                        key = &tracks[i].keys[key_index];
                        key->value = value;
                        key->time = script_read_s16(((event_camera_instruction_t*)cursor)->parameters.duration_le);
                        key->time += tracks[i].keys[key_index - 1].time;
                        component = key->value;
                        values[i] = component;
                        if (i < EVENT_CAMERA_POSITION_COMPONENT_COUNT) {
                            values[i] = component << 10;
                        }
                        if (i == EVENT_CAMERA_COMPONENT_YAW) {
                            camera_store_yaw_band(key->value);
                        }
                    }
                    break;
                }
                cursor += g_script_event_instruction_sizes[*cursor] + 1;
            }
            cursor += g_script_event_instruction_sizes[*cursor] + 1;
        }
    }

    /* Play the tracks; the three position tracks stand still while the position is locked. */
    for (;;) {
        track = tracks;
        for (key_index = 0; key_index < EVENT_CAMERA_POSITION_COMPONENT_COUNT; key_index++, track++) {
            if (g_camera_position_locked == 0) {
                g_script_variables[g_camera_script_variable_indices[key_index]] = camera_step_track(track, key_index);
            }
        }
        for (key_index = EVENT_CAMERA_COMPONENT_TILT; key_index < EVENT_CAMERA_COMPONENT_COUNT; key_index++, track++) {
            g_script_variables[g_camera_script_variable_indices[key_index]] = camera_step_track(track, key_index);
        }
        if (tracks[EVENT_CAMERA_COMPONENT_TILT].finished == 1) {
            break;
        }
        camera_clear_zoom_variable_low_bit();
        thread_yield();
    }

    menu_free_memory(tracks);
    thread_exit_current();
}
