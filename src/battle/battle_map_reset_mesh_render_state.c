#include "fft/battle.h"
#include "fft/battle_gfx.h"
#include "fft/map.h"
#include "psx/gpu.h"
#include "psx/types.h"

/*
 * Reset the battle map's mesh, draw-mode and per-part render state before a
 * map load.
 *
 * Clears the four polygon counts and position spare halfwords, the nine mesh
 * part records (vector triples reset to zero/unit scale) and several map
 * tables. The weather flags keep only bits 0x7d.
 */
void battle_map_reset_mesh_render_state(void) {
    SVECTOR zero_vector;
    VECTOR unit_scale;
    s32 i;
    s32 j;
    s32* weather_flags;

    zero_vector = g_battle_map_zero_svector;
    unit_scale = g_battle_map_unit_scale_vector;
    g_battle_map_tile_width = 1;
    g_battle_map_tile_depth = 1;
    g_battle_target_color_tile_x = 1;
    g_battle_target_color_tile_y = 1;
    g_battle_map_untextured_otz_mask = 0;
    g_battle_map_untextured_fixed_ot_index = 4;
    g_battle_map_gns_dispatch_held = 0;
    for (i = 0; i < 150; i++) {
        for (j = 0; j < 14; j++) {
            D_800F70B4[i][j] = 0;
        }
        D_800F78E8[i] = 0;
    }
    for (i = 0; i < 49; i++) {
        bzero(g_battle_map_loaded_gns_records[i].load_info, 14);
        bzero(&g_battle_map_pending_gns_records[i].resource_type, 14);
    }
    battle_map_set_weather_texture_overlay(0x54);
    g_battle_map_lightning_mode = 0;
    weather_flags = &g_battle_map_weather_flags;
    *weather_flags &= 0x7d;
    SetDrawMode(&g_battle_render_buffers[0].overlay_modes[1], 0, 1, 0, 0);
    SetDrawMode(&g_battle_render_buffers[1].overlay_modes[1], 0, 1, 0, 0);
    SetDrawMode(&g_battle_render_buffers[0].overlay_modes[0], 0, 1, 0, 0);
    SetDrawMode(&g_battle_render_buffers[1].overlay_modes[0], 0, 1, 0, 0);
    g_battle_map_ambient_polygon_color[0] = 0x80;
    g_battle_map_ambient_polygon_color[1] = 0x80;
    g_battle_map_ambient_polygon_color[2] = 0x80;
    for (i = 0; i < 360; i++) {
        g_battle_map_textured_triangle_positions[i].polygon_flags = -0x7fff;
    }
    for (i = 0; i < 710; i++) {
        g_battle_map_textured_quad_positions[i].polygon_flags = -0x7fff;
    }
    for (i = 0; i < 64; i++) {
        g_battle_map_untextured_triangle_positions[i].polygon_flags = 0;
    }
    for (i = 0; i < 256; i++) {
        g_battle_map_untextured_quad_positions[i].polygon_flags = 0;
    }
    for (i = 0; i < 32; i++) {
        g_battle_map_texture_animations[i].mode = 0;
        g_battle_map_texture_animations[i].active = 0;
    }
    for (i = 0; i < 360; i++) {
        g_battle_map_textured_triangle_positions[i].terrain_tile.reset_value = -2;
    }
    for (i = 0; i < 710; i++) {
        g_battle_map_textured_quad_positions[i].terrain_tile.reset_value = -2;
    }
    for (i = 1; i < 9; i++) {
        g_battle_map_mesh_animation_instructions[i].states[0][1].next = 0;
        g_battle_map_mesh_parts[i].value_84 = 0x10;
        g_battle_map_mesh_parts[i].value_87 = 0;
        g_battle_map_mesh_part_animation_states[i] = 0;
    }
    g_battle_map_textured_triangle_count = 0;
    g_battle_map_textured_quad_count = 0;
    g_battle_map_untextured_triangle_count = 0;
    g_battle_map_untextured_quad_count = 0;
    for (i = 0; i < 9; i++) {
        g_battle_map_mesh_parts[i].counts[0] = 0;
        g_battle_map_mesh_parts[i].counts[1] = 0;
        g_battle_map_mesh_parts[i].counts[2] = 0;
        g_battle_map_mesh_parts[i].counts[3] = 0;
        g_battle_map_mesh_parts[i].vectors_a[1] = zero_vector;
        g_battle_map_mesh_parts[i].vectors_a[0] = zero_vector;
        g_battle_map_mesh_parts[i].vectors_a[2] = zero_vector;
        g_battle_map_mesh_parts[i].vectors_b[1] = zero_vector;
        g_battle_map_mesh_parts[i].vectors_b[0] = zero_vector;
        g_battle_map_mesh_parts[i].vectors_b[2] = zero_vector;
        g_battle_map_mesh_parts[i].vectors_c[1] = unit_scale;
        g_battle_map_mesh_parts[i].vectors_c[0] = unit_scale;
        g_battle_map_mesh_parts[i].vectors_c[2] = unit_scale;
        g_battle_map_mesh_parts[i].value_85 = 1;
        g_battle_map_mesh_part_animation_states[i] = 0;
    }
}
