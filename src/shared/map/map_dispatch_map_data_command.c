#include "fft/battle_map.h"
#include "psx/libgpu.h"
#include "psx/types.h"

/* Map-state command dispatcher.
 *
 * For event Use3DObject (0x80) `index` is the object id, `object_state` its
 * state row and `flag` is stored beside it; 0x9d reuses `object_state` as a
 * duration. */
s32 map_dispatch_map_data_command(map_data_command_e command, u32 index, s32 object_state, s32 flag) {
    s32 result;
    s16 i;
    map_mesh_instructions_t* entry;

    switch (command) {
    case MAP_DATA_COMMAND_GET_TEXTURE_ANIMATION_ACTIVE:
        result = g_map_texture_animations[index].active;
        break;
    case MAP_DATA_COMMAND_START_TEXTURE_ANIMATION:
        if (g_map_texture_animations[index].mode == MAP_TEXTURE_ANIMATION_MODE_NONE) {
            if (g_map_saved_texture_animation_modes[index] != 0) {
                g_map_texture_animations[index].mode = g_map_saved_texture_animation_modes[index];
            }
        }
        switch (g_map_texture_animations[index].mode) {
        case MAP_TEXTURE_ANIMATION_MODE_UV_LOOP ... MAP_TEXTURE_ANIMATION_MODE_COLOR_CYCLE:
        case MAP_TEXTURE_ANIMATION_MODE_UV_PING_PONG_BACK ... MAP_TEXTURE_ANIMATION_MODE_UV_REVERSE_ONCE_PLAYING:
            g_map_texture_animation_moves[index].sx = g_map_texture_animations[index].frame_x;
            g_map_texture_animation_moves[index].sy = g_map_texture_animations[index].frame_y;
            g_map_texture_animation_moves[index].w = g_map_texture_animations[index].width;
            g_map_texture_animation_moves[index].h = g_map_texture_animations[index].height;
            g_map_texture_animation_moves[index].x0 = g_map_texture_animations[index].canvas_x;
            g_map_texture_animation_moves[index].y0 = g_map_texture_animations[index].canvas_y;
            g_map_texture_animations[index].active = 1;
            g_map_texture_animations[index].timer = 0x7e;
            g_map_texture_animations[index].frame = 0;
            break;
        default:
            main_system_handle_malloc_exception(2, 0x3cd);
            break;
        }
        break;
    case MAP_DATA_COMMAND_GET_3D_OBJECT_STATE:
        result = g_map_mesh_part_animation_states[index];
        break;
    case MAP_DATA_COMMAND_SET_3D_OBJECT_STATE:
        entry = &g_map_mesh_animation_instructions[index];
        for (i = 0; i < 17; i++) {
            entry->states[0][i] = entry->states[object_state][i];
        }
        g_map_mesh_parts[index].elapsed = 2;
        g_map_mesh_parts[index].duration = 0;
        g_map_mesh_parts[index].next_instruction = 1;
        g_map_mesh_part_animation_states[index] = (object_state << 8) | flag;
        break;
    case MAP_DATA_COMMAND_SPLIT_MESH_PARTS:
        for (i = 1; i < 9; i++) {
            g_map_mesh_parts[i].textured_triangle_count = (g_map_primary_textured_triangle_count - 1) / 8 + 1;
            g_map_mesh_parts[i].textured_quad_count = (g_map_primary_textured_quad_count - 1) / 8 + 1;
            g_map_mesh_parts[i].untextured_triangle_count = (g_map_primary_untextured_triangle_count - 1) / 8 + 1;
            g_map_mesh_parts[i].untextured_quad_count = (g_map_primary_untextured_quad_count - 1) / 8 + 1;
            g_map_mesh_parts[i].textured_triangle_start
                = ((g_map_primary_textured_triangle_count - 1) / 8 + 1) * (i - 1);
            g_map_mesh_parts[i].textured_quad_start = ((g_map_primary_textured_quad_count - 1) / 8 + 1) * (i - 1);
            g_map_mesh_parts[i].untextured_triangle_start
                = ((g_map_primary_untextured_triangle_count - 1) / 8 + 1) * (i - 1);
            g_map_mesh_parts[i].untextured_quad_start = ((g_map_primary_untextured_quad_count - 1) / 8 + 1) * (i - 1);
        }
        g_map_mesh_parts[0].textured_triangle_count = 0;
        g_map_mesh_parts[0].textured_quad_count = 0;
        g_map_mesh_parts[0].untextured_triangle_count = 0;
        g_map_mesh_parts[0].untextured_quad_count = 0;
        g_map_mesh_parts[0].textured_triangle_start = 0;
        g_map_mesh_parts[0].textured_quad_start = 0;
        g_map_mesh_parts[0].untextured_triangle_start = 0;
        g_map_mesh_parts[0].untextured_quad_start = 0;
        break;
    case MAP_DATA_COMMAND_DISABLE_OVERLAY_DITHER:
        SetDrawMode(&g_battle_render_buffers[0].overlay_modes[0], 0, 0, 0, 0);
        SetDrawMode(&g_battle_render_buffers[1].overlay_modes[0], 0, 0, 0, 0);
        break;
    case MAP_DATA_COMMAND_ENABLE_OVERLAY_DITHER:
        SetDrawMode(&g_battle_render_buffers[0].overlay_modes[0], 0, 1, 0, 0);
        SetDrawMode(&g_battle_render_buffers[1].overlay_modes[0], 0, 1, 0, 0);
        break;
    case MAP_DATA_COMMAND_DISABLE_ALL_TEXTURE_ANIMATIONS:
        for (i = 0; i < MAP_TEXTURE_ANIMATION_COUNT; i++) {
            g_map_saved_texture_animation_modes[i] = g_map_texture_animations[i].mode;
            g_map_texture_animations[i].mode = MAP_TEXTURE_ANIMATION_MODE_NONE;
        }
        break;
    case MAP_DATA_COMMAND_RESTORE_ALL_TEXTURE_ANIMATIONS:
        for (i = 0; i < MAP_TEXTURE_ANIMATION_COUNT; i++) {
            g_map_texture_animations[i].mode = g_map_saved_texture_animation_modes[i];
        }
        break;
    case MAP_DATA_COMMAND_DISABLE_TEXTURE_ANIMATION:
        if (index < MAP_TEXTURE_ANIMATION_COUNT) {
            g_map_saved_texture_animation_modes[index] = g_map_texture_animations[index].mode;
            g_map_texture_animations[index].mode = MAP_TEXTURE_ANIMATION_MODE_NONE;
        }
        break;
    case MAP_DATA_COMMAND_RESTORE_TEXTURE_ANIMATION:
        if (index < MAP_TEXTURE_ANIMATION_COUNT) {
            g_map_texture_animations[index].mode = g_map_saved_texture_animation_modes[index];
        }
        break;
    case MAP_DATA_COMMAND_SET_TEXTURE_ANIMATION_DURATION:
        if (index < MAP_TEXTURE_ANIMATION_COUNT) {
            g_map_texture_animations[index].duration = object_state;
            g_map_texture_animations[index].timer = 0;
        }
        break;
    case MAP_DATA_COMMAND_RELEASE_GNS_HOLD:
        g_map_gns_dispatch_held = 0;
        break;
    }
    return result;
}
