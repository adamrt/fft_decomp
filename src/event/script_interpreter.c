/* Event execution and loading, s16 reads and writes, the camera and map light threads, darkness, earthquake, sprite
 * moves, matrix copies, camera focus, unit filters, teleport, warp, rotation, staged status data, inflict status, stat
 * changes and marching units. */

#include "fft/ai.h"
#include "fft/animation.h"
#include "fft/battle_map.h"
#include "fft/battle_map_light.h"
#include "fft/camera.h"
#include "fft/effect_secondary.h"
#include "fft/fade.h"
#include "fft/gfx.h"
#include "fft/job.h"
#include "fft/memory.h"
#include "fft/menu_input.h"
#include "fft/move.h"
#include "fft/script.h"
#include "fft/script_instruction.h"
#include "fft/script_variable.h"
#include "fft/skillset.h"
#include "fft/sound.h"
#include "fft/sprite.h"
#include "fft/status.h"
#include "fft/system.h"
#include "fft/thread.h"
#include "fft/unit.h"
#include "fft/unit_event.h"
#include "fft/unit_identity.h"
#include "fft/unit_runtime.h"
#include "psx/libc.h"
#include "psx/libgte.h"
#include "psx/types.h"

static s32 g_script_loaded_event_id = -1;
static u16 g_script_map_darkness_durations[12] = { 1, 8, 16, 32, 64, 96, 128, 160, 192, 224, 0x100, 0 };
static s16 g_camera_diagonal_yaws[4] = { 3584, 2560, 1536, 512 };
static u16 g_script_unit_face_slot_order[24]
    = { 0, 4, 8, 12, 16, 20, 1, 5, 9, 13, 17, 21, 2, 6, 10, 14, 18, 3, 7, 11, 15, 19, 0x302, 0x100 };

/* Frames per facing step, indexed by the RotateUnit speed operand. */
static u8 g_unit_rotation_speed_frames[8] = { 4, 2, 1, 0, 0, 3, 2, 1 };

static u8 g_unit_status_staging_masks[UNIT_STATUS_STAGING_MASK_COUNT][8]
    = { 20, 176, 22, 12, 49, 0, 0, 0, 31, 188, 254, 255, 255, 0, 0, 0, 15, 188, 190, 255, 255, 0, 0, 0 };

static void status_inflict_by_event_unit_id(s32 event_unit_id, s32 status, s32 which, s32 removal_only);

/* Wait until no thread runs a task from THREAD_TASK_CAMERA to THREAD_TASK_ADD_UNIT, then
 * load the event the current-event variable names unless it is already loaded. Returns
 * 1 when it loaded one, else 0. */
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
s32 unit_is_moving(s32 runtime_id) {
    s32 i;

    if (runtime_id != -1) {
        if (move_is_unit_moving_by_runtime_id(runtime_id) != 0) {
            return 1;
        }
    } else {
        for (i = 0; i < UNIT_SLOT_COUNT; i++) {
            if ((unit_has_runtime_id(i) != 0) && (move_is_unit_moving_by_runtime_id(i) != 0)) {
                return 1;
            }
        }
    }
    return 0;
}

/* Read a 16-bit value from an unaligned script address, low byte first. */
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
void battle_map_wait_for_refresh(void) {
    if (g_battle_map_refresh_pending != 0) {
        do {
            thread_yield();
        } while (g_battle_map_refresh_pending != 0);
    }
}

/* Clear bit 0 of the zoom variable selected by the camera component-index table. */
void camera_clear_zoom_variable_low_bit(void) {
    g_script_variables[g_camera_script_variable_indices[EVENT_CAMERA_COMPONENT_ZOOM]] &= ~1;
}

#define ABS(x) (x < 0 ? -(x) : x)

/*
 * The event CAMERA instruction (0x19) thread.
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
    s32 remaining;

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

    /* Move the current yaw up a full turn when that is nearer the target, so the camera
     * turns the short way. */
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
    /* Positions blend in 1/256 units: drop the low 12 bits of the 1/1024 value, then scale. */
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
            /* Components within 6 position units (or 1.5 rotation units) of the target skip the
             * blend; the assignments after the loop still land them. */
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

            /* Mode 0 and the group the curve leaves linear (0: rotations, 1: positions) move linearly. */
            if (mode == 0 || (group == 0 && j >= EVENT_CAMERA_COMPONENT_TILT)
                || (group == 1 && j < EVENT_CAMERA_POSITION_COMPONENT_COUNT)) {
                value = mul_div_s64(delta, i, duration) + base;
                if (j < EVENT_CAMERA_POSITION_COMPONENT_COUNT) {
                    value *= 4;
                }
            } else if (mode == 1) {
                decel = delta * weight;
                accel = delta * ease * 2;
                /* accel + 2 * decel is 32 * delta, so `first` is the linear term; the ramp term bows it. */
                first = mul_div_s64(accel + decel + decel, i, duration * 32);
                value = mul_div_s64(accel, ramp, duration * duration * 32) + first + base;
                if (j < EVENT_CAMERA_POSITION_COMPONENT_COUNT) {
                    value *= 4;
                }
            } else {
                /* Mode 2 eases each half: the first accelerates to the midpoint, the second
                 * decelerates into the target. */
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
                    remaining = mul_div_s64(accel, duration - elapsed, duration);
                    value = mul_div_s64(accel + remaining + decel + decel, elapsed, duration * 32);
                    if (j < EVENT_CAMERA_POSITION_COMPONENT_COUNT) {
                        value += delta;
                        value += base * 4;
                    } else {
                        value = (value + delta) / 4 + base;
                    }
                }
            }

            /* Rotations round from 1/64 units; the 0xFF mask (not 0x3F) also tests two integer
             * bits, so the half-step test is uneven (retail). */
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

    /* Land exactly on the targets, positions back in 1/1024 units. */
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
void script_opcode_1a_map_darkness(void) {
    event_unit_color_t* parameters;

    thread_set_current_task_id(THREAD_TASK_MAP_DARKNESS);
    parameters = (event_unit_color_t*)thread_get_current_parameter_1();
    battle_map_ambient_start_blend(
        parameters->mode, parameters->duration, parameters->rgb.r, parameters->rgb.g, parameters->rgb.b);
    thread_wait_frames(g_script_map_darkness_durations[parameters->duration]);
    thread_exit_current();
}

