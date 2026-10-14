#include "fft/battle_map.h"
#include "fft/memory.h"
#include "fft/script.h"
#include "fft/thread.h"
#include "fft/world.h"
#include "psx/libgte.h"
#include "psx/types.h"

/* Thread task 7: interpolates the map light rotation (g_world_map_light_rotation) and scale
 * (g_world_map_light_scale) toward six 16-bit targets over the parameter's frame count,
 * rebuilding and submitting the light matrices once per frame, then stores
 * the final values back. */
void world_map_light_thread(void) {
    union {
        struct {
            s32 rotation[3];
            s32 scale[3];
        } vectors;
        s32 values[6];
    } start, target;
    SVECTOR rotation;
    MATRIX light;
    MATRIX color;
    VECTOR scale;
    const u8* params;
    s32 i;
    s32 frames;

    thread_set_current_task_id(THREAD_TASK_MAP_LIGHT);
    params = (const u8*)world_thread_get_current_parameter_1();
    /* Indexed operand fields retain a separate base and add four bytes. */
    for (i = 0; i < 6; i++) {
        target.values[i] = script_read_s16(params);
        params += 2;
    }
    /* The six-target scan leaves the cursor at duration_le. */
    frames = script_read_s16(params);
    start.vectors.rotation[0] = g_world_map_light_rotation.vx;
    start.vectors.rotation[1] = g_world_map_light_rotation.vy;
    start.vectors.rotation[2] = g_world_map_light_rotation.vz;
    start.vectors.scale[0] = g_world_map_light_scale.vx;
    start.vectors.scale[1] = g_world_map_light_scale.vy;
    start.vectors.scale[2] = g_world_map_light_scale.vz;
    for (i = 0; i <= frames; i++) {
        rotation.vx = (target.vectors.rotation[0] - start.vectors.rotation[0]) * i / frames + start.vectors.rotation[0];
        rotation.vy = (target.vectors.rotation[1] - start.vectors.rotation[1]) * i / frames + start.vectors.rotation[1];
        rotation.vz = (target.vectors.rotation[2] - start.vectors.rotation[2]) * i / frames + start.vectors.rotation[2];
        RotMatrix(&rotation, &light);
        MulMatrix(&light, &g_world_map_light_base_matrix);
        script_copy_matrix(&color, &g_world_map_light_color_matrix);
        scale.vx = (target.vectors.scale[0] - start.vectors.scale[0]) * i / frames + start.vectors.scale[0];
        scale.vy = (target.vectors.scale[1] - start.vectors.scale[1]) * i / frames + start.vectors.scale[1];
        scale.vz = (target.vectors.scale[2] - start.vectors.scale[2]) * i / frames + start.vectors.scale[2];
        ScaleMatrixL(&color, &scale);
        map_light_matrix_command(MAP_LIGHT_MATRIX_COMMAND_SET, &color, &light, 0);
        thread_wait_frames(1);
    }
    mem_copy_bytes_nonempty(&g_world_map_light_rotation, &rotation, sizeof(g_world_map_light_rotation));
    mem_copy_bytes_nonempty(&g_world_map_light_scale, &scale, sizeof(g_world_map_light_scale));
    thread_exit_current();
}
