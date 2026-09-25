#include "fft/battle.h"
#include "fft/battle_gfx.h"
#include "fft/map.h"
#include "psx/gpu.h"
#include "psx/types.h"

/* Textured polygon positions (battle_map_append_mesh_geometry). terrain_tile
 * packs the tile's layer (bit 0), z (bits 1..7) and x (bits 8..15); 0xfffe
 * marks a polygon without a tile. */

/*
 * Recolour the textured map polygons that lie on marked tiles.
 *
 * Each textured triangle/quad whose packed terrain tile (layer bit 0, z bits
 * 1..7, x bits 8..15; 0xfffe for none) resolves to a map tile with the mode's
 * mark bit in ceiling_depth_and_marks is recoloured: modes 1/2/7 tint marks
 * 0x20/0x40/0x80 and select CLUT row 0x1e1, modes 5/3/8 restore CLUT row 0x1e0
 * and the ambient colour g_battle_map_ambient_polygon_color (keeping polygons whose flag bit 15 is
 * set), and mode 0 resets every CLUT row. g_battle_target_tile_mark_modes[0..2] record the state
 * of the three mark layers; when the other two are both clear, or in their
 * restored states, the tint modes also shift the unit palettes. Mode 1 is the
 * blue movement-range highlight.
 *
 * The CLUT and tint stores index the render buffer's polygon arrays; the
 * ambient colour stores use pointer arithmetic on the decayed array
 * (`g_battle_data->gt3 + i`) instead, which is what keeps the target's
 * `addu offset, base` operand order for them.
 *
 * Every caller also passes a second argument, which the routine never reads.
 */
