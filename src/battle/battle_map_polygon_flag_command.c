#include "fft/battle.h"
#include "fft/battle_gfx.h"
#include "psx/gpu.h"
#include "psx/types.h"

/* Textured polygon positions (battle_map_append_mesh_geometry). _pad0e holds
 * the polygon flags loaded by GNS resource 0x2c (battle_map_dispatch_gns_resource). */
typedef struct mesh_triangle_positions_t {
    s16 x0, y0, z0;
    u16 terrain_tile;
    s16 x1, y1, z1;
    s16 _pad0e;
    s16 x2, y2, z2;
    u16 _pad16;
} mesh_triangle_positions_t;

typedef struct mesh_quad_positions_t {
    s16 x0, y0, z0;
    u16 terrain_tile;
    s16 x1, y1, z1;
    s16 _pad0e;
    s16 x2, y2, z2;
    u16 _pad16;
    s16 x3, y3, z3;
    u16 _pad1e;
} mesh_quad_positions_t;

extern mesh_triangle_positions_t g_battle_map_textured_triangle_positions[];
extern mesh_quad_positions_t g_battle_map_textured_quad_positions[];

/*
 * Map polygon flag commands for the textured triangles and quads.
 *
 * 0x94 restores the ambient colour g_battle_map_ambient_polygon_color on
 * every polygon whose flag bit 15 is clear and records the render buffer it
 * wrote in g_battle_map_ambient_restored_buffer; the map draw loop repeats the restore on the other
 * buffer and clears it. 0x46 copies flag bit 15 into bit 0 (after the flags
 * load and on the tile-colour reset); 0x47 clears bit 0 everywhere and stops
 * the g_battle_map_polygon_flag_clear_countdown countdown whose expiry issues it. The flag field is s16
 * (the bit 15 tests load it with `lh`).
 */
void battle_map_polygon_flag_command(s32 command) {
    s32 i;

    switch (command) {
    case 0x94:
        for (i = 0; i < g_battle_map_textured_triangle_count; i++) {
            if (!(g_battle_map_textured_triangle_positions[i]._pad0e & 0x8000)) {
                (g_battle_data->gt3 + i)->r0 = g_battle_map_ambient_polygon_color[0];
                (g_battle_data->gt3 + i)->g0 = g_battle_map_ambient_polygon_color[1];
                (g_battle_data->gt3 + i)->b0 = g_battle_map_ambient_polygon_color[2];
                (g_battle_data->gt3 + i)->r1 = g_battle_map_ambient_polygon_color[0];
                (g_battle_data->gt3 + i)->g1 = g_battle_map_ambient_polygon_color[1];
                (g_battle_data->gt3 + i)->b1 = g_battle_map_ambient_polygon_color[2];
                (g_battle_data->gt3 + i)->r2 = g_battle_map_ambient_polygon_color[0];
                (g_battle_data->gt3 + i)->g2 = g_battle_map_ambient_polygon_color[1];
                (g_battle_data->gt3 + i)->b2 = g_battle_map_ambient_polygon_color[2];
            }
        }
        for (i = 0; i < g_battle_map_textured_quad_count; i++) {
            if (!(g_battle_map_textured_quad_positions[i]._pad0e & 0x8000)) {
                (g_battle_data->gt4 + i)->r0 = g_battle_map_ambient_polygon_color[0];
                (g_battle_data->gt4 + i)->g0 = g_battle_map_ambient_polygon_color[1];
                (g_battle_data->gt4 + i)->b0 = g_battle_map_ambient_polygon_color[2];
                (g_battle_data->gt4 + i)->r1 = g_battle_map_ambient_polygon_color[0];
                (g_battle_data->gt4 + i)->g1 = g_battle_map_ambient_polygon_color[1];
                (g_battle_data->gt4 + i)->b1 = g_battle_map_ambient_polygon_color[2];
                (g_battle_data->gt4 + i)->r2 = g_battle_map_ambient_polygon_color[0];
                (g_battle_data->gt4 + i)->g2 = g_battle_map_ambient_polygon_color[1];
                (g_battle_data->gt4 + i)->b2 = g_battle_map_ambient_polygon_color[2];
                (g_battle_data->gt4 + i)->r3 = g_battle_map_ambient_polygon_color[0];
                (g_battle_data->gt4 + i)->g3 = g_battle_map_ambient_polygon_color[1];
                (g_battle_data->gt4 + i)->b3 = g_battle_map_ambient_polygon_color[2];
            }
        }
        g_battle_map_ambient_restored_buffer = g_battle_data;
        break;
    case 0x46:
        for (i = 0; i < g_battle_map_textured_triangle_count; i++) {
            if (g_battle_map_textured_triangle_positions[i]._pad0e & 0x8000) {
                g_battle_map_textured_triangle_positions[i]._pad0e |= 1;
            } else {
                g_battle_map_textured_triangle_positions[i]._pad0e &= ~1;
            }
        }
        for (i = 0; i < g_battle_map_textured_quad_count; i++) {
            if (g_battle_map_textured_quad_positions[i]._pad0e & 0x8000) {
                g_battle_map_textured_quad_positions[i]._pad0e |= 1;
            } else {
                g_battle_map_textured_quad_positions[i]._pad0e &= ~1;
            }
        }
        break;
    case 0x47:
        for (i = 0; i < g_battle_map_textured_triangle_count; i++) {
            g_battle_map_textured_triangle_positions[i]._pad0e &= ~1;
        }
        for (i = 0; i < g_battle_map_textured_quad_count; i++) {
            g_battle_map_textured_quad_positions[i]._pad0e &= ~1;
        }
        g_battle_map_polygon_flag_clear_countdown = 0;
        break;
    }
}
