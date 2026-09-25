/*
 * The event CAMERA instruction (0x19) thread. Instruction-for-instruction twin of the BATTLE routine at
 * 0x80146110; see that file for the interpolation modes and the
 * allocation-sensitive spellings.
 */
#include "fft/world.h"
#include "psx/types.h"

#define ABS(x) ((x) < 0 ? -(x) : (x))

/* Target operand meaning "keep the component's current value". */
#define WORLD_CAMERA_HOLD 10000

/* Script-variable index for each camera component, 0x801531b8. */

void world_camera_thread(void) {
    s32 target[8];
    s32 start[7];
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
    const u8* script;

    mode = g_world_camera_speed_curve & 3;
    group = (g_world_camera_speed_curve & 0xC) >> 2;
    ease = (g_world_camera_speed_curve & 0xF0) >> 4;
    world_thread_set_current_task_id(NATIVE_THREAD_TASK_CAMERA);
    script = (const u8*)world_thread_get_current_parameter_1();
    for (i = 0; i < 8; i++) {
        target[i] = world_script_load_halfword(script);
        script += 2;
    }

    if (g_world_camera_yaw_unwrap_request != 0) {
        g_world_camera_yaw_unwrap_request = 0;
        angle = world_script_get_variable(EVENT_SCRIPT_VAR_CAMERA_YAW);
        yaw_delta = ((angle << 16) >> 16) - target[4];
        yaw = angle;
        if (ABS(yaw_delta + ONE) < ABS(yaw_delta)) {
            yaw = angle + ONE;
        }
        world_script_set_variable(EVENT_SCRIPT_VAR_CAMERA_YAW, yaw);
    }

    for (i = 0; i < 7; i++) {
        start[i] = g_world_script_variables[g_world_camera_script_variable_indices[i]];
    }
    for (i = 0; i < 3; i++) {
        start[i] &= ~0xFFF;
        start[i] /= 4;
    }

    duration = target[7];
    for (i = 0; i < target[7]; i++) {
        weight = 16 - ease;
        ramp = (duration - i) * i;
        for (j = 0; j < 7; j++) {
            variable = &g_world_script_variables[g_world_camera_script_variable_indices[j]];
            if (target[j] == WORLD_CAMERA_HOLD) {
                continue;
            }
            if (*(u16*)&g_world_camera_position_locked != 0 && j < 3) {
                continue;
            }
            if (j < 3) {
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
            if (mode == 0 || (group == 0 && j >= 3) || (group == 1 && j < 3)) {
                value = world_mul_div_64(delta, i, duration) + base;
                if (j < 3) {
                    value *= 4;
                }
            } else if (mode == 1) {
                decel = delta * weight;
                accel = delta * ease * 2;
                first = world_mul_div_64(accel + decel + decel, i, duration * 32);
                value = world_mul_div_64(accel, ramp, duration * duration * 32) + first + base;
                if (j < 3) {
                    value *= 4;
                }
            } else {
                delta *= 2;
                half = i * 2;
                if (half < duration) {
                    decel = delta * weight;
                    value = world_mul_div_64(
                        world_mul_div_64(delta * ease * 2, half, duration) + decel + decel, half, duration * 32);
                    if (j < 3) {
                        value += base * 4;
                    } else {
                        value = value / 4 + base;
                    }
                } else {
                    decel = delta * weight;
                    accel = delta * ease * 2;
                    elapsed = half - duration;
                    value = world_mul_div_64(
                        accel + world_mul_div_64(accel, duration - elapsed, duration) + decel + decel, elapsed,
                        duration * 32);
                    if (j < 3) {
                        value += delta;
                        value += base * 4;
                    } else {
                        value = (value + delta) / 4 + base;
                    }
                }
            }
            if (j >= 3) {
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
        world_script_clear_current_event_word_bit_0();
        world_thread_yield();
    }

    if (*(u16*)&g_world_camera_position_locked == 0) {
        for (i = 0; i < 3; i++) {
            if (target[i] != WORLD_CAMERA_HOLD) {
                g_world_script_variables[g_world_camera_script_variable_indices[i]] = target[i] << 10;
            }
        }
    }
    for (i = 3; i < 7; i++) {
        if (target[i] != WORLD_CAMERA_HOLD) {
            g_world_script_variables[g_world_camera_script_variable_indices[i]] = target[i];
        }
    }
    world_script_clear_current_event_word_bit_0();
    world_thread_yield();
    world_thread_exit_current();
}
