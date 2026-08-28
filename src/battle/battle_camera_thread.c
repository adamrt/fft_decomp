/*
 * The event CAMERA instruction (0x19) thread.
 * Instruction-for-instruction twin of world_camera_thread (0x800f9438).
 *
 * The script supplies seven camera-component targets and a frame count; 10000
 * keeps a component's current value. The packed speed-curve byte selects the
 * interpolation mode (bits 0-1), the component group left linear (bits 2-3)
 * and the easing weight (bits 4-7). Translation components (0-2) use a finer
 * fixed point than the rotation components.
 *
 * Allocation-sensitive spellings: ABS() as a conditional expression keeps the
 * target's copy-then-negate sequences; the explicit shift sign extension keeps
 * CSE from reusing a halfword copy of angle for yaw; accel is shared by the
 * mode 1 and late mode 2 paths while first and elapsed are block-local, which
 * gives the target's s0-s3 assignment.
 */
#include "fft/battle.h"
#include "fft/event.h"
#include "fft/script_variables.h"
#include "fft/thread.h"
#include "psx/types.h"

#define ABS(x) ((x) < 0 ? -(x) : (x))

/* Target operand meaning "keep the component's current value". */
#define BATTLE_CAMERA_HOLD 10000

/* Script-variable index for each camera component, 0x80165ed4. */

void battle_camera_thread(void) {
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

    mode = g_battle_camera_speed_curve & 3;
    group = (g_battle_camera_speed_curve & 0xC) >> 2;
    ease = (g_battle_camera_speed_curve & 0xF0) >> 4;
    battle_thread_set_current_task_id(NATIVE_THREAD_TASK_CAMERA);
    script = (const u8*)battle_thread_get_current_parameter_1();
    for (i = 0; i < 8; i++) {
        target[i] = battle_script_load_halfword(script);
        script += 2;
    }

    if (g_battle_camera_wrap_yaw_pending != 0) {
        g_battle_camera_wrap_yaw_pending = 0;
        angle = battle_script_get_variable(EVENT_SCRIPT_VAR_CAMERA_YAW);
        yaw_delta = ((angle << 16) >> 16) - target[4];
        yaw = angle;
        if (ABS(yaw_delta + ONE) < ABS(yaw_delta)) {
            yaw = angle + ONE;
        }
        battle_script_set_variable(EVENT_SCRIPT_VAR_CAMERA_YAW, yaw);
    }

    for (i = 0; i < 7; i++) {
        start[i] = g_battle_script_variables[g_battle_camera_script_variable_indices[i]];
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
            variable = &g_battle_script_variables[g_battle_camera_script_variable_indices[j]];
            if (target[j] == BATTLE_CAMERA_HOLD) {
                continue;
            }
            if (*(u16*)&g_battle_camera_position_locked != 0 && j < 3) {
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
                value = battle_mul_div_s64(delta, i, duration) + base;
                if (j < 3) {
                    value *= 4;
                }
            } else if (mode == 1) {
                decel = delta * weight;
                accel = delta * ease * 2;
                first = battle_mul_div_s64(accel + decel + decel, i, duration * 32);
                value = battle_mul_div_s64(accel, ramp, duration * duration * 32) + first + base;
                if (j < 3) {
                    value *= 4;
                }
            } else {
                delta *= 2;
                half = i * 2;
                if (half < duration) {
                    decel = delta * weight;
                    value = battle_mul_div_s64(
                        battle_mul_div_s64(delta * ease * 2, half, duration) + decel + decel, half, duration * 32);
                    if (j < 3) {
                        value += base * 4;
                    } else {
                        value = value / 4 + base;
                    }
                } else {
                    decel = delta * weight;
                    accel = delta * ease * 2;
                    elapsed = half - duration;
                    value = battle_mul_div_s64(
                        accel + battle_mul_div_s64(accel, duration - elapsed, duration) + decel + decel, elapsed,
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
        battle_script_clear_indexed_variable_bit0();
        battle_thread_yield();
    }

    if (*(u16*)&g_battle_camera_position_locked == 0) {
        for (i = 0; i < 3; i++) {
            if (target[i] != BATTLE_CAMERA_HOLD) {
                g_battle_script_variables[g_battle_camera_script_variable_indices[i]] = target[i] << 10;
            }
        }
    }
    for (i = 3; i < 7; i++) {
        if (target[i] != BATTLE_CAMERA_HOLD) {
            g_battle_script_variables[g_battle_camera_script_variable_indices[i]] = target[i];
        }
    }
    battle_script_clear_indexed_variable_bit0();
    battle_thread_yield();
    battle_thread_exit_current();
}
