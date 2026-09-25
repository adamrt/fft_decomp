#include "fft/battle.h"
#include "psx/gpu.h"
#include "psx/gte.h"
#include "psx/types.h"

/* Map-state command dispatcher.
 *
 * For event Use3DObject (0x80) `index` is the object id, `object_state` its
 * state row and `flag` is stored beside it; 0x9d reuses `object_state` as a
 * duration. */
s32 battle_map_dispatch_map_data_command(s32 command, u32 index, s32 object_state, s32 flag) {
    s32 result;
    s16 i;
    s32 mode;
    battle_map_mesh_instructions_t* entry;

    switch (command) {
    case MAP_DATA_COMMAND_GET_TEXTURE_ANIMATION_ACTIVE:
        result = g_battle_map_texture_animations[index].active;
        break;
    case MAP_DATA_COMMAND_START_TEXTURE_ANIMATION:
        if (g_battle_map_texture_animations[index].mode == 0) {
            if (g_battle_map_saved_texture_animation_modes[index] != 0) {
                g_battle_map_texture_animations[index].mode = g_battle_map_saved_texture_animation_modes[index];
            }
        }
        mode = g_battle_map_texture_animations[index].mode;
        /* Gotos keep the bound tests separate: a combined condition folds
         * 0x80..0x82 into one unsigned range compare. */
        if (mode == 0) {
            goto fail;
        }
        if (mode >= 0x1e) {
            if (mode >= 0x83) {
                goto fail;
            }
            if (mode < 0x80) {
                goto fail;
            }
        }
        g_battle_map_texture_animation_moves[index].x0 = g_battle_map_texture_animations[index].frame_x;
        g_battle_map_texture_animation_moves[index].y0 = g_battle_map_texture_animations[index].frame_y;
        g_battle_map_texture_animation_moves[index].w = g_battle_map_texture_animations[index].width;
        g_battle_map_texture_animation_moves[index].h = g_battle_map_texture_animations[index].height;
        g_battle_map_texture_animation_moves[index].x1 = g_battle_map_texture_animations[index].canvas_x;
        g_battle_map_texture_animation_moves[index].y1 = g_battle_map_texture_animations[index].canvas_y;
        g_battle_map_texture_animations[index].active = 1;
        g_battle_map_texture_animations[index].timer = 0x7e;
        g_battle_map_texture_animations[index].frame = 0;
        break;
    fail:
        main_system_handle_malloc_exception(2, 0x3cd);
        break;
    case MAP_DATA_COMMAND_GET_3D_OBJECT_STATE:
        result = g_battle_map_mesh_part_animation_states[index];
        break;
    case MAP_DATA_COMMAND_SET_3D_OBJECT_STATE:
        entry = &g_battle_map_mesh_animation_instructions[index];
        for (i = 0; i < 17; i++) {
            entry->states[0][i] = entry->states[object_state][i];
        }
        g_battle_map_mesh_parts[index].value_80 = 2;
        g_battle_map_mesh_parts[index].value_82 = 0;
        g_battle_map_mesh_parts[index].value_85 = 1;
        g_battle_map_mesh_part_animation_states[index] = (object_state << 8) | flag;
        break;
    case MAP_DATA_COMMAND_SPLIT_MESH_PARTS:
        for (i = 1; i < 9; i++) {
            g_battle_map_mesh_parts[i].counts[0] = (g_battle_map_primary_textured_triangle_count - 1) / 8 + 1;
            g_battle_map_mesh_parts[i].counts[1] = (g_battle_map_primary_textured_quad_count - 1) / 8 + 1;
            g_battle_map_mesh_parts[i].counts[2] = (g_battle_map_primary_untextured_triangle_count - 1) / 8 + 1;
            g_battle_map_mesh_parts[i].counts[3] = (g_battle_map_primary_untextured_quad_count - 1) / 8 + 1;
            g_battle_map_mesh_parts[i].starts[0]
                = ((g_battle_map_primary_textured_triangle_count - 1) / 8 + 1) * (i - 1);
            g_battle_map_mesh_parts[i].starts[1] = ((g_battle_map_primary_textured_quad_count - 1) / 8 + 1) * (i - 1);
            g_battle_map_mesh_parts[i].starts[2]
                = ((g_battle_map_primary_untextured_triangle_count - 1) / 8 + 1) * (i - 1);
            g_battle_map_mesh_parts[i].starts[3] = ((g_battle_map_primary_untextured_quad_count - 1) / 8 + 1) * (i - 1);
        }
        g_battle_map_mesh_parts[0].counts[0] = 0;
        g_battle_map_mesh_parts[0].counts[1] = 0;
        g_battle_map_mesh_parts[0].counts[2] = 0;
        g_battle_map_mesh_parts[0].counts[3] = 0;
        g_battle_map_mesh_parts[0].starts[0] = 0;
        g_battle_map_mesh_parts[0].starts[1] = 0;
        g_battle_map_mesh_parts[0].starts[2] = 0;
        g_battle_map_mesh_parts[0].starts[3] = 0;
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
        for (i = 0; i < 32; i++) {
            g_battle_map_saved_texture_animation_modes[i] = g_battle_map_texture_animations[i].mode;
            g_battle_map_texture_animations[i].mode = 0;
        }
        break;
    case MAP_DATA_COMMAND_RESTORE_ALL_TEXTURE_ANIMATIONS:
        for (i = 0; i < 32; i++) {
            g_battle_map_texture_animations[i].mode = g_battle_map_saved_texture_animation_modes[i];
        }
        break;
    case MAP_DATA_COMMAND_DISABLE_TEXTURE_ANIMATION:
        if (index < 32) {
            g_battle_map_saved_texture_animation_modes[index] = g_battle_map_texture_animations[index].mode;
            g_battle_map_texture_animations[index].mode = 0;
        }
        break;
    case MAP_DATA_COMMAND_RESTORE_TEXTURE_ANIMATION:
        if (index < 32) {
            g_battle_map_texture_animations[index].mode = g_battle_map_saved_texture_animation_modes[index];
        }
        break;
    case MAP_DATA_COMMAND_SET_TEXTURE_ANIMATION_DURATION:
        if (index < 32) {
            g_battle_map_texture_animations[index].duration = object_state;
            g_battle_map_texture_animations[index].timer = 0;
        }
        break;
    case MAP_DATA_COMMAND_RELEASE_GNS_HOLD:
        g_battle_map_gns_dispatch_held = 0;
        break;
    }
    return result;
}
