#include "fft/battle.h"
#include "fft/map.h"
#include "fft/memory.h"
#include "fft/script.h"
#include "fft/thread.h"
#include "psx/libgte.h"
#include "psx/types.h"

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
