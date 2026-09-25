#include "fft/battle.h"
#include "psx/gte.h"
#include "psx/types.h"

/*
 * Scenario thread task 7: interpolates the map light colour and its three
 * light vectors from their current values to six script-supplied targets over
 * the script's frame count, rebuilding the GTE light matrix each frame, then
 * commits the final values back to the live light state.
 */
void battle_map_light_thread(void) {
    s32 values[12];
    SVECTOR color;
    MATRIX rotation;
    MATRIX matrix;
    VECTOR translation;
    s32 step;
    s32 frames;
    const u8* script;
    s32* target;

    battle_thread_set_current_task_id(NATIVE_THREAD_TASK_MAP_LIGHT);
    step = 0;
    script = (const u8*)battle_thread_get_current_parameter_1();
    target = values;
    do {
        target[6] = battle_script_load_halfword(script);
        script += 2;
        step++;
        target++;
    } while (step < 6);
    frames = battle_script_load_halfword(script);
    step = 0;
    values[0] = g_battle_script_light_angles.vx;
    values[1] = g_battle_script_light_angles.vy;
    values[2] = g_battle_script_light_angles.vz;
    values[3] = g_battle_script_light_scale.vx;
    values[4] = g_battle_script_light_scale.vy;
    values[5] = g_battle_script_light_scale.vz;
    for (; step <= frames; step++) {
        color.vx = (values[6] - values[0]) * step / frames + values[0];
        color.vy = (values[7] - values[1]) * step / frames + values[1];
        color.vz = (values[8] - values[2]) * step / frames + values[2];
        RotMatrix(&color, &rotation);
        MulMatrix(&rotation, (MATRIX*)g_battle_script_light_base_direction_matrix);
        battle_script_copy_32_bytes(&matrix, g_battle_script_light_base_color_matrix);
        translation.vx = (values[9] - values[3]) * step / frames + values[3];
        translation.vy = (values[10] - values[4]) * step / frames + values[4];
        translation.vz = (values[11] - values[5]) * step / frames + values[5];
        ScaleMatrixL(&matrix, &translation);
        battle_map_light_matrix_command(0x5e, &matrix, &rotation, 0);
        battle_thread_wait_frames(1);
    }
    battle_copy_bytes(&g_battle_script_light_angles, &color, 8);
    battle_copy_bytes(&g_battle_script_light_scale, &translation, 0x10);
    battle_thread_exit_current();
}
