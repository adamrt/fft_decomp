#include "fft/battle_map.h"
#include "psx/libgte.h"
#include "psx/types.h"

/* Advance the keyframe animations of map mesh parts 1-8 by one frame, from
 * map_draw_mesh_and_weather.
 *
 * A part whose running instruction row has a successor (states[0][1].next) adds
 * (speed >> 4) * g_main_system_animation_speed to its elapsed time. When that
 * passes the keyframe's duration the part asks for its next instruction; an
 * instruction of duration 0 ends the animation. For a new keyframe the current
 * rotation, translation and scale become the start values and each of the nine
 * channels takes its delta: target minus start for TWEEN_TO, the keyframe value
 * for TWEEN_BY. Each frame a channel is then start + delta * f, where f is
 * elapsed / duration (LINEAR), the sine of the angle interpolated from
 * angle_start to angle_end (SINE), or that sine plus 1.0 (RAISED_SINE).
 *
 * `next` is assigned only when a keyframe expires in the same pass. A part with a
 * nonzero animation state and keyframe 0 that did not expire indexes the
 * instructions with the value an earlier part left, or with an unset local.
 *
 * A channel with none of the three curve bits is not recomputed, yet its start
 * value is still added to it every frame. */
void map_update_mesh_part_animations(void) {
    s32 i;
    u8 next;
    u8 anim;
    s16 duration;
    s16 time;
    SVECTOR* current_b;
    SVECTOR* start_b;

    /* Copying translation through this invariant pointer to part 0, with the
     * destination taken first, matches the target: it hoists
     * &g_map_mesh_parts[0].translation and bases the strength-reduced pointers on
     * it. */
    current_b = &g_map_mesh_parts[0].translation;

    for (i = 1; i < 9; i++) {
        /* Advance the timer. A keyframe that has run its length asks for the
         * next instruction; one of duration 0 ends the animation. */
        if (g_map_mesh_animation_instructions[i].states[0][1].next != 0) {
            g_map_mesh_parts[i].elapsed += (g_map_mesh_parts[i].speed >> 4) * g_main_system_animation_speed;
            if (g_map_mesh_parts[i].duration < g_map_mesh_parts[i].elapsed) {
                next = g_map_mesh_parts[i].next_instruction;
                g_map_mesh_parts[i].keyframe = 0;
                if (g_map_mesh_animation_instructions[i].states[0][next].duration == 0) {
                    g_map_mesh_part_animation_states[i] = 0;
                }
            }
        }
        if (g_map_mesh_part_animation_states[i] != 0) {
            /* Start the next keyframe: the current values become the start
             * values and each channel takes its delta. */
            if (g_map_mesh_parts[i].keyframe == 0) {
                g_map_mesh_parts[i].keyframe = g_map_mesh_animation_instructions[i].states[0][next].keyframe;
                anim = g_map_mesh_parts[i].keyframe;
                g_map_mesh_parts[i].next_instruction = g_map_mesh_animation_instructions[i].states[0][next].next;
                g_map_mesh_parts[i].duration = g_map_mesh_animation_instructions[i].states[0][next].duration;
                g_map_mesh_parts[i].rotation_start = g_map_mesh_parts[i].rotation;
                g_map_mesh_parts[i].scale_start = g_map_mesh_parts[i].scale;
                start_b = &g_map_mesh_parts[i].translation_start;
                *start_b = current_b[i * (sizeof(map_mesh_part_t) / sizeof(SVECTOR))];
                g_map_mesh_parts[i].elapsed = 1;
                if (g_map_mesh_animation_keyframes[anim].tween_flags[0] & MAP_MESH_TWEEN_TO) {
                    g_map_mesh_parts[i].rotation_step.vx
                        = g_map_mesh_animation_keyframes[anim].rotation.vx - g_map_mesh_parts[i].rotation_start.vx;
                }
                if (g_map_mesh_animation_keyframes[anim].tween_flags[1] & MAP_MESH_TWEEN_TO) {
                    g_map_mesh_parts[i].rotation_step.vy
                        = g_map_mesh_animation_keyframes[anim].rotation.vy - g_map_mesh_parts[i].rotation_start.vy;
                }
                if (g_map_mesh_animation_keyframes[anim].tween_flags[2] & MAP_MESH_TWEEN_TO) {
                    g_map_mesh_parts[i].rotation_step.vz
                        = g_map_mesh_animation_keyframes[anim].rotation.vz - g_map_mesh_parts[i].rotation_start.vz;
                }
                if (g_map_mesh_animation_keyframes[anim].tween_flags[3] & MAP_MESH_TWEEN_TO) {
                    g_map_mesh_parts[i].translation_step.vx = g_map_mesh_animation_keyframes[anim].translation.vx
                        - g_map_mesh_parts[i].translation_start.vx;
                }
                if (g_map_mesh_animation_keyframes[anim].tween_flags[4] & MAP_MESH_TWEEN_TO) {
                    g_map_mesh_parts[i].translation_step.vy = g_map_mesh_animation_keyframes[anim].translation.vy
                        - g_map_mesh_parts[i].translation_start.vy;
                }
                if (g_map_mesh_animation_keyframes[anim].tween_flags[5] & MAP_MESH_TWEEN_TO) {
                    g_map_mesh_parts[i].translation_step.vz = g_map_mesh_animation_keyframes[anim].translation.vz
                        - g_map_mesh_parts[i].translation_start.vz;
                }
                if (g_map_mesh_animation_keyframes[anim].tween_flags[6] & MAP_MESH_TWEEN_TO) {
                    g_map_mesh_parts[i].scale_step.vx
                        = g_map_mesh_animation_keyframes[anim].scale_x - g_map_mesh_parts[i].scale_start.vx;
                }
                if (g_map_mesh_animation_keyframes[anim].tween_flags[7] & MAP_MESH_TWEEN_TO) {
                    g_map_mesh_parts[i].scale_step.vy
                        = g_map_mesh_animation_keyframes[anim].scale_y - g_map_mesh_parts[i].scale_start.vy;
                }
                if (g_map_mesh_animation_keyframes[anim].tween_flags[8] & MAP_MESH_TWEEN_TO) {
                    g_map_mesh_parts[i].scale_step.vz
                        = g_map_mesh_animation_keyframes[anim].scale_z - g_map_mesh_parts[i].scale_start.vz;
                }
                if (g_map_mesh_animation_keyframes[anim].tween_flags[0] & MAP_MESH_TWEEN_BY) {
                    g_map_mesh_parts[i].rotation_step.vx = g_map_mesh_animation_keyframes[anim].rotation.vx;
                }
                if (g_map_mesh_animation_keyframes[anim].tween_flags[1] & MAP_MESH_TWEEN_BY) {
                    g_map_mesh_parts[i].rotation_step.vy = g_map_mesh_animation_keyframes[anim].rotation.vy;
                }
                if (g_map_mesh_animation_keyframes[anim].tween_flags[2] & MAP_MESH_TWEEN_BY) {
                    g_map_mesh_parts[i].rotation_step.vz = g_map_mesh_animation_keyframes[anim].rotation.vz;
                }
                if (g_map_mesh_animation_keyframes[anim].tween_flags[3] & MAP_MESH_TWEEN_BY) {
                    g_map_mesh_parts[i].translation_step.vx = g_map_mesh_animation_keyframes[anim].translation.vx;
                }
                if (g_map_mesh_animation_keyframes[anim].tween_flags[4] & MAP_MESH_TWEEN_BY) {
                    g_map_mesh_parts[i].translation_step.vy = g_map_mesh_animation_keyframes[anim].translation.vy;
                }
                if (g_map_mesh_animation_keyframes[anim].tween_flags[5] & MAP_MESH_TWEEN_BY) {
                    g_map_mesh_parts[i].translation_step.vz = g_map_mesh_animation_keyframes[anim].translation.vz;
                }
                if (g_map_mesh_animation_keyframes[anim].tween_flags[6] & MAP_MESH_TWEEN_BY) {
                    g_map_mesh_parts[i].scale_step.vx = g_map_mesh_animation_keyframes[anim].scale_x;
                }
                if (g_map_mesh_animation_keyframes[anim].tween_flags[7] & MAP_MESH_TWEEN_BY) {
                    g_map_mesh_parts[i].scale_step.vy = g_map_mesh_animation_keyframes[anim].scale_y;
                }
                if (g_map_mesh_animation_keyframes[anim].tween_flags[8] & MAP_MESH_TWEEN_BY) {
                    g_map_mesh_parts[i].scale_step.vz = g_map_mesh_animation_keyframes[anim].scale_z;
                }
            }
            /* Evaluate each channel's delta at elapsed / duration on its
             * curve: linear, sine or raised sine. */
            anim = g_map_mesh_parts[i].keyframe;
            duration = g_map_mesh_parts[i].duration;
            time = g_map_mesh_parts[i].elapsed;
            if (g_map_mesh_animation_keyframes[anim].tween_flags[0] & MAP_MESH_TWEEN_LINEAR) {
                g_map_mesh_parts[i].rotation.vx = g_map_mesh_parts[i].rotation_step.vx * time / duration;
            }
            if (g_map_mesh_animation_keyframes[anim].tween_flags[1] & MAP_MESH_TWEEN_LINEAR) {
                g_map_mesh_parts[i].rotation.vy = g_map_mesh_parts[i].rotation_step.vy * time / duration;
            }
            if (g_map_mesh_animation_keyframes[anim].tween_flags[2] & MAP_MESH_TWEEN_LINEAR) {
                g_map_mesh_parts[i].rotation.vz = g_map_mesh_parts[i].rotation_step.vz * time / duration;
            }
            if (g_map_mesh_animation_keyframes[anim].tween_flags[3] & MAP_MESH_TWEEN_LINEAR) {
                g_map_mesh_parts[i].translation.vx = g_map_mesh_parts[i].translation_step.vx * time / duration;
            }
            if (g_map_mesh_animation_keyframes[anim].tween_flags[4] & MAP_MESH_TWEEN_LINEAR) {
                g_map_mesh_parts[i].translation.vy = g_map_mesh_parts[i].translation_step.vy * time / duration;
            }
            if (g_map_mesh_animation_keyframes[anim].tween_flags[5] & MAP_MESH_TWEEN_LINEAR) {
                g_map_mesh_parts[i].translation.vz = g_map_mesh_parts[i].translation_step.vz * time / duration;
            }
            if (g_map_mesh_animation_keyframes[anim].tween_flags[6] & MAP_MESH_TWEEN_LINEAR) {
                g_map_mesh_parts[i].scale.vx = g_map_mesh_parts[i].scale_step.vx * time / duration;
            }
            if (g_map_mesh_animation_keyframes[anim].tween_flags[7] & MAP_MESH_TWEEN_LINEAR) {
                g_map_mesh_parts[i].scale.vy = g_map_mesh_parts[i].scale_step.vy * time / duration;
            }
            if (g_map_mesh_animation_keyframes[anim].tween_flags[8] & MAP_MESH_TWEEN_LINEAR) {
                g_map_mesh_parts[i].scale.vz = g_map_mesh_parts[i].scale_step.vz * time / duration;
            }
            if (g_map_mesh_animation_keyframes[anim].tween_flags[0] & MAP_MESH_TWEEN_SINE) {
                g_map_mesh_parts[i].rotation.vx = g_map_mesh_parts[i].rotation_step.vx
                        * rsin((g_map_mesh_animation_keyframes[anim].angle_end[0]
                                   - g_map_mesh_animation_keyframes[anim].angle_start[0])
                                * time / duration
                            + g_map_mesh_animation_keyframes[anim].angle_start[0])
                    >> 12;
            }
            if (g_map_mesh_animation_keyframes[anim].tween_flags[1] & MAP_MESH_TWEEN_SINE) {
                g_map_mesh_parts[i].rotation.vy = g_map_mesh_parts[i].rotation_step.vy
                        * rsin((g_map_mesh_animation_keyframes[anim].angle_end[1]
                                   - g_map_mesh_animation_keyframes[anim].angle_start[1])
                                * time / duration
                            + g_map_mesh_animation_keyframes[anim].angle_start[1])
                    >> 12;
            }
            if (g_map_mesh_animation_keyframes[anim].tween_flags[2] & MAP_MESH_TWEEN_SINE) {
                g_map_mesh_parts[i].rotation.vz = g_map_mesh_parts[i].rotation_step.vz
                        * rsin((g_map_mesh_animation_keyframes[anim].angle_end[2]
                                   - g_map_mesh_animation_keyframes[anim].angle_start[2])
                                * time / duration
                            + g_map_mesh_animation_keyframes[anim].angle_start[2])
                    >> 12;
            }
            if (g_map_mesh_animation_keyframes[anim].tween_flags[3] & MAP_MESH_TWEEN_SINE) {
                g_map_mesh_parts[i].translation.vx = g_map_mesh_parts[i].translation_step.vx
                        * rsin((g_map_mesh_animation_keyframes[anim].angle_end[3]
                                   - g_map_mesh_animation_keyframes[anim].angle_start[3])
                                * time / duration
                            + g_map_mesh_animation_keyframes[anim].angle_start[3])
                    >> 12;
            }
            if (g_map_mesh_animation_keyframes[anim].tween_flags[4] & MAP_MESH_TWEEN_SINE) {
                g_map_mesh_parts[i].translation.vy = g_map_mesh_parts[i].translation_step.vy
                        * rsin((g_map_mesh_animation_keyframes[anim].angle_end[4]
                                   - g_map_mesh_animation_keyframes[anim].angle_start[4])
                                * time / duration
                            + g_map_mesh_animation_keyframes[anim].angle_start[4])
                    >> 12;
            }
            if (g_map_mesh_animation_keyframes[anim].tween_flags[5] & MAP_MESH_TWEEN_SINE) {
                g_map_mesh_parts[i].translation.vz = g_map_mesh_parts[i].translation_step.vz
                        * rsin((g_map_mesh_animation_keyframes[anim].angle_end[5]
                                   - g_map_mesh_animation_keyframes[anim].angle_start[5])
                                * time / duration
                            + g_map_mesh_animation_keyframes[anim].angle_start[5])
                    >> 12;
            }
            if (g_map_mesh_animation_keyframes[anim].tween_flags[6] & MAP_MESH_TWEEN_SINE) {
                g_map_mesh_parts[i].scale.vx = g_map_mesh_parts[i].scale_step.vx
                        * rsin((g_map_mesh_animation_keyframes[anim].angle_end[6]
                                   - g_map_mesh_animation_keyframes[anim].angle_start[6])
                                * time / duration
                            + g_map_mesh_animation_keyframes[anim].angle_start[6])
                    >> 12;
            }
            if (g_map_mesh_animation_keyframes[anim].tween_flags[7] & MAP_MESH_TWEEN_SINE) {
                g_map_mesh_parts[i].scale.vy = g_map_mesh_parts[i].scale_step.vy
                        * rsin((g_map_mesh_animation_keyframes[anim].angle_end[7]
                                   - g_map_mesh_animation_keyframes[anim].angle_start[7])
                                * time / duration
                            + g_map_mesh_animation_keyframes[anim].angle_start[7])
                    >> 12;
            }
            if (g_map_mesh_animation_keyframes[anim].tween_flags[8] & MAP_MESH_TWEEN_SINE) {
                g_map_mesh_parts[i].scale.vz = g_map_mesh_parts[i].scale_step.vz
                        * rsin((g_map_mesh_animation_keyframes[anim].angle_end[8]
                                   - g_map_mesh_animation_keyframes[anim].angle_start[8])
                                * time / duration
                            + g_map_mesh_animation_keyframes[anim].angle_start[8])
                    >> 12;
            }
            if (g_map_mesh_animation_keyframes[anim].tween_flags[0] & MAP_MESH_TWEEN_RAISED_SINE) {
                g_map_mesh_parts[i].rotation.vx = g_map_mesh_parts[i].rotation_step.vx
                        * (rsin((g_map_mesh_animation_keyframes[anim].angle_end[0]
                                    - g_map_mesh_animation_keyframes[anim].angle_start[0])
                                   * time / duration
                               + g_map_mesh_animation_keyframes[anim].angle_start[0])
                            + ONE)
                    >> 12;
            }
            if (g_map_mesh_animation_keyframes[anim].tween_flags[1] & MAP_MESH_TWEEN_RAISED_SINE) {
                g_map_mesh_parts[i].rotation.vy = g_map_mesh_parts[i].rotation_step.vy
                        * (rsin((g_map_mesh_animation_keyframes[anim].angle_end[1]
                                    - g_map_mesh_animation_keyframes[anim].angle_start[1])
                                   * time / duration
                               + g_map_mesh_animation_keyframes[anim].angle_start[1])
                            + ONE)
                    >> 12;
            }
            if (g_map_mesh_animation_keyframes[anim].tween_flags[2] & MAP_MESH_TWEEN_RAISED_SINE) {
                g_map_mesh_parts[i].rotation.vz = g_map_mesh_parts[i].rotation_step.vz
                        * (rsin((g_map_mesh_animation_keyframes[anim].angle_end[2]
                                    - g_map_mesh_animation_keyframes[anim].angle_start[2])
                                   * time / duration
                               + g_map_mesh_animation_keyframes[anim].angle_start[2])
                            + ONE)
                    >> 12;
            }
            if (g_map_mesh_animation_keyframes[anim].tween_flags[3] & MAP_MESH_TWEEN_RAISED_SINE) {
                g_map_mesh_parts[i].translation.vx = g_map_mesh_parts[i].translation_step.vx
                        * (rsin((g_map_mesh_animation_keyframes[anim].angle_end[3]
                                    - g_map_mesh_animation_keyframes[anim].angle_start[3])
                                   * time / duration
                               + g_map_mesh_animation_keyframes[anim].angle_start[3])
                            + ONE)
                    >> 12;
            }
            if (g_map_mesh_animation_keyframes[anim].tween_flags[4] & MAP_MESH_TWEEN_RAISED_SINE) {
                g_map_mesh_parts[i].translation.vy = g_map_mesh_parts[i].translation_step.vy
                        * (rsin((g_map_mesh_animation_keyframes[anim].angle_end[4]
                                    - g_map_mesh_animation_keyframes[anim].angle_start[4])
                                   * time / duration
                               + g_map_mesh_animation_keyframes[anim].angle_start[4])
                            + ONE)
                    >> 12;
            }
            if (g_map_mesh_animation_keyframes[anim].tween_flags[5] & MAP_MESH_TWEEN_RAISED_SINE) {
                g_map_mesh_parts[i].translation.vz = g_map_mesh_parts[i].translation_step.vz
                        * (rsin((g_map_mesh_animation_keyframes[anim].angle_end[5]
                                    - g_map_mesh_animation_keyframes[anim].angle_start[5])
                                   * time / duration
                               + g_map_mesh_animation_keyframes[anim].angle_start[5])
                            + ONE)
                    >> 12;
            }
            if (g_map_mesh_animation_keyframes[anim].tween_flags[6] & MAP_MESH_TWEEN_RAISED_SINE) {
                g_map_mesh_parts[i].scale.vx = g_map_mesh_parts[i].scale_step.vx
                        * (rsin((g_map_mesh_animation_keyframes[anim].angle_end[6]
                                    - g_map_mesh_animation_keyframes[anim].angle_start[6])
                                   * time / duration
                               + g_map_mesh_animation_keyframes[anim].angle_start[6])
                            + ONE)
                    >> 12;
            }
            if (g_map_mesh_animation_keyframes[anim].tween_flags[7] & MAP_MESH_TWEEN_RAISED_SINE) {
                g_map_mesh_parts[i].scale.vy = g_map_mesh_parts[i].scale_step.vy
                        * (rsin((g_map_mesh_animation_keyframes[anim].angle_end[7]
                                    - g_map_mesh_animation_keyframes[anim].angle_start[7])
                                   * time / duration
                               + g_map_mesh_animation_keyframes[anim].angle_start[7])
                            + ONE)
                    >> 12;
            }
            if (g_map_mesh_animation_keyframes[anim].tween_flags[8] & MAP_MESH_TWEEN_RAISED_SINE) {
                g_map_mesh_parts[i].scale.vz = g_map_mesh_parts[i].scale_step.vz
                        * (rsin((g_map_mesh_animation_keyframes[anim].angle_end[8]
                                    - g_map_mesh_animation_keyframes[anim].angle_start[8])
                                   * time / duration
                               + g_map_mesh_animation_keyframes[anim].angle_start[8])
                            + ONE)
                    >> 12;
            }
            /* Offset every channel by its start value. */
            g_map_mesh_parts[i].rotation.vx += g_map_mesh_parts[i].rotation_start.vx;
            g_map_mesh_parts[i].rotation.vy += g_map_mesh_parts[i].rotation_start.vy;
            g_map_mesh_parts[i].rotation.vz += g_map_mesh_parts[i].rotation_start.vz;
            g_map_mesh_parts[i].translation.vx += g_map_mesh_parts[i].translation_start.vx;
            g_map_mesh_parts[i].translation.vy += g_map_mesh_parts[i].translation_start.vy;
            g_map_mesh_parts[i].translation.vz += g_map_mesh_parts[i].translation_start.vz;
            g_map_mesh_parts[i].scale.vx += g_map_mesh_parts[i].scale_start.vx;
            g_map_mesh_parts[i].scale.vy += g_map_mesh_parts[i].scale_start.vy;
            g_map_mesh_parts[i].scale.vz += g_map_mesh_parts[i].scale_start.vz;
        }
    }
}
