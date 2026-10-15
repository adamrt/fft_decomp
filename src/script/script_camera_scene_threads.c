#include "fft/battle.h"
#include "fft/battle_camera.h"
#include "fft/battle_gfx.h"
#include "fft/battle_map.h"
#include "fft/battle_script.h"
#include "fft/battle_unit.h"
#include "fft/camera.h"
#include "fft/main.h"
#include "fft/map.h"
#include "fft/memory.h"
#include "fft/script.h"
#include "fft/thread.h"
#include "psx/libgte.h"
#include "psx/types.h"

/* Read a little-endian 16-bit value from a script operand, which may be unaligned. */
s16 script_read_s16(const void* source) {
    const u8* bytes = source;

    return (bytes[0] | (bytes[1] << 8));
}

/* Store a 16-bit value at an unaligned script address, low byte first. */
void script_write_s16(u8* destination, s16 value) {
    destination[0] = value;
    destination[1] = value >> 8;
}

/* Yield until this scene's pending map refresh clears. */
void map_wait_for_refresh(void) {
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
void camera_thread(void) {
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
    parameters = (const event_camera_parameters_t*)thread_get_current_parameter_1_variant();
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
void script_map_darkness(void) {
    event_unit_color_t* parameters;

    thread_set_current_task_id(THREAD_TASK_MAP_DARKNESS);
    parameters = (event_unit_color_t*)thread_get_current_parameter_1_variant();
    map_start_ambient_light_blend(
        parameters->mode, parameters->duration, parameters->rgb.r, parameters->rgb.g, parameters->rgb.b);
    thread_wait_frames(g_script_map_darkness_durations[parameters->duration]);
    thread_exit_current();
}

/* Two independent frame counters apply the two shake pulses. The task loops
 * until another instruction tears it down. */
void script_earthquake_start(void) {
    event_earthquake_parameters_t* parameters;
    s32 first_counter;
    s32 second_counter;

    thread_set_current_task_id(THREAD_TASK_EARTHQUAKE);
    parameters = (event_earthquake_parameters_t*)thread_get_current_parameter_1_variant();
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
void script_color_screen_thread(void) {
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
    parameters = (u8*)thread_get_current_parameter_1_variant();
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
void script_run_sprite_move(const event_sprite_move_parameters_t* arg, s32 use_speed) {
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
    script_run_sprite_move((const event_sprite_move_parameters_t*)thread_get_current_parameter_1_variant(), 0);
    thread_exit_current();
}

/* Thread body of SpriteMoveBeta: run the sprite move with the last operand taken
 * as a speed. */
void script_sprite_move_beta(void) {
    thread_set_current_task_id(THREAD_TASK_SPRITE_MOVE);
    script_run_sprite_move((const event_sprite_move_parameters_t*)thread_get_current_parameter_1_variant(), 1);
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
            if (thread_is_running_asm_variant(i) != 0 && g_thread_contexts[i].task_id == THREAD_TASK_SPRITE_MOVE
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

void script_copy_matrix(MATRIX* destination, const MATRIX* source) {
    mem_copy_bytes_nonempty(destination, source, sizeof(*destination));
}

/*
 * Scenario thread task 7: interpolates the map light color and its three
 * light vectors from their current values to six script-supplied targets over
 * the script's frame count, rebuilding the GTE light matrix each frame, then
 * commits the final values back to the live light state.
 */
void map_light_thread(void) {
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
    script = (const u8*)thread_get_current_parameter_1_variant();
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
        map_light_matrix_command(MAP_LIGHT_MATRIX_COMMAND_SET, &matrix, &rotation, 0);
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
s16 camera_pick_nearest_unblocked_yaw(s32 runtime_id_a, s32 runtime_id_b) {
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
    mask = map_get_tile_data_value(MAP_TILE_DATA_FLAT_CAMERA_BLOCK_MASK, g_unit_scratch_coords.vx,
        g_unit_scratch_coords.vz, g_unit_scratch_coords.vy);
    mask &= 0xFF;
    unit_copy_map_coords_by_runtime_id(runtime_id_b, &g_unit_scratch_coords);
    mask = (mask
               | map_get_tile_data_value(MAP_TILE_DATA_FLAT_CAMERA_BLOCK_MASK, g_unit_scratch_coords.vx,
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
