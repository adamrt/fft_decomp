#include "fft/battle_map.h"
#include "psx/libgpu.h"
#include "psx/types.h"

/* Textured polygon positions (map_append_mesh_geometry). polygon_flags holds
 * the polygon flags loaded by GNS resource 0x2c (map_dispatch_gns_resource). */

/*
 * Map polygon flag commands for the textured triangles and quads.
 *
 * 0x94 restores the ambient color g_map_ambient_polygon_color on
 * every polygon whose flag bit 15 is clear and records the render buffer it
 * wrote in g_map_ambient_restored_buffer; the map draw loop repeats the restore on the other
 * buffer and clears it. 0x46 copies flag bit 15 into bit 0 (after the flags
 * load and on the tile-color reset); 0x47 clears bit 0 everywhere and stops
 * the g_map_polygon_flag_clear_countdown countdown whose expiry issues it. The flag field is s16
 * (the bit 15 tests load it with `lh`).
 */
void map_polygon_flag_command(s32 command) {
    s32 i;

    switch (command) {
    case MAP_POLYGON_FLAG_COMMAND_RESTORE_AMBIENT:
        for (i = 0; i < g_map_textured_triangle_count; i++) {
            if (!(g_map_textured_triangle_positions[i].polygon_flags & MAP_POLYGON_FLAG_LIT)) {
                (g_battle_gfx_current_render_buffer->gt3 + i)->r0 = g_map_ambient_polygon_color.r;
                (g_battle_gfx_current_render_buffer->gt3 + i)->g0 = g_map_ambient_polygon_color.g;
                (g_battle_gfx_current_render_buffer->gt3 + i)->b0 = g_map_ambient_polygon_color.b;
                (g_battle_gfx_current_render_buffer->gt3 + i)->r1 = g_map_ambient_polygon_color.r;
                (g_battle_gfx_current_render_buffer->gt3 + i)->g1 = g_map_ambient_polygon_color.g;
                (g_battle_gfx_current_render_buffer->gt3 + i)->b1 = g_map_ambient_polygon_color.b;
                (g_battle_gfx_current_render_buffer->gt3 + i)->r2 = g_map_ambient_polygon_color.r;
                (g_battle_gfx_current_render_buffer->gt3 + i)->g2 = g_map_ambient_polygon_color.g;
                (g_battle_gfx_current_render_buffer->gt3 + i)->b2 = g_map_ambient_polygon_color.b;
            }
        }
        for (i = 0; i < g_map_textured_quad_count; i++) {
            if (!(g_map_textured_quad_positions[i].polygon_flags & MAP_POLYGON_FLAG_LIT)) {
                (g_battle_gfx_current_render_buffer->gt4 + i)->r0 = g_map_ambient_polygon_color.r;
                (g_battle_gfx_current_render_buffer->gt4 + i)->g0 = g_map_ambient_polygon_color.g;
                (g_battle_gfx_current_render_buffer->gt4 + i)->b0 = g_map_ambient_polygon_color.b;
                (g_battle_gfx_current_render_buffer->gt4 + i)->r1 = g_map_ambient_polygon_color.r;
                (g_battle_gfx_current_render_buffer->gt4 + i)->g1 = g_map_ambient_polygon_color.g;
                (g_battle_gfx_current_render_buffer->gt4 + i)->b1 = g_map_ambient_polygon_color.b;
                (g_battle_gfx_current_render_buffer->gt4 + i)->r2 = g_map_ambient_polygon_color.r;
                (g_battle_gfx_current_render_buffer->gt4 + i)->g2 = g_map_ambient_polygon_color.g;
                (g_battle_gfx_current_render_buffer->gt4 + i)->b2 = g_map_ambient_polygon_color.b;
                (g_battle_gfx_current_render_buffer->gt4 + i)->r3 = g_map_ambient_polygon_color.r;
                (g_battle_gfx_current_render_buffer->gt4 + i)->g3 = g_map_ambient_polygon_color.g;
                (g_battle_gfx_current_render_buffer->gt4 + i)->b3 = g_map_ambient_polygon_color.b;
            }
        }
        g_map_ambient_restored_buffer = g_battle_gfx_current_render_buffer;
        break;
    case MAP_POLYGON_FLAG_COMMAND_RESET_ACTIVE:
        for (i = 0; i < g_map_textured_triangle_count; i++) {
            if (g_map_textured_triangle_positions[i].polygon_flags & MAP_POLYGON_FLAG_LIT) {
                g_map_textured_triangle_positions[i].polygon_flags |= MAP_POLYGON_FLAG_LIT_ACTIVE;
            } else {
                g_map_textured_triangle_positions[i].polygon_flags &= ~MAP_POLYGON_FLAG_LIT_ACTIVE;
            }
        }
        for (i = 0; i < g_map_textured_quad_count; i++) {
            if (g_map_textured_quad_positions[i].polygon_flags & MAP_POLYGON_FLAG_LIT) {
                g_map_textured_quad_positions[i].polygon_flags |= MAP_POLYGON_FLAG_LIT_ACTIVE;
            } else {
                g_map_textured_quad_positions[i].polygon_flags &= ~MAP_POLYGON_FLAG_LIT_ACTIVE;
            }
        }
        break;
    case MAP_POLYGON_FLAG_COMMAND_CLEAR_ACTIVE:
        for (i = 0; i < g_map_textured_triangle_count; i++) {
            g_map_textured_triangle_positions[i].polygon_flags &= ~MAP_POLYGON_FLAG_LIT_ACTIVE;
        }
        for (i = 0; i < g_map_textured_quad_count; i++) {
            g_map_textured_quad_positions[i].polygon_flags &= ~MAP_POLYGON_FLAG_LIT_ACTIVE;
        }
        g_map_polygon_flag_clear_countdown = 0;
        break;
    }
}
