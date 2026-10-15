/* Camera fusion thread and track stepping. */

#include "fft/battle.h"
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

/* Advances one camera-track component by a frame and returns its new value.
 * Called once for each component by the scene's camera-fusion thread.
 *
 * Each call samples the two tangent lines of the active key segment at this
 * frame and the next, intersects them (fixed_cross_product_q12 is a Q12 cross product
 * that sets g_camera_cross_product_overflow on overflow) and moves the value along the resulting
 * control point. The locals span, frame and lead are reused across the
 * stages, as the target's register map requires. */
s32 camera_step_track(camera_track_t* track, s32 component) {
    s32 time[4];  /* line 0: time[0]->time[1]; line 1: time[2]->time[3] */
    s32 value[4]; /* matching values for the two sampled lines */
    s32 value_delta0;
    s32 flag;
    s32 i;
    s32 frame;
    s32 span;
    s32 result;
    s32 segment;
    s32 start_value;
    s32 t0;
    s32 t1;
    s32 t2;
    s32 lead;
    s32 time_delta0;
    s32 time_delta1;
    s32 value_delta1;
    s32 divisor;
    s32 time0;
    s32 time1;
    s32 time2;
    s32 time3;
    s32 value0;
    s32 value1;
    s32 value2;
    s32 value3;

    /* Entering a segment: its length in frames and the value it starts from. */
    if (track->segment_frame == 0) {
        segment = track->segment;
        t2 = track->keys[segment + 2].time;
        t1 = track->keys[segment + 1].time;
        t0 = track->keys[segment].time;
        span = t2 - t1;
        lead = t1 - t0;
        frame = t2 - t0; /* whole two-key span */
        if (track->key_count >= 4) {
            if (segment == 0) {
                track->segment_duration = span / 2 + lead;
            } else if (segment == track->key_count - 3) {
                track->segment_duration = span + lead / 2;
            } else {
                track->segment_duration = frame / 2;
            }
        } else {
            track->segment_duration = frame;
        }
        start_value = track->keys[track->segment].value;
        track->prev_value = start_value;
        if (track->key_count >= 4 && track->segment != 0) {
            track->prev_value
                = (track->keys[track->segment + 1].value - track->keys[track->segment].value) / 2 + start_value;
        }
        track->prev_value *= ONE;
        track->prev_time = track->frame * ONE;
        track->start_value = track->prev_value;
    }

    /* Three equal keys hold their value. */
    if (track->keys[track->segment].value == track->keys[track->segment + 1].value
        && track->keys[track->segment].value == track->keys[track->segment + 2].value) {
        result = track->keys[track->segment].value * ONE;
    } else {
        for (i = 0; i < 2; i++) {
            frame = i + track->segment_frame;
            if (track->segment == 0) {
                time[i * 2] = script_interpolate_range_fixed12(track->keys[track->segment].time,
                    track->keys[track->segment + 1].time, frame, track->segment_duration);
                value[i * 2] = script_interpolate_range_fixed12(track->keys[track->segment].value,
                    track->keys[track->segment + 1].value, frame, track->segment_duration);
            } else {
                time[i * 2] = script_interpolate_range_fixed12(track->keys[track->segment].time,
                                  track->keys[track->segment + 1].time, frame, track->segment_duration * 2)
                    - track->keys[track->segment].time
                    + (track->keys[track->segment + 1].time - track->keys[track->segment].time) / 2;
                value[i * 2] = script_interpolate_range_fixed12(track->keys[track->segment].value,
                                   track->keys[track->segment + 1].value, frame, track->segment_duration * 2)
                    - track->keys[track->segment].value
                    + (track->keys[track->segment + 1].value - track->keys[track->segment].value) / 2;
            }
            span = track->segment_duration;
            if (track->segment != track->key_count - 3) {
                span *= 2;
            }
            time[i * 2 + 1] = script_interpolate_range_fixed12(
                track->keys[track->segment + 1].time, track->keys[track->segment + 2].time, frame, span);
            value[i * 2 + 1] = script_interpolate_range_fixed12(
                track->keys[track->segment + 1].value, track->keys[track->segment + 2].value, frame, span);
        }
        flag = 0;
        g_camera_cross_product_overflow = 0;
        value1 = value[1];
        value0 = value[0];
        time3 = time[3];
        time2 = time[2];
        time0 = time[0];
        value3 = value[3];
        value2 = value[2];
        time1 = time[1];
        value_delta0 = value1 - value0;
        time_delta1 = time3 - time2;
        value_delta1 = value3 - value2;
        time_delta0 = time1 - time0;
        divisor = fixed_cross_product_q12(value_delta0, time_delta1, time_delta0, value_delta1);
        result = fixed_cross_product_q12(time_delta1, value2 - value0, value_delta1, time2 - time0);
        if (divisor != 0) {
            span = mul_div_s64(time_delta0, result, divisor) + time0;   /* crossing time */
            lead = mul_div_s64(value_delta0, result, divisor) + value0; /* crossing value */
            divisor = span - track->prev_time;
            if (divisor != 0) {
                result = mul_div_s64(lead - track->prev_value, (track->frame * ONE) - track->prev_time, divisor)
                    + track->prev_value;
                track->prev_time = span;
                track->prev_value = lead;
            } else {
                flag = 1;
            }
        } else {
            flag = 1;
        }
        if (g_camera_cross_product_overflow != 0 || flag != 0) {
            result = track->prev_value;
        }
    }

    track->frame++;
    if (++track->segment_frame >= track->segment_duration) {
        track->segment_frame = 0;
        if (track->key_count - 3 < ++track->segment) {
            track->finished = 1;
        }
    }
    /* Angles and zoom round Q12 to the nearest integer; positions return in
     * the 1/1024 units of their script variables. */
    if (component >= EVENT_CAMERA_COMPONENT_TILT) {
        if (result > 0) {
            if ((result & 0xFFF) > 0x800) {
                result += ONE;
            }
        } else if ((result & 0xFFF) < 0x800) {
            result -= ONE;
        }
        return result / ONE;
    }
    return result / 4;
}

/* Linear interpolation from start to end at fraction / value, returned in 20.12
 * fixed point: start * ONE + (end - start) * fraction * ONE / value. */
s32 script_interpolate_range_fixed12(s32 start, s32 end, s32 fraction, s32 value) {
    return mul_div_s64(end - start, fraction * ONE, value) + (start * ONE);
}
