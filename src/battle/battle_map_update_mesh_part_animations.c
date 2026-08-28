#include "fft/main_runtime.h"
#include "fft/map.h"
#include "psx/gte.h"
#include "psx/types.h"

void battle_map_update_mesh_part_animations(void) {
    s32 i;
    u8 next;
    u8 anim;
    s16 duration;
    s16 time;
    SVECTOR* current_b;
    SVECTOR* start_b;

    /* Copying vectors_b[1] through this invariant pointer to part 0, with the
     * destination taken first, matches the target: it hoists
     * &g_battle_map_mesh_parts[0].vectors_b[1] and bases the strength-reduced pointers on
     * it. */
    current_b = &g_battle_map_mesh_parts[0].vectors_b[1];

    for (i = 1; i < 9; i++) {
        if (g_battle_map_mesh_animation_instructions[i].states[0][1].next != 0) {
            g_battle_map_mesh_parts[i].value_80 += (g_battle_map_mesh_parts[i].value_84 >> 4) * g_animation_speed;
            if (g_battle_map_mesh_parts[i].value_82 < g_battle_map_mesh_parts[i].value_80) {
                next = g_battle_map_mesh_parts[i].value_85;
                g_battle_map_mesh_parts[i].value_87 = 0;
                if (g_battle_map_mesh_animation_instructions[i].states[0][next].duration == 0) {
                    g_battle_map_mesh_part_animation_states[i] = 0;
                }
            }
        }
        if (g_battle_map_mesh_part_animation_states[i] != 0) {
            if (g_battle_map_mesh_parts[i].value_87 == 0) {
                g_battle_map_mesh_parts[i].value_87
                    = g_battle_map_mesh_animation_instructions[i].states[0][next].keyframe;
                anim = g_battle_map_mesh_parts[i].value_87;
                g_battle_map_mesh_parts[i].value_85 = g_battle_map_mesh_animation_instructions[i].states[0][next].next;
                g_battle_map_mesh_parts[i].value_82
                    = g_battle_map_mesh_animation_instructions[i].states[0][next].duration;
                g_battle_map_mesh_parts[i].vectors_a[0] = g_battle_map_mesh_parts[i].vectors_a[1];
                g_battle_map_mesh_parts[i].vectors_c[0] = g_battle_map_mesh_parts[i].vectors_c[1];
                start_b = &g_battle_map_mesh_parts[i].vectors_b[0];
                *start_b = current_b[i * (sizeof(battle_map_mesh_part_t) / sizeof(SVECTOR))];
                g_battle_map_mesh_parts[i].value_80 = 1;
                if (g_battle_map_mesh_animation_keyframes[anim].flags[0] & 1) {
                    g_battle_map_mesh_parts[i].vectors_a[2].vx = g_battle_map_mesh_animation_keyframes[anim].vector_0.vx
                        - g_battle_map_mesh_parts[i].vectors_a[0].vx;
                }
                if (g_battle_map_mesh_animation_keyframes[anim].flags[1] & 1) {
                    g_battle_map_mesh_parts[i].vectors_a[2].vy = g_battle_map_mesh_animation_keyframes[anim].vector_0.vy
                        - g_battle_map_mesh_parts[i].vectors_a[0].vy;
                }
                if (g_battle_map_mesh_animation_keyframes[anim].flags[2] & 1) {
                    g_battle_map_mesh_parts[i].vectors_a[2].vz = g_battle_map_mesh_animation_keyframes[anim].vector_0.vz
                        - g_battle_map_mesh_parts[i].vectors_a[0].vz;
                }
                if (g_battle_map_mesh_animation_keyframes[anim].flags[3] & 1) {
                    g_battle_map_mesh_parts[i].vectors_b[2].vx = g_battle_map_mesh_animation_keyframes[anim].vector_8.vx
                        - g_battle_map_mesh_parts[i].vectors_b[0].vx;
                }
                if (g_battle_map_mesh_animation_keyframes[anim].flags[4] & 1) {
                    g_battle_map_mesh_parts[i].vectors_b[2].vy = g_battle_map_mesh_animation_keyframes[anim].vector_8.vy
                        - g_battle_map_mesh_parts[i].vectors_b[0].vy;
                }
                if (g_battle_map_mesh_animation_keyframes[anim].flags[5] & 1) {
                    g_battle_map_mesh_parts[i].vectors_b[2].vz = g_battle_map_mesh_animation_keyframes[anim].vector_8.vz
                        - g_battle_map_mesh_parts[i].vectors_b[0].vz;
                }
                if (g_battle_map_mesh_animation_keyframes[anim].flags[6] & 1) {
                    g_battle_map_mesh_parts[i].vectors_c[2].vx = g_battle_map_mesh_animation_keyframes[anim].value_10
                        - g_battle_map_mesh_parts[i].vectors_c[0].vx;
                }
                if (g_battle_map_mesh_animation_keyframes[anim].flags[7] & 1) {
                    g_battle_map_mesh_parts[i].vectors_c[2].vy = g_battle_map_mesh_animation_keyframes[anim].value_12
                        - g_battle_map_mesh_parts[i].vectors_c[0].vy;
                }
                if (g_battle_map_mesh_animation_keyframes[anim].flags[8] & 1) {
                    g_battle_map_mesh_parts[i].vectors_c[2].vz = g_battle_map_mesh_animation_keyframes[anim].value_14
                        - g_battle_map_mesh_parts[i].vectors_c[0].vz;
                }
                if (g_battle_map_mesh_animation_keyframes[anim].flags[0] & 2) {
                    g_battle_map_mesh_parts[i].vectors_a[2].vx
                        = g_battle_map_mesh_animation_keyframes[anim].vector_0.vx;
                }
                if (g_battle_map_mesh_animation_keyframes[anim].flags[1] & 2) {
                    g_battle_map_mesh_parts[i].vectors_a[2].vy
                        = g_battle_map_mesh_animation_keyframes[anim].vector_0.vy;
                }
                if (g_battle_map_mesh_animation_keyframes[anim].flags[2] & 2) {
                    g_battle_map_mesh_parts[i].vectors_a[2].vz
                        = g_battle_map_mesh_animation_keyframes[anim].vector_0.vz;
                }
                if (g_battle_map_mesh_animation_keyframes[anim].flags[3] & 2) {
                    g_battle_map_mesh_parts[i].vectors_b[2].vx
                        = g_battle_map_mesh_animation_keyframes[anim].vector_8.vx;
                }
                if (g_battle_map_mesh_animation_keyframes[anim].flags[4] & 2) {
                    g_battle_map_mesh_parts[i].vectors_b[2].vy
                        = g_battle_map_mesh_animation_keyframes[anim].vector_8.vy;
                }
                if (g_battle_map_mesh_animation_keyframes[anim].flags[5] & 2) {
                    g_battle_map_mesh_parts[i].vectors_b[2].vz
                        = g_battle_map_mesh_animation_keyframes[anim].vector_8.vz;
                }
                if (g_battle_map_mesh_animation_keyframes[anim].flags[6] & 2) {
                    g_battle_map_mesh_parts[i].vectors_c[2].vx = g_battle_map_mesh_animation_keyframes[anim].value_10;
                }
                if (g_battle_map_mesh_animation_keyframes[anim].flags[7] & 2) {
                    g_battle_map_mesh_parts[i].vectors_c[2].vy = g_battle_map_mesh_animation_keyframes[anim].value_12;
                }
                if (g_battle_map_mesh_animation_keyframes[anim].flags[8] & 2) {
                    g_battle_map_mesh_parts[i].vectors_c[2].vz = g_battle_map_mesh_animation_keyframes[anim].value_14;
                }
            }
            anim = g_battle_map_mesh_parts[i].value_87;
            duration = g_battle_map_mesh_parts[i].value_82;
            time = g_battle_map_mesh_parts[i].value_80;
            if (g_battle_map_mesh_animation_keyframes[anim].flags[0] & 4) {
                g_battle_map_mesh_parts[i].vectors_a[1].vx
                    = g_battle_map_mesh_parts[i].vectors_a[2].vx * time / duration;
            }
            if (g_battle_map_mesh_animation_keyframes[anim].flags[1] & 4) {
                g_battle_map_mesh_parts[i].vectors_a[1].vy
                    = g_battle_map_mesh_parts[i].vectors_a[2].vy * time / duration;
            }
            if (g_battle_map_mesh_animation_keyframes[anim].flags[2] & 4) {
                g_battle_map_mesh_parts[i].vectors_a[1].vz
                    = g_battle_map_mesh_parts[i].vectors_a[2].vz * time / duration;
            }
            if (g_battle_map_mesh_animation_keyframes[anim].flags[3] & 4) {
                g_battle_map_mesh_parts[i].vectors_b[1].vx
                    = g_battle_map_mesh_parts[i].vectors_b[2].vx * time / duration;
            }
            if (g_battle_map_mesh_animation_keyframes[anim].flags[4] & 4) {
                g_battle_map_mesh_parts[i].vectors_b[1].vy
                    = g_battle_map_mesh_parts[i].vectors_b[2].vy * time / duration;
            }
            if (g_battle_map_mesh_animation_keyframes[anim].flags[5] & 4) {
                g_battle_map_mesh_parts[i].vectors_b[1].vz
                    = g_battle_map_mesh_parts[i].vectors_b[2].vz * time / duration;
            }
            if (g_battle_map_mesh_animation_keyframes[anim].flags[6] & 4) {
                g_battle_map_mesh_parts[i].vectors_c[1].vx
                    = g_battle_map_mesh_parts[i].vectors_c[2].vx * time / duration;
            }
            if (g_battle_map_mesh_animation_keyframes[anim].flags[7] & 4) {
                g_battle_map_mesh_parts[i].vectors_c[1].vy
                    = g_battle_map_mesh_parts[i].vectors_c[2].vy * time / duration;
            }
            if (g_battle_map_mesh_animation_keyframes[anim].flags[8] & 4) {
                g_battle_map_mesh_parts[i].vectors_c[1].vz
                    = g_battle_map_mesh_parts[i].vectors_c[2].vz * time / duration;
            }
            if (g_battle_map_mesh_animation_keyframes[anim].flags[0] & 8) {
                g_battle_map_mesh_parts[i].vectors_a[1].vx = g_battle_map_mesh_parts[i].vectors_a[2].vx
                        * rsin((g_battle_map_mesh_animation_keyframes[anim].angle_end[0]
                                   - g_battle_map_mesh_animation_keyframes[anim].angle_start[0])
                                * time / duration
                            + g_battle_map_mesh_animation_keyframes[anim].angle_start[0])
                    >> 12;
            }
            if (g_battle_map_mesh_animation_keyframes[anim].flags[1] & 8) {
                g_battle_map_mesh_parts[i].vectors_a[1].vy = g_battle_map_mesh_parts[i].vectors_a[2].vy
                        * rsin((g_battle_map_mesh_animation_keyframes[anim].angle_end[1]
                                   - g_battle_map_mesh_animation_keyframes[anim].angle_start[1])
                                * time / duration
                            + g_battle_map_mesh_animation_keyframes[anim].angle_start[1])
                    >> 12;
            }
            if (g_battle_map_mesh_animation_keyframes[anim].flags[2] & 8) {
                g_battle_map_mesh_parts[i].vectors_a[1].vz = g_battle_map_mesh_parts[i].vectors_a[2].vz
                        * rsin((g_battle_map_mesh_animation_keyframes[anim].angle_end[2]
                                   - g_battle_map_mesh_animation_keyframes[anim].angle_start[2])
                                * time / duration
                            + g_battle_map_mesh_animation_keyframes[anim].angle_start[2])
                    >> 12;
            }
            if (g_battle_map_mesh_animation_keyframes[anim].flags[3] & 8) {
                g_battle_map_mesh_parts[i].vectors_b[1].vx = g_battle_map_mesh_parts[i].vectors_b[2].vx
                        * rsin((g_battle_map_mesh_animation_keyframes[anim].angle_end[3]
                                   - g_battle_map_mesh_animation_keyframes[anim].angle_start[3])
                                * time / duration
                            + g_battle_map_mesh_animation_keyframes[anim].angle_start[3])
                    >> 12;
            }
            if (g_battle_map_mesh_animation_keyframes[anim].flags[4] & 8) {
                g_battle_map_mesh_parts[i].vectors_b[1].vy = g_battle_map_mesh_parts[i].vectors_b[2].vy
                        * rsin((g_battle_map_mesh_animation_keyframes[anim].angle_end[4]
                                   - g_battle_map_mesh_animation_keyframes[anim].angle_start[4])
                                * time / duration
                            + g_battle_map_mesh_animation_keyframes[anim].angle_start[4])
                    >> 12;
            }
            if (g_battle_map_mesh_animation_keyframes[anim].flags[5] & 8) {
                g_battle_map_mesh_parts[i].vectors_b[1].vz = g_battle_map_mesh_parts[i].vectors_b[2].vz
                        * rsin((g_battle_map_mesh_animation_keyframes[anim].angle_end[5]
                                   - g_battle_map_mesh_animation_keyframes[anim].angle_start[5])
                                * time / duration
                            + g_battle_map_mesh_animation_keyframes[anim].angle_start[5])
                    >> 12;
            }
            if (g_battle_map_mesh_animation_keyframes[anim].flags[6] & 8) {
                g_battle_map_mesh_parts[i].vectors_c[1].vx = g_battle_map_mesh_parts[i].vectors_c[2].vx
                        * rsin((g_battle_map_mesh_animation_keyframes[anim].angle_end[6]
                                   - g_battle_map_mesh_animation_keyframes[anim].angle_start[6])
                                * time / duration
                            + g_battle_map_mesh_animation_keyframes[anim].angle_start[6])
                    >> 12;
            }
            if (g_battle_map_mesh_animation_keyframes[anim].flags[7] & 8) {
                g_battle_map_mesh_parts[i].vectors_c[1].vy = g_battle_map_mesh_parts[i].vectors_c[2].vy
                        * rsin((g_battle_map_mesh_animation_keyframes[anim].angle_end[7]
                                   - g_battle_map_mesh_animation_keyframes[anim].angle_start[7])
                                * time / duration
                            + g_battle_map_mesh_animation_keyframes[anim].angle_start[7])
                    >> 12;
            }
            if (g_battle_map_mesh_animation_keyframes[anim].flags[8] & 8) {
                g_battle_map_mesh_parts[i].vectors_c[1].vz = g_battle_map_mesh_parts[i].vectors_c[2].vz
                        * rsin((g_battle_map_mesh_animation_keyframes[anim].angle_end[8]
                                   - g_battle_map_mesh_animation_keyframes[anim].angle_start[8])
                                * time / duration
                            + g_battle_map_mesh_animation_keyframes[anim].angle_start[8])
                    >> 12;
            }
            if (g_battle_map_mesh_animation_keyframes[anim].flags[0] & 0x10) {
                g_battle_map_mesh_parts[i].vectors_a[1].vx = g_battle_map_mesh_parts[i].vectors_a[2].vx
                        * (rsin((g_battle_map_mesh_animation_keyframes[anim].angle_end[0]
                                    - g_battle_map_mesh_animation_keyframes[anim].angle_start[0])
                                   * time / duration
                               + g_battle_map_mesh_animation_keyframes[anim].angle_start[0])
                            + ONE)
                    >> 12;
            }
            if (g_battle_map_mesh_animation_keyframes[anim].flags[1] & 0x10) {
                g_battle_map_mesh_parts[i].vectors_a[1].vy = g_battle_map_mesh_parts[i].vectors_a[2].vy
                        * (rsin((g_battle_map_mesh_animation_keyframes[anim].angle_end[1]
                                    - g_battle_map_mesh_animation_keyframes[anim].angle_start[1])
                                   * time / duration
                               + g_battle_map_mesh_animation_keyframes[anim].angle_start[1])
                            + ONE)
                    >> 12;
            }
            if (g_battle_map_mesh_animation_keyframes[anim].flags[2] & 0x10) {
                g_battle_map_mesh_parts[i].vectors_a[1].vz = g_battle_map_mesh_parts[i].vectors_a[2].vz
                        * (rsin((g_battle_map_mesh_animation_keyframes[anim].angle_end[2]
                                    - g_battle_map_mesh_animation_keyframes[anim].angle_start[2])
                                   * time / duration
                               + g_battle_map_mesh_animation_keyframes[anim].angle_start[2])
                            + ONE)
                    >> 12;
            }
            if (g_battle_map_mesh_animation_keyframes[anim].flags[3] & 0x10) {
                g_battle_map_mesh_parts[i].vectors_b[1].vx = g_battle_map_mesh_parts[i].vectors_b[2].vx
                        * (rsin((g_battle_map_mesh_animation_keyframes[anim].angle_end[3]
                                    - g_battle_map_mesh_animation_keyframes[anim].angle_start[3])
                                   * time / duration
                               + g_battle_map_mesh_animation_keyframes[anim].angle_start[3])
                            + ONE)
                    >> 12;
            }
            if (g_battle_map_mesh_animation_keyframes[anim].flags[4] & 0x10) {
                g_battle_map_mesh_parts[i].vectors_b[1].vy = g_battle_map_mesh_parts[i].vectors_b[2].vy
                        * (rsin((g_battle_map_mesh_animation_keyframes[anim].angle_end[4]
                                    - g_battle_map_mesh_animation_keyframes[anim].angle_start[4])
                                   * time / duration
                               + g_battle_map_mesh_animation_keyframes[anim].angle_start[4])
                            + ONE)
                    >> 12;
            }
            if (g_battle_map_mesh_animation_keyframes[anim].flags[5] & 0x10) {
                g_battle_map_mesh_parts[i].vectors_b[1].vz = g_battle_map_mesh_parts[i].vectors_b[2].vz
                        * (rsin((g_battle_map_mesh_animation_keyframes[anim].angle_end[5]
                                    - g_battle_map_mesh_animation_keyframes[anim].angle_start[5])
                                   * time / duration
                               + g_battle_map_mesh_animation_keyframes[anim].angle_start[5])
                            + ONE)
                    >> 12;
            }
            if (g_battle_map_mesh_animation_keyframes[anim].flags[6] & 0x10) {
                g_battle_map_mesh_parts[i].vectors_c[1].vx = g_battle_map_mesh_parts[i].vectors_c[2].vx
                        * (rsin((g_battle_map_mesh_animation_keyframes[anim].angle_end[6]
                                    - g_battle_map_mesh_animation_keyframes[anim].angle_start[6])
                                   * time / duration
                               + g_battle_map_mesh_animation_keyframes[anim].angle_start[6])
                            + ONE)
                    >> 12;
            }
            if (g_battle_map_mesh_animation_keyframes[anim].flags[7] & 0x10) {
                g_battle_map_mesh_parts[i].vectors_c[1].vy = g_battle_map_mesh_parts[i].vectors_c[2].vy
                        * (rsin((g_battle_map_mesh_animation_keyframes[anim].angle_end[7]
                                    - g_battle_map_mesh_animation_keyframes[anim].angle_start[7])
                                   * time / duration
                               + g_battle_map_mesh_animation_keyframes[anim].angle_start[7])
                            + ONE)
                    >> 12;
            }
            if (g_battle_map_mesh_animation_keyframes[anim].flags[8] & 0x10) {
                g_battle_map_mesh_parts[i].vectors_c[1].vz = g_battle_map_mesh_parts[i].vectors_c[2].vz
                        * (rsin((g_battle_map_mesh_animation_keyframes[anim].angle_end[8]
                                    - g_battle_map_mesh_animation_keyframes[anim].angle_start[8])
                                   * time / duration
                               + g_battle_map_mesh_animation_keyframes[anim].angle_start[8])
                            + ONE)
                    >> 12;
            }
            g_battle_map_mesh_parts[i].vectors_a[1].vx += g_battle_map_mesh_parts[i].vectors_a[0].vx;
            g_battle_map_mesh_parts[i].vectors_a[1].vy += g_battle_map_mesh_parts[i].vectors_a[0].vy;
            g_battle_map_mesh_parts[i].vectors_a[1].vz += g_battle_map_mesh_parts[i].vectors_a[0].vz;
            g_battle_map_mesh_parts[i].vectors_b[1].vx += g_battle_map_mesh_parts[i].vectors_b[0].vx;
            g_battle_map_mesh_parts[i].vectors_b[1].vy += g_battle_map_mesh_parts[i].vectors_b[0].vy;
            g_battle_map_mesh_parts[i].vectors_b[1].vz += g_battle_map_mesh_parts[i].vectors_b[0].vz;
            g_battle_map_mesh_parts[i].vectors_c[1].vx += g_battle_map_mesh_parts[i].vectors_c[0].vx;
            g_battle_map_mesh_parts[i].vectors_c[1].vy += g_battle_map_mesh_parts[i].vectors_c[0].vy;
            g_battle_map_mesh_parts[i].vectors_c[1].vz += g_battle_map_mesh_parts[i].vectors_c[0].vz;
        }
    }
}