void battle_target_set_tile_background_color(s32 mode, s32 unused) {
    s32 i;
    s16 other_state_a;
    s16 other_state_b;
    s16 tile;
    u8 marks;

    g_battle_map_tile_data_ptr = g_battle_map_tile_data;
    switch (mode) {
    case 0:
        for (i = 0; i < g_battle_map_mesh_parts[0].counts[0]; i++) {
            g_battle_data->gt3[i].clut = (g_battle_data->gt3[i].clut & 0x803f) | 0x7800;
        }
        for (i = 0; i < g_battle_map_mesh_parts[0].counts[1]; i++) {
            g_battle_data->gt4[i].clut = (g_battle_data->gt4[i].clut & 0x803f) | 0x7800;
        }
        g_battle_target_tile_mark_modes[0] = 5;
        g_battle_target_tile_mark_modes[1] = 3;
        g_battle_target_tile_mark_modes[2] = 8;
        battle_map_polygon_flag_command(0x46);
        battle_map_polygon_flag_command(0x94);
        g_battle_target_tile_color_buffer = g_battle_data;
        break;
    case 1:
        for (i = 0; i < g_battle_map_mesh_parts[0].counts[0]; i++) {
            if (g_battle_map_textured_triangle_positions[i].terrain_tile.packed != 0xfffe) {
                g_battle_target_color_tile_y
                    = (g_battle_map_textured_triangle_positions[i].terrain_tile.packed >> 1) & 0x7f;
                g_battle_target_color_tile_x = g_battle_map_textured_triangle_positions[i].terrain_tile.packed >> 8;
                tile = (s16)g_battle_target_color_tile_y * g_battle_map_tile_width + (s16)g_battle_target_color_tile_x;
                if (g_battle_map_textured_triangle_positions[i].terrain_tile.packed & 1) {
                    tile += 0x100;
                }
                if (g_battle_map_tile_data_ptr[tile].ceiling_depth_and_marks & 0x20) {
                    g_battle_data->gt3[i].clut = (g_battle_data->gt3[i].clut & 0x803f) | 0x7840;
                    g_battle_data->gt3[i].r0 = 0x20;
                    g_battle_data->gt3[i].r1 = 0x20;
                    g_battle_data->gt3[i].r2 = 0x20;
                    g_battle_data->gt3[i].g0 = 0x28;
                    g_battle_data->gt3[i].g1 = 0x28;
                    g_battle_data->gt3[i].g2 = 0x28;
                    g_battle_data->gt3[i].b0 = 0x60;
                    g_battle_data->gt3[i].b1 = 0x60;
                    g_battle_data->gt3[i].b2 = 0x60;
                    g_battle_map_textured_triangle_positions[i].polygon_flags &= 0xfffe;
                }
            }
        }
        for (i = 0; i < g_battle_map_mesh_parts[0].counts[1]; i++) {
            if (g_battle_map_textured_quad_positions[i].terrain_tile.packed != 0xfffe) {
                g_battle_target_color_tile_y
                    = (g_battle_map_textured_quad_positions[i].terrain_tile.packed >> 1) & 0x7f;
                g_battle_target_color_tile_x = g_battle_map_textured_quad_positions[i].terrain_tile.packed >> 8;
                tile = (s16)g_battle_target_color_tile_y * g_battle_map_tile_width + (s16)g_battle_target_color_tile_x;
                if (g_battle_map_textured_quad_positions[i].terrain_tile.packed & 1) {
                    tile += 0x100;
                }
                if (g_battle_map_tile_data_ptr[tile].ceiling_depth_and_marks & 0x20) {
                    g_battle_data->gt4[i].clut = (g_battle_data->gt4[i].clut & 0x803f) | 0x7840;
                    g_battle_data->gt4[i].r0 = 0x20;
                    g_battle_data->gt4[i].r1 = 0x20;
                    g_battle_data->gt4[i].r2 = 0x20;
                    g_battle_data->gt4[i].r3 = 0x20;
                    g_battle_data->gt4[i].g0 = 0x28;
                    g_battle_data->gt4[i].g1 = 0x28;
                    g_battle_data->gt4[i].g2 = 0x28;
                    g_battle_data->gt4[i].g3 = 0x28;
                    g_battle_data->gt4[i].b0 = 0x60;
                    g_battle_data->gt4[i].b1 = 0x60;
                    g_battle_data->gt4[i].b2 = 0x60;
                    g_battle_data->gt4[i].b3 = 0x60;
                    g_battle_map_textured_quad_positions[i].polygon_flags &= 0xfffe;
                }
            }
        }
        g_battle_target_tile_mark_modes[0] = 1;
        other_state_a = g_battle_target_tile_mark_modes[1];
        other_state_b = g_battle_target_tile_mark_modes[2];
        if ((other_state_a | other_state_b) == 0 || (other_state_a == 3 && other_state_b == 8)) {
            battle_map_modify_palette(9, 8, 1, 0, 1, 10, 10, 10);
        }
        break;
    case 5:
        for (i = 0; i < g_battle_map_mesh_parts[0].counts[0]; i++) {
            if (g_battle_map_textured_triangle_positions[i].terrain_tile.packed != 0xfffe) {
                g_battle_target_color_tile_y
                    = (g_battle_map_textured_triangle_positions[i].terrain_tile.packed >> 1) & 0x7f;
                g_battle_target_color_tile_x = g_battle_map_textured_triangle_positions[i].terrain_tile.packed >> 8;
                tile = (s16)g_battle_target_color_tile_y * g_battle_map_tile_width + (s16)g_battle_target_color_tile_x;
                if (g_battle_map_textured_triangle_positions[i].terrain_tile.packed & 1) {
                    tile += 0x100;
                }
                if (g_battle_map_tile_data_ptr[tile].ceiling_depth_and_marks & 0x20) {
                    g_battle_data->gt3[i].clut = (g_battle_data->gt3[i].clut & 0x803f) | 0x7800;
                    if (g_battle_map_textured_triangle_positions[i].polygon_flags & 0x8000) {
                        g_battle_map_textured_triangle_positions[i].polygon_flags |= 1;
                    } else {
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
            }
        }
        for (i = 0; i < g_battle_map_mesh_parts[0].counts[1]; i++) {
            if (g_battle_map_textured_quad_positions[i].terrain_tile.packed != 0xfffe) {
                g_battle_target_color_tile_y
                    = (g_battle_map_textured_quad_positions[i].terrain_tile.packed >> 1) & 0x7f;
                g_battle_target_color_tile_x = g_battle_map_textured_quad_positions[i].terrain_tile.packed >> 8;
                tile = (s16)g_battle_target_color_tile_y * g_battle_map_tile_width + (s16)g_battle_target_color_tile_x;
                if (g_battle_map_textured_quad_positions[i].terrain_tile.packed & 1) {
                    tile += 0x100;
                }
                if (g_battle_map_tile_data_ptr[tile].ceiling_depth_and_marks & 0x20) {
                    g_battle_data->gt4[i].clut = (g_battle_data->gt4[i].clut & 0x803f) | 0x7800;
                    if (g_battle_map_textured_quad_positions[i].polygon_flags & 0x8000) {
                        g_battle_map_textured_quad_positions[i].polygon_flags |= 1;
                    } else {
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
            }
        }
        g_battle_target_tile_mark_modes[0] = 5;
        g_battle_target_tile_color_buffer = g_battle_data;
        break;
    case 2:
        for (i = 0; i < g_battle_map_mesh_parts[0].counts[0]; i++) {
            if (g_battle_map_textured_triangle_positions[i].terrain_tile.packed != 0xfffe) {
                g_battle_target_color_tile_y
                    = (g_battle_map_textured_triangle_positions[i].terrain_tile.packed >> 1) & 0x7f;
                g_battle_target_color_tile_x = g_battle_map_textured_triangle_positions[i].terrain_tile.packed >> 8;
                tile = (s16)g_battle_target_color_tile_y * g_battle_map_tile_width + (s16)g_battle_target_color_tile_x;
                if (g_battle_map_textured_triangle_positions[i].terrain_tile.packed & 1) {
                    tile += 0x100;
                }
                if (g_battle_map_tile_data_ptr[tile].ceiling_depth_and_marks & 0x40) {
                    g_battle_data->gt3[i].clut = (g_battle_data->gt3[i].clut & 0x803f) | 0x7840;
                    g_battle_data->gt3[i].r0 = 0x60;
                    g_battle_data->gt3[i].r1 = 0x60;
                    g_battle_data->gt3[i].r2 = 0x60;
                    g_battle_data->gt3[i].g0 = 0x18;
                    g_battle_data->gt3[i].g1 = 0x18;
                    g_battle_data->gt3[i].g2 = 0x18;
                    g_battle_data->gt3[i].b0 = 0x20;
                    g_battle_data->gt3[i].b1 = 0x20;
                    g_battle_data->gt3[i].b2 = 0x20;
                    g_battle_map_textured_triangle_positions[i].polygon_flags &= 0xfffe;
                }
            }
        }
        for (i = 0; i < g_battle_map_mesh_parts[0].counts[1]; i++) {
            if (g_battle_map_textured_quad_positions[i].terrain_tile.packed != 0xfffe) {
                g_battle_target_color_tile_y
                    = (g_battle_map_textured_quad_positions[i].terrain_tile.packed >> 1) & 0x7f;
                g_battle_target_color_tile_x = g_battle_map_textured_quad_positions[i].terrain_tile.packed >> 8;
                tile = (s16)g_battle_target_color_tile_y * g_battle_map_tile_width + (s16)g_battle_target_color_tile_x;
                if (g_battle_map_textured_quad_positions[i].terrain_tile.packed & 1) {
                    tile += 0x100;
                }
                if (g_battle_map_tile_data_ptr[tile].ceiling_depth_and_marks & 0x40) {
                    g_battle_data->gt4[i].clut = (g_battle_data->gt4[i].clut & 0x803f) | 0x7840;
                    g_battle_data->gt4[i].r0 = 0x60;
                    g_battle_data->gt4[i].r1 = 0x60;
                    g_battle_data->gt4[i].r2 = 0x60;
                    g_battle_data->gt4[i].r3 = 0x60;
                    g_battle_data->gt4[i].g0 = 0x18;
                    g_battle_data->gt4[i].g1 = 0x18;
                    g_battle_data->gt4[i].g2 = 0x18;
                    g_battle_data->gt4[i].g3 = 0x18;
                    g_battle_data->gt4[i].b0 = 0x20;
                    g_battle_data->gt4[i].b1 = 0x20;
                    g_battle_data->gt4[i].b2 = 0x20;
                    g_battle_data->gt4[i].b3 = 0x20;
                    g_battle_map_textured_quad_positions[i].polygon_flags &= 0xfffe;
                }
            }
        }
        g_battle_target_tile_mark_modes[1] = 2;
        other_state_a = g_battle_target_tile_mark_modes[0];
        other_state_b = g_battle_target_tile_mark_modes[2];
        if ((other_state_a | other_state_b) == 0 || (other_state_a == 5 && other_state_b == 8)) {
            battle_map_modify_palette(9, 8, 1, 0, 1, 10, 10, 10);
        }
        break;
    case 3:
        for (i = 0; i < g_battle_map_mesh_parts[0].counts[0]; i++) {
            if (g_battle_map_textured_triangle_positions[i].terrain_tile.packed != 0xfffe) {
                g_battle_target_color_tile_y
                    = (g_battle_map_textured_triangle_positions[i].terrain_tile.packed >> 1) & 0x7f;
                g_battle_target_color_tile_x = g_battle_map_textured_triangle_positions[i].terrain_tile.packed >> 8;
                tile = (s16)g_battle_target_color_tile_y * g_battle_map_tile_width + (s16)g_battle_target_color_tile_x;
                if (g_battle_map_textured_triangle_positions[i].terrain_tile.packed & 1) {
                    tile += 0x100;
                }
                if (g_battle_map_tile_data_ptr[tile].ceiling_depth_and_marks & 0x40) {
                    g_battle_data->gt3[i].clut = (g_battle_data->gt3[i].clut & 0x803f) | 0x7800;
                    if (g_battle_map_textured_triangle_positions[i].polygon_flags & 0x8000) {
                        g_battle_map_textured_triangle_positions[i].polygon_flags |= 1;
                    } else {
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
            }
        }
        for (i = 0; i < g_battle_map_mesh_parts[0].counts[1]; i++) {
            if (g_battle_map_textured_quad_positions[i].terrain_tile.packed != 0xfffe) {
                g_battle_target_color_tile_y
                    = (g_battle_map_textured_quad_positions[i].terrain_tile.packed >> 1) & 0x7f;
                g_battle_target_color_tile_x = g_battle_map_textured_quad_positions[i].terrain_tile.packed >> 8;
                tile = (s16)g_battle_target_color_tile_y * g_battle_map_tile_width + (s16)g_battle_target_color_tile_x;
                if (g_battle_map_textured_quad_positions[i].terrain_tile.packed & 1) {
                    tile += 0x100;
                }
                if (g_battle_map_tile_data_ptr[tile].ceiling_depth_and_marks & 0x40) {
                    g_battle_data->gt4[i].clut = (g_battle_data->gt4[i].clut & 0x803f) | 0x7800;
                    if (g_battle_map_textured_quad_positions[i].polygon_flags & 0x8000) {
                        g_battle_map_textured_quad_positions[i].polygon_flags |= 1;
                    } else {
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
            }
        }
        g_battle_target_tile_mark_modes[1] = 3;
        g_battle_target_tile_color_buffer = g_battle_data;
        break;
    case 7:
        for (i = 0; i < g_battle_map_mesh_parts[0].counts[0]; i++) {
            if (g_battle_map_textured_triangle_positions[i].terrain_tile.packed != 0xfffe) {
                g_battle_target_color_tile_y
                    = (g_battle_map_textured_triangle_positions[i].terrain_tile.packed >> 1) & 0x7f;
                g_battle_target_color_tile_x = g_battle_map_textured_triangle_positions[i].terrain_tile.packed >> 8;
                tile = (s16)g_battle_target_color_tile_y * g_battle_map_tile_width + (s16)g_battle_target_color_tile_x;
                if (g_battle_map_textured_triangle_positions[i].terrain_tile.packed & 1) {
                    tile += 0x100;
                }
                if (g_battle_map_tile_data_ptr[tile].ceiling_depth_and_marks & 0x80) {
                    g_battle_data->gt3[i].clut = (g_battle_data->gt3[i].clut & 0x803f) | 0x7840;
                    g_battle_data->gt3[i].r0 = 0x50;
                    g_battle_data->gt3[i].r1 = 0x50;
                    g_battle_data->gt3[i].r2 = 0x50;
                    g_battle_data->gt3[i].g0 = 0x60;
                    g_battle_data->gt3[i].g1 = 0x60;
                    g_battle_data->gt3[i].g2 = 0x60;
                    g_battle_data->gt3[i].b0 = 0x10;
                    g_battle_data->gt3[i].b1 = 0x10;
                    g_battle_data->gt3[i].b2 = 0x10;
                    g_battle_map_textured_triangle_positions[i].polygon_flags &= 0xfffe;
                }
            }
        }
        for (i = 0; i < g_battle_map_mesh_parts[0].counts[1]; i++) {
            if (g_battle_map_textured_quad_positions[i].terrain_tile.packed != 0xfffe) {
                g_battle_target_color_tile_y
                    = (g_battle_map_textured_quad_positions[i].terrain_tile.packed >> 1) & 0x7f;
                g_battle_target_color_tile_x = g_battle_map_textured_quad_positions[i].terrain_tile.packed >> 8;
                tile = (s16)g_battle_target_color_tile_y * g_battle_map_tile_width + (s16)g_battle_target_color_tile_x;
                if (g_battle_map_textured_quad_positions[i].terrain_tile.packed & 1) {
                    tile += 0x100;
                }
                if (g_battle_map_tile_data_ptr[tile].ceiling_depth_and_marks & 0x80) {
                    g_battle_data->gt4[i].clut = (g_battle_data->gt4[i].clut & 0x803f) | 0x7840;
                    g_battle_data->gt4[i].r0 = 0x50;
                    g_battle_data->gt4[i].r1 = 0x50;
                    g_battle_data->gt4[i].r2 = 0x50;
                    g_battle_data->gt4[i].r3 = 0x50;
                    g_battle_data->gt4[i].g0 = 0x60;
                    g_battle_data->gt4[i].g1 = 0x60;
                    g_battle_data->gt4[i].g2 = 0x60;
                    g_battle_data->gt4[i].g3 = 0x60;
                    g_battle_data->gt4[i].b0 = 0x10;
                    g_battle_data->gt4[i].b1 = 0x10;
                    g_battle_data->gt4[i].b2 = 0x10;
                    g_battle_data->gt4[i].b3 = 0x10;
                    g_battle_map_textured_quad_positions[i].polygon_flags &= 0xfffe;
                }
            }
        }
        g_battle_target_tile_mark_modes[2] = 7;
        other_state_a = g_battle_target_tile_mark_modes[0];
        other_state_b = g_battle_target_tile_mark_modes[1];
        if ((other_state_a | other_state_b) == 0 || (other_state_a == 5 && other_state_b == 3)) {
            battle_map_modify_palette(9, 8, 1, 0, 1, 10, 10, 10);
        }
        break;
    case 8:
        for (i = 0; i < g_battle_map_mesh_parts[0].counts[0]; i++) {
            if (g_battle_map_textured_triangle_positions[i].terrain_tile.packed != 0xfffe) {
                g_battle_target_color_tile_y
                    = (g_battle_map_textured_triangle_positions[i].terrain_tile.packed >> 1) & 0x7f;
                g_battle_target_color_tile_x = g_battle_map_textured_triangle_positions[i].terrain_tile.packed >> 8;
                tile = (s16)g_battle_target_color_tile_y * g_battle_map_tile_width + (s16)g_battle_target_color_tile_x;
                if (g_battle_map_textured_triangle_positions[i].terrain_tile.packed & 1) {
                    tile += 0x100;
                }
                marks = g_battle_map_tile_data_ptr[tile].ceiling_depth_and_marks;
                if (marks & 0x80) {
                    if (!(marks & 0x40)) {
                        g_battle_data->gt3[i].clut = (g_battle_data->gt3[i].clut & 0x803f) | 0x7800;
                        if (g_battle_map_textured_triangle_positions[i].polygon_flags & 0x8000) {
                            g_battle_map_textured_triangle_positions[i].polygon_flags |= 1;
                        } else {
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
                    } else {
                        g_battle_data->gt3[i].r0 = 0x60;
                        g_battle_data->gt3[i].r1 = 0x60;
                        g_battle_data->gt3[i].r2 = 0x60;
                        g_battle_data->gt3[i].g0 = 0x18;
                        g_battle_data->gt3[i].g1 = 0x18;
                        g_battle_data->gt3[i].g2 = 0x18;
                        g_battle_data->gt3[i].b0 = 0x20;
                        g_battle_data->gt3[i].b1 = 0x20;
                        g_battle_data->gt3[i].b2 = 0x20;
                    }
                }
            }
        }
        for (i = 0; i < g_battle_map_mesh_parts[0].counts[1]; i++) {
            if (g_battle_map_textured_quad_positions[i].terrain_tile.packed != 0xfffe) {
                g_battle_target_color_tile_y
                    = (g_battle_map_textured_quad_positions[i].terrain_tile.packed >> 1) & 0x7f;
                g_battle_target_color_tile_x = g_battle_map_textured_quad_positions[i].terrain_tile.packed >> 8;
                tile = (s16)g_battle_target_color_tile_y * g_battle_map_tile_width + (s16)g_battle_target_color_tile_x;
                if (g_battle_map_textured_quad_positions[i].terrain_tile.packed & 1) {
                    tile += 0x100;
                }
                marks = g_battle_map_tile_data_ptr[tile].ceiling_depth_and_marks;
                if (marks & 0x80) {
                    if (!(marks & 0x40)) {
                        g_battle_data->gt4[i].clut = (g_battle_data->gt4[i].clut & 0x803f) | 0x7800;
                        if (g_battle_map_textured_quad_positions[i].polygon_flags & 0x8000) {
                            g_battle_map_textured_quad_positions[i].polygon_flags |= 1;
                        } else {
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
                    } else {
                        g_battle_data->gt4[i].r0 = 0x60;
                        g_battle_data->gt4[i].r1 = 0x60;
                        g_battle_data->gt4[i].r2 = 0x60;
                        g_battle_data->gt4[i].r3 = 0x60;
                        g_battle_data->gt4[i].g0 = 0x18;
                        g_battle_data->gt4[i].g1 = 0x18;
                        g_battle_data->gt4[i].g2 = 0x18;
                        g_battle_data->gt4[i].g3 = 0x18;
                        g_battle_data->gt4[i].b0 = 0x20;
                        g_battle_data->gt4[i].b1 = 0x20;
                        g_battle_data->gt4[i].b2 = 0x20;
                        g_battle_data->gt4[i].b3 = 0x20;
                    }
                }
            }
        }
        g_battle_target_tile_mark_modes[2] = 8;
        g_battle_target_tile_color_buffer = g_battle_data;
        break;
    }
}
