#include "fft/battle.h"
#include "psx/types.h"

/*
 * Map light matrix commands, selected on command - 0x5e: 0x5e loads both
 * matrices from the arguments and applies the colour matrix to the GTE, 0x5f
 * saves and 0x60 restores them, 0x61 and 0x62 return the colour and light
 * matrices. 0x66 and 0x68 start a transition from the current matrices over
 * duration frames and queue 0x67 or 0x69 in g_battle_map_light_transition_command, which the map draw
 * loop then issues once per frame: 0x67 interpolates toward target matrices,
 * 0x69 adds a frame-scaled delta.
 *
 * The translation rows add the start matrix element m[i][3] (the loop index
 * after the inner loop) rather than the start translation, as in the target.
 * Commands 0x5e-0x60 and 0x66-0x69 return the uninitialized result, like
 * battle_map_light_state_command.
 */
MATRIX* battle_map_light_matrix_command(s32 command, MATRIX* color, MATRIX* light, s32 duration) {
    MATRIX* result;
    s32 i;
    s32 j;

    switch (command) {
    case 0x5e:
        g_battle_map_light_color_matrix = *color;
        g_battle_map_light_matrix = *light;
        SetColorMatrix(&g_battle_map_light_color_matrix);
        break;
    case 0x5f:
        g_battle_map_saved_light_color_matrix = g_battle_map_light_color_matrix;
        g_battle_map_saved_light_matrix = g_battle_map_light_matrix;
        break;
    case 0x60:
        g_battle_map_light_color_matrix = g_battle_map_saved_light_color_matrix;
        g_battle_map_light_matrix = g_battle_map_saved_light_matrix;
        break;
    case 0x61:
        result = &g_battle_map_light_color_matrix;
        break;
    case 0x62:
        result = &g_battle_map_light_matrix;
        break;
    case 0x66:
        g_battle_map_light_transition_start_color_matrix = g_battle_map_light_color_matrix;
        g_battle_map_light_transition_start_light_matrix = g_battle_map_light_matrix;
        g_battle_map_light_transition_target_color_matrix = *color;
        g_battle_map_light_transition_target_light_matrix = *light;
        g_battle_map_light_transition_frame = 0;
        g_battle_map_light_transition_duration = duration;
        g_battle_map_light_transition_command = 0x67;
        break;
    case 0x67:
        for (i = 0; i < 3; i++) {
            for (j = 0; j < 3; j++) {
                g_battle_map_light_color_matrix.m[i][j] = g_battle_map_light_transition_start_color_matrix.m[i][j]
                    + (g_battle_map_light_transition_target_color_matrix.m[i][j]
                          - g_battle_map_light_transition_start_color_matrix.m[i][j])
                        * g_battle_map_light_transition_frame / g_battle_map_light_transition_duration;
                g_battle_map_light_matrix.m[i][j] = g_battle_map_light_transition_start_light_matrix.m[i][j]
                    + (g_battle_map_light_transition_target_light_matrix.m[i][j]
                          - g_battle_map_light_transition_start_light_matrix.m[i][j])
                        * g_battle_map_light_transition_frame / g_battle_map_light_transition_duration;
            }
            g_battle_map_light_color_matrix.t[i] = g_battle_map_light_transition_start_color_matrix.m[i][j]
                + (g_battle_map_light_transition_target_color_matrix.t[i]
                      - g_battle_map_light_transition_start_color_matrix.t[i])
                    * g_battle_map_light_transition_frame / g_battle_map_light_transition_duration;
            g_battle_map_light_matrix.t[i] = g_battle_map_light_transition_start_light_matrix.m[i][j]
                + (g_battle_map_light_transition_target_light_matrix.t[i]
                      - g_battle_map_light_transition_start_light_matrix.t[i])
                    * g_battle_map_light_transition_frame / g_battle_map_light_transition_duration;
        }
        SetColorMatrix(&g_battle_map_light_color_matrix);
        g_battle_map_light_transition_frame++;
        if (g_battle_map_light_transition_frame > g_battle_map_light_transition_duration) {
            g_battle_map_light_transition_command = 0;
        }
        break;
    case 0x68:
        g_battle_map_light_transition_start_color_matrix = g_battle_map_light_color_matrix;
        g_battle_map_light_transition_start_light_matrix = g_battle_map_light_matrix;
        g_battle_map_light_transition_target_color_matrix = *color;
        g_battle_map_light_transition_target_light_matrix = *light;
        g_battle_map_light_transition_frame = 0;
        g_battle_map_light_transition_duration = duration;
        g_battle_map_light_transition_command = 0x69;
        break;
    case 0x69:
        for (i = 0; i < 3; i++) {
            for (j = 0; j < 3; j++) {
                g_battle_map_light_color_matrix.m[i][j] = g_battle_map_light_transition_start_color_matrix.m[i][j]
                    + g_battle_map_light_transition_target_color_matrix.m[i][j] * g_battle_map_light_transition_frame
                        / g_battle_map_light_transition_duration;
                g_battle_map_light_matrix.m[i][j] = g_battle_map_light_transition_start_light_matrix.m[i][j]
                    + g_battle_map_light_transition_target_light_matrix.m[i][j] * g_battle_map_light_transition_frame
                        / g_battle_map_light_transition_duration;
            }
            g_battle_map_light_color_matrix.t[i] = g_battle_map_light_transition_start_color_matrix.m[i][j]
                + g_battle_map_light_transition_frame * g_battle_map_light_transition_target_color_matrix.t[i]
                    / g_battle_map_light_transition_duration;
            g_battle_map_light_matrix.t[i] = g_battle_map_light_transition_start_light_matrix.m[i][j]
                + g_battle_map_light_transition_frame * g_battle_map_light_transition_target_light_matrix.t[i]
                    / g_battle_map_light_transition_duration;
        }
        SetColorMatrix(&g_battle_map_light_color_matrix);
        g_battle_map_light_transition_frame++;
        if (g_battle_map_light_transition_frame > g_battle_map_light_transition_duration) {
            g_battle_map_light_transition_command = 0;
        }
        break;
    }
    return result;
}
