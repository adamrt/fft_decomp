#include "fft/battle.h"
#include "fft/world.h"
#include "psx/gte.h"
#include "psx/types.h"

/* Thread task 7: interpolates the map light rotation (g_world_map_light_rotation) and scale
 * (g_world_map_light_scale) toward six halfword targets over the parameter's frame count,
 * rebuilding and submitting the light matrices once per frame, then stores
 * the final values back. */
void world_map_light_thread(void) {
    s32 start[6];
    s32 target[6];
    SVECTOR rotation;
    MATRIX light;
    MATRIX colour;
    VECTOR scale;
    const u8* params;
    s32 i;
    s32 frames;

    world_thread_set_current_task_id(NATIVE_THREAD_TASK_MAP_LIGHT);
    params = world_thread_get_current_parameter_1();
    for (i = 0; i < 6; i++) {
        target[i] = world_script_load_halfword(params);
        params += 2;
    }
    frames = world_script_load_halfword(params);
    start[0] = g_world_map_light_rotation[0];
    start[1] = g_world_map_light_rotation[1];
    start[2] = g_world_map_light_rotation[2];
    start[3] = g_world_map_light_scale[0];
    start[4] = g_world_map_light_scale[1];
    start[5] = g_world_map_light_scale[2];
    for (i = 0; i <= frames; i++) {
        rotation.vx = (target[0] - start[0]) * i / frames + start[0];
        rotation.vy = (target[1] - start[1]) * i / frames + start[1];
        rotation.vz = (target[2] - start[2]) * i / frames + start[2];
        RotMatrix(&rotation, &light);
        MulMatrix(&light, &g_world_map_light_base_matrix);
        world_script_copy_32_bytes(&colour, &g_world_map_light_color_matrix);
        scale.vx = (target[3] - start[3]) * i / frames + start[3];
        scale.vy = (target[4] - start[4]) * i / frames + start[4];
        scale.vz = (target[5] - start[5]) * i / frames + start[5];
        ScaleMatrixL(&colour, &scale);
        battle_map_light_matrix_command(0x5E, &colour, &light, 0);
        world_thread_wait_frames(1);
    }
    world_script_copy_bytes(g_world_map_light_rotation, &rotation, 8);
    world_script_copy_bytes(g_world_map_light_scale, &scale, 16);
    world_thread_exit_current();
}
