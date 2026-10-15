#include "fft/battle.h"
#include "fft/battle_map.h"
#include "fft/battle_target.h"
#include "psx/libgpu.h"
#include "psx/types.h"

/*
 * Reset the battle map's mesh, draw-mode and per-part render state before a
 * map load.
 *
 * Clears the four polygon counts and position spare 16-bit values, the nine mesh
 * part records (vector triples reset to zero/unit scale) and several map
 * tables. The weather flags keep only bits 0x7d.
 */
void map_reset_mesh_render_state(void) {
    SVECTOR zero_vector;
    VECTOR unit_scale;
    s32 i;
    s16 no_tile;
    s32 j;
    s32* weather_flags;

    zero_vector = g_map_zero_svector;
    unit_scale = g_map_unit_scale_vector;
    g_map_tile_width = 1;
    g_map_tile_depth = 1;
    g_target_color_tile_x = 1;
    g_target_color_tile_y = 1;
    g_map_untextured_otz_mask = 0;
    g_map_untextured_fixed_ot_index = 4;
    g_map_gns_dispatch_held = 0;
    for (i = 0; i < 150; i++) {
        for (j = 0; j < 14; j++) {
            D_800F70B4[i][j] = 0;
        }
        D_800F78E8[i] = 0;
    }
    for (i = 0; i < GNS_RESOURCE_TABLE_COUNT; i++) {
        bzero(g_map_loaded_gns_records[i].load_info.bytes, sizeof(gns_load_info_t));
        bzero(&g_map_pending_gns_records[i].resource_type, 14);
    }
    map_run_weather_command(MAP_WEATHER_COMMAND_CLEAR);
    g_map_lightning_mode = MAP_LIGHTNING_OFF;
    weather_flags = &g_map_weather_flags;
    *weather_flags &= 0x7d;
    SetDrawMode(&g_battle_render_buffers[0].overlay_modes[1], 0, 1, 0, 0);
    SetDrawMode(&g_battle_render_buffers[1].overlay_modes[1], 0, 1, 0, 0);
    SetDrawMode(&g_battle_render_buffers[0].overlay_modes[0], 0, 1, 0, 0);
    SetDrawMode(&g_battle_render_buffers[1].overlay_modes[0], 0, 1, 0, 0);
    g_map_ambient_polygon_color.r = 0x80;
    g_map_ambient_polygon_color.g = 0x80;
    g_map_ambient_polygon_color.b = 0x80;
    for (i = 0; i < 360; i++) {
        g_map_textured_triangle_positions[i].polygon_flags = (s16)(MAP_POLYGON_FLAG_LIT | MAP_POLYGON_FLAG_LIT_ACTIVE);
    }
    for (i = 0; i < 710; i++) {
        g_map_textured_quad_positions[i].polygon_flags = (s16)(MAP_POLYGON_FLAG_LIT | MAP_POLYGON_FLAG_LIT_ACTIVE);
    }
    for (i = 0; i < 64; i++) {
        g_map_untextured_triangle_positions[i].polygon_flags = 0;
    }
    for (i = 0; i < 256; i++) {
        g_map_untextured_quad_positions[i].polygon_flags = 0;
    }
    for (i = 0; i < MAP_TEXTURE_ANIMATION_COUNT; i++) {
        g_map_texture_animations[i].mode = MAP_TEXTURE_ANIMATION_MODE_NONE;
        g_map_texture_animations[i].active = 0;
    }
    /* A signed sentinel local retains each array's original -2 constant load. */
    no_tile = MAP_POLYGON_TILE_NONE;
    for (i = 0; i < 360; i++) {
        g_map_textured_triangle_positions[i].terrain_tile = no_tile;
    }
    no_tile = MAP_POLYGON_TILE_NONE;
    for (i = 0; i < 710; i++) {
        g_map_textured_quad_positions[i].terrain_tile = no_tile;
    }
    for (i = 1; i < 9; i++) {
        g_map_mesh_animation_instructions[i].states[0][1].next = 0;
        g_map_mesh_parts[i].speed = 0x10;
        g_map_mesh_parts[i].keyframe = 0;
        g_map_mesh_part_animation_states[i] = 0;
    }
    g_map_textured_triangle_count = 0;
    g_map_textured_quad_count = 0;
    g_map_untextured_triangle_count = 0;
    g_map_untextured_quad_count = 0;
    for (i = 0; i < 9; i++) {
        g_map_mesh_parts[i].textured_triangle_count = 0;
        g_map_mesh_parts[i].textured_quad_count = 0;
        g_map_mesh_parts[i].untextured_triangle_count = 0;
        g_map_mesh_parts[i].untextured_quad_count = 0;
        g_map_mesh_parts[i].rotation = zero_vector;
        g_map_mesh_parts[i].rotation_start = zero_vector;
        g_map_mesh_parts[i].rotation_step = zero_vector;
        g_map_mesh_parts[i].translation = zero_vector;
        g_map_mesh_parts[i].translation_start = zero_vector;
        g_map_mesh_parts[i].translation_step = zero_vector;
        g_map_mesh_parts[i].scale = unit_scale;
        g_map_mesh_parts[i].scale_start = unit_scale;
        g_map_mesh_parts[i].scale_step = unit_scale;
        g_map_mesh_parts[i].next_instruction = 1;
        g_map_mesh_part_animation_states[i] = 0;
    }
}