/* Thread body of Earthquake: each of the two pulses adds its z delta to the camera shake
 * offset every pulses[k].frames frames, on independent counters. The thread never ends on
 * its own; another instruction tears it down. */
void script_opcode_41_earthquake_start(void) {
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

/* Thread body of ColorScreen: blend the screen color modulation from the start RGB to the
 * end RGB over the duration in frames, restarting a 2-frame modulation fade every second
 * frame, then settle on the end color. */
void script_opcode_3e_color_screen_thread(void) {
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
        fade_tint_start_fade(mode, color.current[0], color.current[1], color.current[2], 2);
    }
    for (j = 0; j < 3; j++) {
        color.current[j] = color.start_and_end[j + 3];
    }
    fade_tint_start_fade(mode, color.current[0], color.current[1], color.current[2], 2);
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
    g_thread_contexts[g_thread_current_id].task_words[SPRITE_MOVE_THREAD_WORD_RUNTIME_ID] = runtime_id;
    coords = unit_get_event_offset_by_runtime_id(runtime_id);
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
    /* Curve 0 is linear, curves 1 and 3 are bowed by the weight, and curve 2 eases into the
     * midpoint and out of it again. The final step after the loop lands on the destination. */
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
        if (unit_has_runtime_id(runtime_id) == 0) {
            thread_exit_current();
        }
        vec.vx = pos[3][0];
        vec.vy = pos[3][1];
        vec.vz = pos[3][2];
        unit_add_event_offset_by_runtime_id(runtime_id, &vec);
        vec.vx = pos[2][0] / 256;
        vec.vy = pos[2][1] / 256;
        vec.vz = pos[2][2] / 256;
        unit_add_event_offset_by_runtime_id(runtime_id, &vec);
        pos[3][0] = -vec.vx;
        pos[3][1] = -vec.vy;
        pos[3][2] = -vec.vz;
    }
    if (unit_has_runtime_id(runtime_id) == 0) {
        thread_exit_current();
    }
    vec.vx = pos[3][0];
    vec.vy = pos[3][1];
    vec.vz = pos[3][2];
    unit_add_event_offset_by_runtime_id(runtime_id, &vec);
    vec.vx = pos[1][0] / 256;
    vec.vy = pos[1][1] / 256;
    vec.vz = pos[1][2] / 256;
    unit_add_event_offset_by_runtime_id(runtime_id, &vec);
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
void script_opcode_6f_wait_sprite_move(s32 unit_id) {
    s32 runtime_id = script_resolve_runtime_id(unit_id);
    s32 i;

    if (runtime_id == EVENT_RUNTIME_ID_NONE) {
        return;
    }
    for (;;) {
        for (i = 0; i < VARIANT_THREAD_TABLE_SLOT_COUNT; i++) {
            if (thread_is_running_asm(i) != 0 && g_thread_contexts[i].task_id == THREAD_TASK_SPRITE_MOVE
                && g_thread_contexts[i].task_words[SPRITE_MOVE_THREAD_WORD_RUNTIME_ID] == runtime_id) {
                break;
            }
        }
        if (i == VARIANT_THREAD_TABLE_SLOT_COUNT) {
            return;
        }
        thread_yield();
    }
}

/* Copy a whole MATRIX, rotation and translation. */
static void script_copy_matrix(MATRIX* destination, const MATRIX* source) {
    mem_copy_bytes_nonempty(destination, source, sizeof(*destination));
}

/*
 * Scenario thread task 7: interpolates the map light color and its three
 * light vectors from their current values to six script-supplied targets over
 * the script's frame count, rebuilding the GTE light matrix each frame, then
 * commits the final values back to the live light state.
 */
void battle_map_light_thread(void) {
    /* One keyframe: three rotation angles and three color scale factors.
     * The original reads all six operands in one loop, so the target also
     * needs a six-word view; stores through the named fields do not match. */
    union {
        struct {
            s32 angles[3];
            s32 scale[3];
        } light;
        s32 values[6];
    } start, target;
    SVECTOR angles;
    MATRIX direction;
    MATRIX color;
    VECTOR scale;
    s32 step;
    s32 frames;
    const u8* script;

    thread_set_current_task_id(THREAD_TASK_MAP_LIGHT);
    step = 0;
    script = (const u8*)thread_get_current_parameter_1();
    do {
        target.values[step] = script_read_s16(script);
        script += 2;
        step++;
    } while (step < 6);
    /* The six-target scan leaves the cursor at duration_le. */
    frames = script_read_s16(script);
    step = 0;
    start.light.angles[0] = g_script_light_angles.vx;
    start.light.angles[1] = g_script_light_angles.vy;
    start.light.angles[2] = g_script_light_angles.vz;
    start.light.scale[0] = g_script_light_scale.vx;
    start.light.scale[1] = g_script_light_scale.vy;
    start.light.scale[2] = g_script_light_scale.vz;
    for (; step <= frames; step++) {
        angles.vx = (target.light.angles[0] - start.light.angles[0]) * step / frames + start.light.angles[0];
        angles.vy = (target.light.angles[1] - start.light.angles[1]) * step / frames + start.light.angles[1];
        angles.vz = (target.light.angles[2] - start.light.angles[2]) * step / frames + start.light.angles[2];
        RotMatrix(&angles, &direction);
        MulMatrix(&direction, &g_script_light_base_direction_matrix);
        script_copy_matrix(&color, &g_script_light_base_color_matrix);
        scale.vx = (target.light.scale[0] - start.light.scale[0]) * step / frames + start.light.scale[0];
        scale.vy = (target.light.scale[1] - start.light.scale[1]) * step / frames + start.light.scale[1];
        scale.vz = (target.light.scale[2] - start.light.scale[2]) * step / frames + start.light.scale[2];
        ScaleMatrixL(&color, &scale);
        battle_map_light_matrix_command(BATTLE_MAP_LIGHT_MATRIX_COMMAND_SET, &color, &direction, 0);
        thread_wait_frames(1);
    }
    mem_copy_bytes_nonempty(&g_script_light_angles, &angles, sizeof(g_script_light_angles));
    mem_copy_bytes_nonempty(&g_script_light_scale, &scale, sizeof(g_script_light_scale));
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
    mask = battle_map_tile_get_value(BATTLE_MAP_TILE_DATA_FLAT_CAMERA_BLOCK_MASK, g_unit_scratch_coords.vx,
        g_unit_scratch_coords.vz, g_unit_scratch_coords.vy);
    mask &= 0xFF;
    unit_copy_map_coords_by_runtime_id(runtime_id_b, &g_unit_scratch_coords);
    mask = (mask
               | battle_map_tile_get_value(BATTLE_MAP_TILE_DATA_FLAT_CAMERA_BLOCK_MASK, g_unit_scratch_coords.vx,
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

/* RelativeCamera: write its seven camera components, offset by the current camera variables
 * camera_values, into the Camera instruction that follows it (skipping one FocusSpeed in
 * between). Positions convert from the variables' 1/1024 units; hold sentinels pass through
 * unchanged. */
void script_apply_relative_camera(event_camera_values_t* parameters, const s32* camera_values) {
    u8* source = parameters->position_le[0];
    const s32* deltas = camera_values;
    u8* destination = ((event_camera_instruction_t*)(parameters + 1))->parameters.values.position_le[0];
    s32 index;
    s32 delta;
    s32 value;

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

/* Focus: patch the next Camera instruction in the script to frame two units.
 *
 * Its position becomes the units' midpoint scaled by 4 (the sum of both positions doubled),
 * with y offset by 0x20, and further when the first unit is Altima's second form or the
 * second unit Altima's first form. Unless keep_angle is set, its yaw becomes the nearest diagonal not
 * blocked at either unit's tile. When either unit is missing, the Camera instruction is
 * disabled and system error 7 is reported. */
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
        system_report_error(7, 0);
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

/* Empty in BATTLE and WORLD apart from a 48-byte stack frame (the unused twelve-word local).
 * Nothing in our sources calls it; the name is a neutral placeholder. */
void script_noop_11(void) {
    s32 unused[12];
}

/* Prepare an event unit selector for script_filter_unit_id_by_mode. A single unit id is
 * resolved in place to its runtime id with filter mode 0 (returning 0 when it does not
 * resolve). Id 0 selects every unit (mode 1), and the group ids from EVENT_UNIT_GROUP_FIRST
 * select modes 2-5: the blue team, the blue team without excluded statuses, the other
 * teams, and the other teams without them. Callers then loop over all unit slots and stop
 * after the first match in mode 0. */
static s32 script_prepare_unit_filter(u16* unit_id, s32* filter_mode) {
    u32 id = *unit_id;

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
 *   0  always accepted, and out_id keeps the single unit already resolved
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
    if (unit_has_runtime_id(*out_id) != 0) {
        index = unit_get_battle_id_by_runtime_id(*out_id);
        if (index != -1) {
            unit = unit_get_stats(index);
        }
    }
    if (*mode == 1) {
        return unit_has_runtime_id(*out_id);
    }
    if (unit_has_runtime_id(*out_id) == 0) {
        return 0;
    }
    query = *mode;
    if (query == 2) {
        if (index == -1) {
            return 1;
        }
        return (unit->team_flags & UNIT_TEAM_MASK) == UNIT_TEAM_BLUE;
    }
    if (query == 3) {
        if (index == -1) {
            return 1;
        }
        for (i = 0; i < STATUS_BYTE_COUNT; i++) {
            if ((unit->status_sets.current[i] & g_status_check_sets[STATUS_CHECK_SET_SCRIPT_FILTER_EXCLUDED][i]) != 0) {
                break;
            }
        }
        if ((unit->team_flags & UNIT_TEAM_MASK) == UNIT_TEAM_BLUE && i == STATUS_BYTE_COUNT) {
            return 1;
        }
        return 0;
    }
    if (query == 4) {
        if (index == -1) {
            return 0;
        }
        return (unit->team_flags & UNIT_TEAM_MASK) != UNIT_TEAM_BLUE;
    }
    if (query == 5) {
        if (index == -1) {
            return 0;
        }
        first = g_status_check_sets[STATUS_CHECK_SET_SCRIPT_FILTER_EXCLUDED][0];
        for (i = 0; i < STATUS_BYTE_COUNT; i++) {
            if (i == 0) {
                s32 status = unit->status_sets.current[0] & (0xff ^ STATUS_BYTE_MASK(STATUS_ID_UNDEAD));

                blocked = first & status;
            } else {
                blocked
                    = unit->status_sets.current[i] & g_status_check_sets[STATUS_CHECK_SET_SCRIPT_FILTER_EXCLUDED][i];
            }
            if (blocked != 0) {
                break;
            }
        }
        if ((unit->team_flags & UNIT_TEAM_MASK) != UNIT_TEAM_BLUE && i == STATUS_BYTE_COUNT) {
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
void script_opcode_94_teleport_out(s32 unit_id, s32 remove) {
    s32 unit = script_resolve_runtime_id(unit_id);
    s32 idx;

    if (unit == EVENT_RUNTIME_ID_NONE) {
        return;
    }
    g_sound_pending_sfx_id = SOUND_SFX_TELEPORT_OUT;
    g_thread_call_target = (void (*)(void))effect_secondary_set_teleport_by_runtime_id;
    thread_call_on_main_stack(unit);
    idx = unit_get_battle_id_by_runtime_id(unit);
    unit_set_animation(unit, ANIMATION_RAISE_HANDS);
    unit_fade_out_start_for_event(unit, 2, 0x1F, 0x1F, 0x1F);
    thread_wait_frames(0x3C);
    unit_fade_out_start_for_event(unit, 2, -0x1F, -0x1F, -0x1F);
    thread_wait_frames(0x3C);
    if (remove != 0) {
        unit_set_off_field(idx);
    }
}

/* Start the teleport-in effect for an event unit. The interpreter supplies 1
 * as its second argument; this entry forwards only the unit ID. */
void script_opcode_98_teleport_in(s32 unit_id, s32 unused) {
    s32 runtime_id = script_resolve_runtime_id(unit_id);
    if (runtime_id != EVENT_RUNTIME_ID_NONE) {
        g_thread_call_target = (void (*)(void))effect_secondary_set_teleport_by_runtime_id;
        thread_call_on_main_stack(runtime_id);
    }
}

/* CallFunction 4: tint blue (red and green modulated by -0x1f) every displayed
 * unit whose initial team is not the blue one, skipping units whose ENTD lookup
 * reports -2. */
void script_blue_remove_all_enemy_units(void) {
    s32 i;
    s32 idx;
    s32 out_index;
    unit_stats_t* stats;
    unit_stats_t* found;

    for (i = 0; i < UNIT_SLOT_COUNT; i++) {
        if (unit_has_runtime_id(i) != 0) {
            idx = unit_get_battle_id_by_runtime_id(i);
            if (idx != -1) {
                stats = unit_get_stats(idx);
                found = unit_entd_find_stats(stats->unit_id, &out_index);
                if ((found->initial_team_flags & UNIT_TEAM_MASK) && out_index != -2) {
                    unit_fade_out_start_for_event(i, 2, -0x1F, -0x1F, 0);
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
void script_opcode_99_blue_remove_unit(s32 unit_id) {
    s32 runtime_id;
    s32 battle_index;
    unit_stats_t* unit;
    s32 found_battle_id[2];

    for (runtime_id = 0; runtime_id < UNIT_SLOT_COUNT; runtime_id++) {
        if (unit_has_runtime_id(runtime_id) == 0) {
            continue;
        }
        battle_index = unit_get_battle_id_by_runtime_id(runtime_id);
        if (battle_index == -1) {
            continue;
        }
        unit = unit_entd_find_stats(unit_get_stats(battle_index)->unit_id, found_battle_id);
        if ((unit->team_flags & UNIT_TEAM_MASK) == UNIT_TEAM_BLUE) {
            continue;
        }
        if (found_battle_id[0] == -2) {
            continue;
        }
        if (unit->unit_id != unit_id) {
            continue;
        }
        unit_fade_out_start_for_event(runtime_id, 2, -0x1F, -0x1F, 0);
    }
}

/*
 * WarpUnit event instruction: move an ENTD unit to a new tile and facing.
 *
 * Parameters: 0x00 ENTD unit id (16-bit value), 0x02 x, 0x03 y, 0x04 higher
 * elevation, 0x05 facing.
 */
void script_opcode_5f_warp_unit(const event_warp_unit_parameters_t* instruction) {
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
    battle_map_tile_coordinates_t tile;
    unit_animation_state_t* rotation_state;

    cursor.instruction = instruction;
    unit_id = script_read_s16(cursor.instruction->unit_id_le);
    cursor.placement = &cursor.instruction->placement;
    unit = unit_entd_find_stats(unit_id, &result);
    battle_id = 0;
    if (result != -2 && result < 0) {
        return;
    }
    for (; battle_id < UNIT_SLOT_COUNT; battle_id++) {
        if (unit_get_stats(battle_id) == unit) {
            break;
        }
    }
    unit->x = cursor.placement->x;
    unit->position.y = cursor.placement->y;
    unit->position.higher_elevation = cursor.placement->elevation;
    unit->position.facing = cursor.placement->facing;
    runtime_id = unit_get_runtime_id_by_battle_id(battle_id);
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
    unit_place_in_new_location(runtime_id, &tile, cursor.placement->facing);
    unit_set_facing(runtime_id, cursor.placement->facing * 4);
}

/* Set facing and animation immediately; the animation high byte also controls facing-update suppression. */
void script_opcode_8c_unit_anim_rotate(const event_unit_anim_rotate_parameters_t* parameters) {
    u8 rotation;
    s16 animation;
    s16 call_runtime_id;
    s16 runtime_id = script_read_s16(parameters->unit_id_le);
    s32 index;
    unit_animation_state_t* rotation_state;

    rotation = parameters->rotation;
    animation = script_read_s16(parameters->animation_le);
    call_runtime_id = script_resolve_runtime_id(runtime_id);
    runtime_id = call_runtime_id;
    if (parameters->animation_le[1] == 0) {
        unit_clear_facing_update_suppression(call_runtime_id);
    } else {
        unit_set_facing_update_suppression(call_runtime_id);
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
void script_opcode_2c_face_unit(const event_face_unit_parameters_t* parameters, s32 second_only) {
    SVECTOR position;
    SVECTOR target;
    u16 unit_id;
    s32 filter_mode;
    u16 runtime_id;
    s32 i;
    s32 order = 1;
    s32 facing;
    s32 reverse;
    s32 dx;
    s32 dy;
    unit_animation_state_t* state;

    runtime_id = script_read_s16(parameters->first_unit_id_le);
    unit_id = script_read_s16(parameters->second_unit_id_le);
    if (script_prepare_unit_filter(&unit_id, &filter_mode) != 0) {
        runtime_id = script_resolve_runtime_id(runtime_id);
        if (runtime_id != EVENT_RUNTIME_ID_NONE) {
            for (i = 0; i < UNIT_SLOT_COUNT; i++) {
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
void script_opcode_2d_rotate_unit(const event_rotate_unit_parameters_t* parameters) {
    u16 unit_id;
    s32 filter_mode;
    s32 i;
    s32 order = 1;
    s32 facing;
    s32 saved_filter_mode;
    unit_animation_state_t* state;

    unit_id = script_read_s16(parameters->unit_id_le);
    if (script_prepare_unit_filter(&unit_id, &filter_mode) != 0) {
        for (i = 0; i < UNIT_SLOT_COUNT; i++) {
            if (script_filter_unit_id_by_mode(&unit_id, &g_script_unit_face_slot_order[i], &filter_mode) != 0) {
                facing = parameters->facing;
                if (facing >= EVENT_ROTATE_UNIT_FACING_RELATIVE_FIRST && facing < EVENT_ROTATE_UNIT_FACING_RESTORE) {
                    facing = facing & 0xF;
                    facing = unit_facing_get_sixteenth_by_runtime_id(unit_id) + facing * 4;
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
void status_queue_current_status_graphics(s32 battle_id, s32 enabled) {
    unit_stats_t* stats = unit_get_stats(battle_id);
    s32 i;
    s32 mask;

    g_thread_call_target = (void (*)(void))status_queue_display_flag_change;
    for (i = 0; i < STATUS_COUNT; i++) {
        mask = 0x80 >> (i % 8);
        if ((stats->status_sets.current[i / 8] & mask) != 0) {
            thread_call_on_main_stack(STATUS_HANDLER_INDEX(i), enabled, battle_id);
        }
    }
}

/* Resets every unit slot's status staging state and inflict-status mode to none. */
void unit_clear_status_staging_data(void) {
    s32 i;
    for (i = 0; i < UNIT_SLOT_COUNT; i++) {
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
        stats = unit_get_stats(battle_id);
        for (i = 0; i < STATUS_BYTE_COUNT; i++) {
            g_unit_status_staging_data->innate[battle_id][i] = stats->status_sets.innate[i];
            g_unit_status_staging_data->inflicted[battle_id][i] = stats->inflicted_status[i];
            g_unit_status_staging_data->current[battle_id][i] = stats->status_sets.current[i];
        }
        for (i = 0; i < STATUS_TIMED_COUNT; i++) {
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
        g_thread_call_target = (void (*)(void))status_apply_unit_action_removal;
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
 * the unit graphics are refreshed. The RESTORED state prevents applying the
 * snapshot twice, and nothing is restored when bit 15 of the scenario's finish
 * operation is set. A pending inflict-status mode then applies its change for real.
 */
static void unit_apply_staged_status_data(s32 battle_id, s32 runtime_id) {
    unit_stats_t* unit;
    s32 i;
    s32 byte_index;
    s32 mask;
    u8 inflict_status_mode;
    if (!(scenario_get_finish_operation() & 0x8000)
        && g_unit_status_staging_data->state[battle_id] == UNIT_STATUS_STAGING_SAVED) {
        unit = unit_get_stats(battle_id);
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
        for (i = 0; i < STATUS_TIMED_COUNT; i++) {
            unit->status_ct[i] = g_unit_status_staging_data->status_ct[battle_id][i];
        }
        unit->hp = g_unit_status_staging_data->hp[battle_id];
        unit->team_flags = g_unit_status_staging_data->team_flags[battle_id];
        unit->death_counter = g_unit_status_staging_data->death_counter[battle_id];
        /* Restoring the snapshot undoes the event's changes: the status graphics the event
         * added are switched off and the ones it removed back on.
         * WORLD's copy targets this BATTLE address too (see QUIRKS.md). */
        g_thread_call_target = (void (*)(void))status_queue_display_flag_change;
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
            g_thread_call_target = (void (*)(void))status_resolve_unit_changes;
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
        g_thread_call_target = (void (*)(void))unit_update_graphics_by_runtime_id;
        thread_call_on_main_stack(runtime_id);
    }
}

/* Save or restore the event status snapshot of each unit the event unit id
 * selects (one unit or a unit group), under player input only.
 *
 * With restore clear, each unit is snapshotted and, when saved, its graphics are
 * refreshed on the main stack; the jumping units are hidden at
 * the end. Otherwise the snapshot is replayed through
 * unit_apply_staged_status_data. Removing Jump or Float from any unit adds a
 * 40-frame wait, and removing Jump adds another. */
void unit_update_staged_status_data(u16 unit_id, u16 restore) {
    s32 filter_mode;
    s32 unit_index;
    s32 battle_id;
    s32 battle_unit_index;

    g_unit_status_staging_data = &g_ai_workspace_ptr->status_staging;
    if (g_menu_input_mode == MENU_INPUT_PLAYER && script_prepare_unit_filter(&unit_id, &filter_mode) != 0) {
        unit_index = 0;
        g_unit_status_staging_data->jump_float_flags = 0;
        for (; unit_index < UNIT_SLOT_COUNT; unit_index++) {
            if (script_filter_unit_id_by_mode(&unit_id, (u16*)&unit_index, &filter_mode) != 0) {
                battle_unit_index = unit_get_battle_id_by_runtime_id(unit_id);
                if (battle_unit_index != -1) {
                    unit_entd_find_stats(unit_get_stats(battle_unit_index)->unit_id, &battle_id);
                    if (battle_id >= 0) {
                        if (restore == 0) {
                            if (unit_save_staged_status_data(battle_unit_index) != 0) {
                                g_thread_call_target = (void (*)(void))unit_update_graphics_by_runtime_id;
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
            g_thread_call_target = unit_hide_all_jumping;
            thread_call_on_main_stack();
        }
    }
}

/* Thread body of InflictStatus: snapshot the unit's statuses (unit_update_staged_status_data),
 * then poison it, crystallize it, or revive it at 1 HP with Critical when it is dead. The
 * matching inflict-status mode makes the snapshot restore at the event's end apply the same
 * change for real. REVIVE on a unit that is neither dead nor Critical only refreshes its
 * mounted animation. Waits wait_frames frames before exiting. */
void script_inflict_status_thread(void) {
    event_inflict_status_parameters_t* parameters;
    s32 unit_id;
    s32 mode;
    s32 wait_frames;
    s32 battle_unit_index;
    s32 runtime_id;
    unit_stats_t* unit;
    s32 health; /* 0 dead, 1 Critical, 2 otherwise */

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
    unit = unit_get_stats(battle_unit_index);
    if (unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_DEAD)] & STATUS_BYTE_MASK(STATUS_ID_DEAD)) {
        health = 0;
    } else {
        health = 2;
        if (unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_CRITICAL)] & STATUS_BYTE_MASK(STATUS_ID_CRITICAL)) {
            health = 1;
        }
    }
    if (mode == EVENT_INFLICT_STATUS_POISON) {
        status_inflict_by_event_unit_id(unit_id, STATUS_LSB_INDEX(STATUS_ID_POISON), 1, 1);
        g_unit_status_staging_data->inflict_status_mode[battle_unit_index] = UNIT_INFLICT_STATUS_MODE_POISON;
        g_thread_call_target = (void (*)(void))unit_update_graphics_by_runtime_id;
        thread_call_on_main_stack(runtime_id);
        unit_set_animation(runtime_id, ANIMATION_KNEEL);
    } else if (mode == EVENT_INFLICT_STATUS_CRYSTAL) {
        status_inflict_by_event_unit_id(unit_id, STATUS_LSB_INDEX(STATUS_ID_DEAD), 0, 1);
        status_inflict_by_event_unit_id(unit_id, STATUS_LSB_INDEX(STATUS_ID_CRITICAL), 0, 1);
        status_inflict_by_event_unit_id(unit_id, STATUS_LSB_INDEX(STATUS_ID_CRYSTAL), 1, 1);
        g_unit_status_staging_data->inflict_status_mode[battle_unit_index] = UNIT_INFLICT_STATUS_MODE_CRYSTAL;
        g_thread_call_target = (void (*)(void))unit_update_graphics_by_runtime_id;
        thread_call_on_main_stack(runtime_id);
    } else if (mode == EVENT_INFLICT_STATUS_REVIVE) {
        if (health == 0) {
            g_unit_status_staging_data->inflict_status_mode[battle_unit_index] = UNIT_INFLICT_STATUS_MODE_REVIVE;
            status_inflict_by_event_unit_id(unit_id, STATUS_LSB_INDEX(STATUS_ID_DEAD), 0, 1);
            unit->hp = 1;
            status_inflict_by_event_unit_id(unit_id, STATUS_LSB_INDEX(STATUS_ID_CRITICAL), 1, 1);
            g_thread_call_target = (void (*)(void))unit_update_graphics_by_runtime_id;
            thread_call_on_main_stack(runtime_id);
            g_sound_pending_sfx_id = SOUND_SFX_EVENT_REVIVE;
        } else if (health == 2) {
            g_thread_call_target = (void (*)(void))animation_set_mounted_by_runtime_id;
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
    s32 idx = unit_get_battle_id_by_event_unit_id(event_unit_id);
    unit_stats_t* unit;
    s32 i;
    s32 byte;
    s32 bit;

    if (idx == EVENT_BATTLE_ID_NONE) {
        return;
    }
    unit = unit_get_stats(idx);
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
    g_thread_call_target = (void (*)(void))status_resolve_unit_changes;
    thread_call_on_main_stack(idx, removal_only);
}

/* ChangeStats event instruction: add a signed delta to one stat of each
 * resolved unit.
 *
 * Parameters: 16-bit unit id, stat selector (event_change_stats_e), then a
 * 16-bit delta. HP clamps to max HP and the other stats to 0-100. */
void script_opcode_83_change_stats(const event_change_stats_parameters_t* parameters) {
    u16 unit_id;
    s32 filter_mode;
    s32 unit_index;
    s32 battle_id;
    s32 value;
    s16 delta = script_read_s16(parameters->delta_le);
    s32 battle_unit_index;
    unit_stats_t* unit;

    unit_id = script_read_s16(parameters->unit_id_le);
    if (script_prepare_unit_filter(&unit_id, &filter_mode) != 0) {
        for (unit_index = 0; unit_index < UNIT_SLOT_COUNT; unit_index++) {
            if (script_filter_unit_id_by_mode(&unit_id, (u16*)&unit_index, &filter_mode) != 0) {
                battle_unit_index = unit_get_battle_id_by_runtime_id(unit_id);
                if (battle_unit_index != -1) {
                    unit = unit_entd_find_stats(unit_get_stats(battle_unit_index)->unit_id, &battle_id);
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
 * 1 flip). Units are selected as script_prepare_unit_filter describes.
 */
void script_opcode_11_unit_anim(const event_unit_anim_parameters_t* parameters) {
    u16 unit_id = script_read_s16(parameters->unit_id_le);
    s32 filter_mode;
    s32 unit_index;
    s32 animation;
    s32 move_flag_mode;

    if (script_prepare_unit_filter(&unit_id, &filter_mode) == 0) {
        return;
    }
    for (unit_index = 0; unit_index < UNIT_SLOT_COUNT; unit_index++) {
        if (script_filter_unit_id_by_mode(&unit_id, (u16*)&unit_index, &filter_mode) != 0) {
            move_flag_mode = parameters->facing_update_mode;
            if (move_flag_mode == 0) {
                unit_clear_facing_update_suppression(unit_id);
            } else if (move_flag_mode == 1) {
                unit_set_facing_update_suppression(unit_id);
            }
            animation = script_read_s16(parameters->animation_le);
            unit_set_animation(unit_id, animation);
            if (animation >= ANIMATION_BOW_HIGH && animation <= ANIMATION_BOW_LOW) {
                script_play_effect_and_wait();
            }
            if (filter_mode == 0) {
                return;
            }
        }
    }
}

/*
 * Event instruction: put the addressed unit(s) into their status idle
 * animation and wait.  Parameters: 0x00 unit id (16-bit value), 0x02 wait length
 * in frames. Units are selected as script_prepare_unit_filter describes.
 */
void script_opcode_80_march(const event_march_parameters_t* parameters) {
    u16 unit_id = script_read_s16(parameters->unit_id_le);
    s32 filter_mode;
    s32 unit_index;

    if (script_prepare_unit_filter(&unit_id, &filter_mode) == 0) {
        return;
    }
    for (unit_index = 0; unit_index < UNIT_SLOT_COUNT; unit_index++) {
        if (script_filter_unit_id_by_mode(&unit_id, (u16*)&unit_index, &filter_mode) != 0) {
            animation_set_status_by_runtime_id(unit_id);
            thread_wait_frames(parameters->wait_frames);
            if (filter_mode == 0) {
                return;
            }
        }
    }
}

/*
 * Event instruction: set or clear movement-effect suppression on the addressed unit(s).
 * Parameters: 0x00 unit id (16-bit value), 0x02 nonzero to set, zero to clear.
 * Units are selected as script_prepare_unit_filter describes.
 */
void script_opcode_81_set_unit_movement_effect_suppression(const event_movement_effect_parameters_t* parameters) {
    u16 unit_id = script_read_s16(parameters->unit_id_le);
    s32 filter_mode;
    s32 unit_index;

    if (script_prepare_unit_filter(&unit_id, &filter_mode) != 0) {
        for (unit_index = 0; unit_index < UNIT_SLOT_COUNT; unit_index++) {
            if (script_filter_unit_id_by_mode(&unit_id, (u16*)&unit_index, &filter_mode) != 0) {
                if (parameters->suppressed != 0) {
                    unit_set_movement_effect_suppression(unit_id);
                } else {
                    unit_clear_movement_effect_suppression(unit_id);
                }
                if (filter_mode == 0) {
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
void script_opcode_32_color_unit(const event_color_unit_parameters_t* parameters) {
    const event_unit_color_t* color;
    u16 unit_id = script_read_s16(parameters->unit_id_le);
    s32 filter_mode;
    s32 i;

    if (script_prepare_unit_filter(&unit_id, &filter_mode) != 0) {
        color = &parameters->color;
        for (i = 0; i < UNIT_SLOT_COUNT; i++) {
            if (script_filter_unit_id_by_mode(&unit_id, (u16*)&i, &filter_mode) != 0) {
                sprite_palette_apply_modulation_to_units(
                    color->mode, color->duration, unit_id, color->rgb.r, color->rgb.g, color->rgb.b);
                if (filter_mode == 0) {
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
void script_set_units_palette_update_suppression(s32 event_unit_id, s32 enable) {
    u16 unit_id = event_unit_id;
    s32 filter_mode;
    s32 i;

    if (script_prepare_unit_filter(&unit_id, &filter_mode) != 0) {
        for (i = 0; i < UNIT_SLOT_COUNT; i++) {
            if (script_filter_unit_id_by_mode(&unit_id, (u16*)&i, &filter_mode) != 0) {
                if (enable == 0) {
                    unit_clear_palette_update_suppression(unit_id);
                } else {
                    unit_set_palette_update_suppression(unit_id);
                }
                if (filter_mode == 0) {
                    return;
                }
            }
        }
    }
}

/* Nonzero when the unit's facing, relative to the camera yaw quadrant, is
 * one of the six "away from camera" directions 7..12 (of 16). */
s32 unit_check_facing_relative_to_camera(s32 event_unit_id) {
    s32 runtime_id = script_resolve_runtime_id(event_unit_id);
    s32 camera_facing = camera_wrap_yaw_angle();

    camera_facing += 0x200;
    camera_facing &= 0xf00;
    /* Preserve the retail arithmetic shift after the masked camera angle. */
    camera_facing >>= 8;
    camera_facing += unit_facing_get_sixteenth_by_runtime_id(runtime_id);
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
void script_opcode_69_face_tile(const event_face_tile_parameters_t* parameters) {
    SVECTOR screen;
    SVECTOR tile;
    u16 unit_id;
    s32 filter_mode;
    s32 i;
    unit_animation_state_t* rotation_state;
    s32 order = 1;
    s32 facing;
    s32 dx;
    s32 dy;

    tile.vx = parameters->x * BATTLE_MAP_TILE_SIZE + (BATTLE_MAP_TILE_SIZE / 2);
    tile.vz = parameters->y * BATTLE_MAP_TILE_SIZE + (BATTLE_MAP_TILE_SIZE / 2);
    unit_id = script_read_s16(parameters->unit_id_le);
    if (script_prepare_unit_filter(&unit_id, &filter_mode) == 0) {
        return;
    }
    for (i = 0; i < UNIT_SLOT_COUNT; i++) {
        if (script_filter_unit_id_by_mode(&unit_id, &g_script_unit_face_slot_order[i], &filter_mode) != 0) {
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
            if (filter_mode == 0) {
                return;
            }
        }
    }
}

/* WaitRotateUnit/All: resolved units yield at least once; -1 waits for all 21 slots. */
void script_opcode_65_wait_rotate_all(s32 unit_id) {
    s32 i;
    s32 index;

    if (unit_id == -1) {
        do {
            thread_yield();
            for (i = 0; i < UNIT_SLOT_COUNT; i++) {
                if (g_unit_animation_states[i].rotating != 0) {
                    break;
                }
            }
        } while (i != UNIT_SLOT_COUNT);
    } else {
        index = script_resolve_runtime_id(unit_id);
        if (index != EVENT_RUNTIME_ID_NONE) {
            do {
                thread_yield();
            } while (g_unit_animation_states[index].rotating != 0);
        }
    }
}
