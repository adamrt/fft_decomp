/* Target tile background colors. Its start is a cut chosen by topic; its end is an object boundary the read-only data
 * layout places somewhere nearby. */

#include "fft/battle.h"
#include "fft/battle_effect.h"
#include "fft/battle_gfx.h"
#include "fft/battle_map.h"
#include "fft/battle_target.h"
#include "psx/libgpu.h"
#include "psx/types.h"

/* Textured polygon positions (map_append_mesh_geometry). terrain_tile
 * packs the tile's layer (bit 0), z (bits 1..7) and x (bits 8..15); 0xfffe
 * marks a polygon without a tile. */

/*
 * Recolor the textured map polygons that lie on marked tiles.
 *
 * Each textured triangle/quad whose packed terrain tile (layer bit 0, z bits
 * 1..7, x bits 8..15; 0xfffe for none) resolves to a map tile with the mode's
 * mark bit in ceiling_depth_and_marks is recolored: modes 1/2/7 tint marks
 * 0x20/0x40/0x80 and select the marked-tile CLUT row (y 0x1e1), modes 5/3/8 restore the map CLUT row (y 0x1e0)
 * and the ambient color g_map_ambient_polygon_color (keeping polygons whose flag bit 15 is
 * set), and mode 0 returns every polygon to the map CLUT row. g_target_tile_mark_modes[0..2] record the state
 * of the three mark layers; when the other two are both clear, or in their
 * restored states, the tint modes also shift the unit palettes. Mode 1 is the
 * blue movement-range highlight.
 *
 * The CLUT and tint stores index the render buffer's polygon arrays; the
 * ambient color stores use pointer arithmetic on the decayed array
 * (`g_battle_gfx_current_render_buffer->gt3 + i`) instead, which is what keeps the target's
 * `addu offset, base` operand order for them.
 *
 * Every caller also passes a second argument, which the routine never reads.
 */
void target_set_tile_background_color(s32 mode, s32 unused) {
    s32 i;
    s16 other_state_a;
    s16 other_state_b;
    s16 tile;
    map_tile_marks_t marks;

    g_map_tile_data_ptr = g_map_tile_data;
    switch (mode) {
    case BATTLE_TILE_MARK_NONE:
        for (i = 0; i < g_map_mesh_parts[0].textured_triangle_count; i++) {
            g_battle_gfx_current_render_buffer->gt3[i].clut
                = (g_battle_gfx_current_render_buffer->gt3[i].clut & 0x803f) | MAP_CLUT(MAP_CLUT_ROW_MAP, 0);
        }
        for (i = 0; i < g_map_mesh_parts[0].textured_quad_count; i++) {
            g_battle_gfx_current_render_buffer->gt4[i].clut
                = (g_battle_gfx_current_render_buffer->gt4[i].clut & 0x803f) | MAP_CLUT(MAP_CLUT_ROW_MAP, 0);
        }
        g_target_tile_mark_modes[0] = BATTLE_TILE_MARK_HIDE_MOVE;
        g_target_tile_mark_modes[1] = BATTLE_TILE_MARK_HIDE_RANGE;
        g_target_tile_mark_modes[2] = BATTLE_TILE_MARK_HIDE_TARGETS;
        map_polygon_flag_command(MAP_POLYGON_FLAG_COMMAND_RESET_ACTIVE);
        map_polygon_flag_command(MAP_POLYGON_FLAG_COMMAND_RESTORE_AMBIENT);
        g_target_tile_color_buffer = g_battle_gfx_current_render_buffer;
        break;
    case BATTLE_TILE_MARK_SHOW_MOVE:
        for (i = 0; i < g_map_mesh_parts[0].textured_triangle_count; i++) {
            if (g_map_textured_triangle_positions[i].terrain_tile != MAP_POLYGON_TILE_NONE) {
                g_target_color_tile_y = (g_map_textured_triangle_positions[i].terrain_tile >> 1) & 0x7f;
                g_target_color_tile_x = g_map_textured_triangle_positions[i].terrain_tile >> 8;
                tile = g_target_color_tile_y * g_map_tile_width + g_target_color_tile_x;
                if (g_map_textured_triangle_positions[i].terrain_tile & 1) {
                    tile += 0x100;
                }
                if (g_map_tile_data_ptr[tile].ceiling_depth_and_marks.move_destination) {
                    g_battle_gfx_current_render_buffer->gt3[i].clut
                        = (g_battle_gfx_current_render_buffer->gt3[i].clut & 0x803f)
                        | MAP_CLUT(MAP_CLUT_ROW_MARKED_TILES, 0);
                    g_battle_gfx_current_render_buffer->gt3[i].r0 = 0x20;
                    g_battle_gfx_current_render_buffer->gt3[i].r1 = 0x20;
                    g_battle_gfx_current_render_buffer->gt3[i].r2 = 0x20;
                    g_battle_gfx_current_render_buffer->gt3[i].g0 = 0x28;
                    g_battle_gfx_current_render_buffer->gt3[i].g1 = 0x28;
                    g_battle_gfx_current_render_buffer->gt3[i].g2 = 0x28;
                    g_battle_gfx_current_render_buffer->gt3[i].b0 = 0x60;
                    g_battle_gfx_current_render_buffer->gt3[i].b1 = 0x60;
                    g_battle_gfx_current_render_buffer->gt3[i].b2 = 0x60;
                    g_map_textured_triangle_positions[i].polygon_flags &= ~MAP_POLYGON_FLAG_LIT_ACTIVE;
                }
            }
        }
        for (i = 0; i < g_map_mesh_parts[0].textured_quad_count; i++) {
            if (g_map_textured_quad_positions[i].terrain_tile != MAP_POLYGON_TILE_NONE) {
                g_target_color_tile_y = (g_map_textured_quad_positions[i].terrain_tile >> 1) & 0x7f;
                g_target_color_tile_x = g_map_textured_quad_positions[i].terrain_tile >> 8;
                tile = g_target_color_tile_y * g_map_tile_width + g_target_color_tile_x;
                if (g_map_textured_quad_positions[i].terrain_tile & 1) {
                    tile += 0x100;
                }
                if (g_map_tile_data_ptr[tile].ceiling_depth_and_marks.move_destination) {
                    g_battle_gfx_current_render_buffer->gt4[i].clut
                        = (g_battle_gfx_current_render_buffer->gt4[i].clut & 0x803f)
                        | MAP_CLUT(MAP_CLUT_ROW_MARKED_TILES, 0);
                    g_battle_gfx_current_render_buffer->gt4[i].r0 = 0x20;
                    g_battle_gfx_current_render_buffer->gt4[i].r1 = 0x20;
                    g_battle_gfx_current_render_buffer->gt4[i].r2 = 0x20;
                    g_battle_gfx_current_render_buffer->gt4[i].r3 = 0x20;
                    g_battle_gfx_current_render_buffer->gt4[i].g0 = 0x28;
                    g_battle_gfx_current_render_buffer->gt4[i].g1 = 0x28;
                    g_battle_gfx_current_render_buffer->gt4[i].g2 = 0x28;
                    g_battle_gfx_current_render_buffer->gt4[i].g3 = 0x28;
                    g_battle_gfx_current_render_buffer->gt4[i].b0 = 0x60;
                    g_battle_gfx_current_render_buffer->gt4[i].b1 = 0x60;
                    g_battle_gfx_current_render_buffer->gt4[i].b2 = 0x60;
                    g_battle_gfx_current_render_buffer->gt4[i].b3 = 0x60;
                    g_map_textured_quad_positions[i].polygon_flags &= ~MAP_POLYGON_FLAG_LIT_ACTIVE;
                }
            }
        }
        g_target_tile_mark_modes[0] = BATTLE_TILE_MARK_SHOW_MOVE;
        other_state_a = g_target_tile_mark_modes[1];
        other_state_b = g_target_tile_mark_modes[2];
        if ((other_state_a | other_state_b) == 0
            || (other_state_a == BATTLE_TILE_MARK_HIDE_RANGE && other_state_b == BATTLE_TILE_MARK_HIDE_TARGETS)) {
            map_modify_palette(MAP_COLOR_MODE_BASE_REPEAT, 8, MAP_CLUT_ROW_MARKED_TILES, 0, 1, 10, 10, 10);
        }
        break;
    case BATTLE_TILE_MARK_HIDE_MOVE:
        for (i = 0; i < g_map_mesh_parts[0].textured_triangle_count; i++) {
            if (g_map_textured_triangle_positions[i].terrain_tile != MAP_POLYGON_TILE_NONE) {
                g_target_color_tile_y = (g_map_textured_triangle_positions[i].terrain_tile >> 1) & 0x7f;
                g_target_color_tile_x = g_map_textured_triangle_positions[i].terrain_tile >> 8;
                tile = g_target_color_tile_y * g_map_tile_width + g_target_color_tile_x;
                if (g_map_textured_triangle_positions[i].terrain_tile & 1) {
                    tile += 0x100;
                }
                if (g_map_tile_data_ptr[tile].ceiling_depth_and_marks.move_destination) {
                    g_battle_gfx_current_render_buffer->gt3[i].clut
                        = (g_battle_gfx_current_render_buffer->gt3[i].clut & 0x803f) | MAP_CLUT(MAP_CLUT_ROW_MAP, 0);
                    if (g_map_textured_triangle_positions[i].polygon_flags & MAP_POLYGON_FLAG_LIT) {
                        g_map_textured_triangle_positions[i].polygon_flags |= MAP_POLYGON_FLAG_LIT_ACTIVE;
                    } else {
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
            }
        }
        for (i = 0; i < g_map_mesh_parts[0].textured_quad_count; i++) {
            if (g_map_textured_quad_positions[i].terrain_tile != MAP_POLYGON_TILE_NONE) {
                g_target_color_tile_y = (g_map_textured_quad_positions[i].terrain_tile >> 1) & 0x7f;
                g_target_color_tile_x = g_map_textured_quad_positions[i].terrain_tile >> 8;
                tile = g_target_color_tile_y * g_map_tile_width + g_target_color_tile_x;
                if (g_map_textured_quad_positions[i].terrain_tile & 1) {
                    tile += 0x100;
                }
                if (g_map_tile_data_ptr[tile].ceiling_depth_and_marks.move_destination) {
                    g_battle_gfx_current_render_buffer->gt4[i].clut
                        = (g_battle_gfx_current_render_buffer->gt4[i].clut & 0x803f) | MAP_CLUT(MAP_CLUT_ROW_MAP, 0);
                    if (g_map_textured_quad_positions[i].polygon_flags & MAP_POLYGON_FLAG_LIT) {
                        g_map_textured_quad_positions[i].polygon_flags |= MAP_POLYGON_FLAG_LIT_ACTIVE;
                    } else {
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
            }
        }
        g_target_tile_mark_modes[0] = BATTLE_TILE_MARK_HIDE_MOVE;
        g_target_tile_color_buffer = g_battle_gfx_current_render_buffer;
        break;
    case BATTLE_TILE_MARK_SHOW_RANGE:
        for (i = 0; i < g_map_mesh_parts[0].textured_triangle_count; i++) {
            if (g_map_textured_triangle_positions[i].terrain_tile != MAP_POLYGON_TILE_NONE) {
                g_target_color_tile_y = (g_map_textured_triangle_positions[i].terrain_tile >> 1) & 0x7f;
                g_target_color_tile_x = g_map_textured_triangle_positions[i].terrain_tile >> 8;
                tile = g_target_color_tile_y * g_map_tile_width + g_target_color_tile_x;
                if (g_map_textured_triangle_positions[i].terrain_tile & 1) {
                    tile += 0x100;
                }
                if (g_map_tile_data_ptr[tile].ceiling_depth_and_marks.ability_range) {
                    g_battle_gfx_current_render_buffer->gt3[i].clut
                        = (g_battle_gfx_current_render_buffer->gt3[i].clut & 0x803f)
                        | MAP_CLUT(MAP_CLUT_ROW_MARKED_TILES, 0);
                    g_battle_gfx_current_render_buffer->gt3[i].r0 = 0x60;
                    g_battle_gfx_current_render_buffer->gt3[i].r1 = 0x60;
                    g_battle_gfx_current_render_buffer->gt3[i].r2 = 0x60;
                    g_battle_gfx_current_render_buffer->gt3[i].g0 = 0x18;
                    g_battle_gfx_current_render_buffer->gt3[i].g1 = 0x18;
                    g_battle_gfx_current_render_buffer->gt3[i].g2 = 0x18;
                    g_battle_gfx_current_render_buffer->gt3[i].b0 = 0x20;
                    g_battle_gfx_current_render_buffer->gt3[i].b1 = 0x20;
                    g_battle_gfx_current_render_buffer->gt3[i].b2 = 0x20;
                    g_map_textured_triangle_positions[i].polygon_flags &= ~MAP_POLYGON_FLAG_LIT_ACTIVE;
                }
            }
        }
        for (i = 0; i < g_map_mesh_parts[0].textured_quad_count; i++) {
            if (g_map_textured_quad_positions[i].terrain_tile != MAP_POLYGON_TILE_NONE) {
                g_target_color_tile_y = (g_map_textured_quad_positions[i].terrain_tile >> 1) & 0x7f;
                g_target_color_tile_x = g_map_textured_quad_positions[i].terrain_tile >> 8;
                tile = g_target_color_tile_y * g_map_tile_width + g_target_color_tile_x;
                if (g_map_textured_quad_positions[i].terrain_tile & 1) {
                    tile += 0x100;
                }
                if (g_map_tile_data_ptr[tile].ceiling_depth_and_marks.ability_range) {
                    g_battle_gfx_current_render_buffer->gt4[i].clut
                        = (g_battle_gfx_current_render_buffer->gt4[i].clut & 0x803f)
                        | MAP_CLUT(MAP_CLUT_ROW_MARKED_TILES, 0);
                    g_battle_gfx_current_render_buffer->gt4[i].r0 = 0x60;
                    g_battle_gfx_current_render_buffer->gt4[i].r1 = 0x60;
                    g_battle_gfx_current_render_buffer->gt4[i].r2 = 0x60;
                    g_battle_gfx_current_render_buffer->gt4[i].r3 = 0x60;
                    g_battle_gfx_current_render_buffer->gt4[i].g0 = 0x18;
                    g_battle_gfx_current_render_buffer->gt4[i].g1 = 0x18;
                    g_battle_gfx_current_render_buffer->gt4[i].g2 = 0x18;
                    g_battle_gfx_current_render_buffer->gt4[i].g3 = 0x18;
                    g_battle_gfx_current_render_buffer->gt4[i].b0 = 0x20;
                    g_battle_gfx_current_render_buffer->gt4[i].b1 = 0x20;
                    g_battle_gfx_current_render_buffer->gt4[i].b2 = 0x20;
                    g_battle_gfx_current_render_buffer->gt4[i].b3 = 0x20;
                    g_map_textured_quad_positions[i].polygon_flags &= ~MAP_POLYGON_FLAG_LIT_ACTIVE;
                }
            }
        }
        g_target_tile_mark_modes[1] = BATTLE_TILE_MARK_SHOW_RANGE;
        other_state_a = g_target_tile_mark_modes[0];
        other_state_b = g_target_tile_mark_modes[2];
        if ((other_state_a | other_state_b) == 0
            || (other_state_a == BATTLE_TILE_MARK_HIDE_MOVE && other_state_b == BATTLE_TILE_MARK_HIDE_TARGETS)) {
            map_modify_palette(MAP_COLOR_MODE_BASE_REPEAT, 8, MAP_CLUT_ROW_MARKED_TILES, 0, 1, 10, 10, 10);
        }
        break;
    case BATTLE_TILE_MARK_HIDE_RANGE:
        for (i = 0; i < g_map_mesh_parts[0].textured_triangle_count; i++) {
            if (g_map_textured_triangle_positions[i].terrain_tile != MAP_POLYGON_TILE_NONE) {
                g_target_color_tile_y = (g_map_textured_triangle_positions[i].terrain_tile >> 1) & 0x7f;
                g_target_color_tile_x = g_map_textured_triangle_positions[i].terrain_tile >> 8;
                tile = g_target_color_tile_y * g_map_tile_width + g_target_color_tile_x;
                if (g_map_textured_triangle_positions[i].terrain_tile & 1) {
                    tile += 0x100;
                }
                if (g_map_tile_data_ptr[tile].ceiling_depth_and_marks.ability_range) {
                    g_battle_gfx_current_render_buffer->gt3[i].clut
                        = (g_battle_gfx_current_render_buffer->gt3[i].clut & 0x803f) | MAP_CLUT(MAP_CLUT_ROW_MAP, 0);
                    if (g_map_textured_triangle_positions[i].polygon_flags & MAP_POLYGON_FLAG_LIT) {
                        g_map_textured_triangle_positions[i].polygon_flags |= MAP_POLYGON_FLAG_LIT_ACTIVE;
                    } else {
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
            }
        }
        for (i = 0; i < g_map_mesh_parts[0].textured_quad_count; i++) {
            if (g_map_textured_quad_positions[i].terrain_tile != MAP_POLYGON_TILE_NONE) {
                g_target_color_tile_y = (g_map_textured_quad_positions[i].terrain_tile >> 1) & 0x7f;
                g_target_color_tile_x = g_map_textured_quad_positions[i].terrain_tile >> 8;
                tile = g_target_color_tile_y * g_map_tile_width + g_target_color_tile_x;
                if (g_map_textured_quad_positions[i].terrain_tile & 1) {
                    tile += 0x100;
                }
                if (g_map_tile_data_ptr[tile].ceiling_depth_and_marks.ability_range) {
                    g_battle_gfx_current_render_buffer->gt4[i].clut
                        = (g_battle_gfx_current_render_buffer->gt4[i].clut & 0x803f) | MAP_CLUT(MAP_CLUT_ROW_MAP, 0);
                    if (g_map_textured_quad_positions[i].polygon_flags & MAP_POLYGON_FLAG_LIT) {
                        g_map_textured_quad_positions[i].polygon_flags |= MAP_POLYGON_FLAG_LIT_ACTIVE;
                    } else {
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
            }
        }
        g_target_tile_mark_modes[1] = BATTLE_TILE_MARK_HIDE_RANGE;
        g_target_tile_color_buffer = g_battle_gfx_current_render_buffer;
        break;
    case BATTLE_TILE_MARK_SHOW_TARGETS:
        for (i = 0; i < g_map_mesh_parts[0].textured_triangle_count; i++) {
            if (g_map_textured_triangle_positions[i].terrain_tile != MAP_POLYGON_TILE_NONE) {
                g_target_color_tile_y = (g_map_textured_triangle_positions[i].terrain_tile >> 1) & 0x7f;
                g_target_color_tile_x = g_map_textured_triangle_positions[i].terrain_tile >> 8;
                tile = g_target_color_tile_y * g_map_tile_width + g_target_color_tile_x;
                if (g_map_textured_triangle_positions[i].terrain_tile & 1) {
                    tile += 0x100;
                }
                if (g_map_tile_data_ptr[tile].ceiling_depth_and_marks.targeted) {
                    g_battle_gfx_current_render_buffer->gt3[i].clut
                        = (g_battle_gfx_current_render_buffer->gt3[i].clut & 0x803f)
                        | MAP_CLUT(MAP_CLUT_ROW_MARKED_TILES, 0);
                    g_battle_gfx_current_render_buffer->gt3[i].r0 = 0x50;
                    g_battle_gfx_current_render_buffer->gt3[i].r1 = 0x50;
                    g_battle_gfx_current_render_buffer->gt3[i].r2 = 0x50;
                    g_battle_gfx_current_render_buffer->gt3[i].g0 = 0x60;
                    g_battle_gfx_current_render_buffer->gt3[i].g1 = 0x60;
                    g_battle_gfx_current_render_buffer->gt3[i].g2 = 0x60;
                    g_battle_gfx_current_render_buffer->gt3[i].b0 = 0x10;
                    g_battle_gfx_current_render_buffer->gt3[i].b1 = 0x10;
                    g_battle_gfx_current_render_buffer->gt3[i].b2 = 0x10;
                    g_map_textured_triangle_positions[i].polygon_flags &= ~MAP_POLYGON_FLAG_LIT_ACTIVE;
                }
            }
        }
        for (i = 0; i < g_map_mesh_parts[0].textured_quad_count; i++) {
            if (g_map_textured_quad_positions[i].terrain_tile != MAP_POLYGON_TILE_NONE) {
                g_target_color_tile_y = (g_map_textured_quad_positions[i].terrain_tile >> 1) & 0x7f;
                g_target_color_tile_x = g_map_textured_quad_positions[i].terrain_tile >> 8;
                tile = g_target_color_tile_y * g_map_tile_width + g_target_color_tile_x;
                if (g_map_textured_quad_positions[i].terrain_tile & 1) {
                    tile += 0x100;
                }
                if (g_map_tile_data_ptr[tile].ceiling_depth_and_marks.targeted) {
                    g_battle_gfx_current_render_buffer->gt4[i].clut
                        = (g_battle_gfx_current_render_buffer->gt4[i].clut & 0x803f)
                        | MAP_CLUT(MAP_CLUT_ROW_MARKED_TILES, 0);
                    g_battle_gfx_current_render_buffer->gt4[i].r0 = 0x50;
                    g_battle_gfx_current_render_buffer->gt4[i].r1 = 0x50;
                    g_battle_gfx_current_render_buffer->gt4[i].r2 = 0x50;
                    g_battle_gfx_current_render_buffer->gt4[i].r3 = 0x50;
                    g_battle_gfx_current_render_buffer->gt4[i].g0 = 0x60;
                    g_battle_gfx_current_render_buffer->gt4[i].g1 = 0x60;
                    g_battle_gfx_current_render_buffer->gt4[i].g2 = 0x60;
                    g_battle_gfx_current_render_buffer->gt4[i].g3 = 0x60;
                    g_battle_gfx_current_render_buffer->gt4[i].b0 = 0x10;
                    g_battle_gfx_current_render_buffer->gt4[i].b1 = 0x10;
                    g_battle_gfx_current_render_buffer->gt4[i].b2 = 0x10;
                    g_battle_gfx_current_render_buffer->gt4[i].b3 = 0x10;
                    g_map_textured_quad_positions[i].polygon_flags &= ~MAP_POLYGON_FLAG_LIT_ACTIVE;
                }
            }
        }
        g_target_tile_mark_modes[2] = BATTLE_TILE_MARK_SHOW_TARGETS;
        other_state_a = g_target_tile_mark_modes[0];
        other_state_b = g_target_tile_mark_modes[1];
        if ((other_state_a | other_state_b) == 0
            || (other_state_a == BATTLE_TILE_MARK_HIDE_MOVE && other_state_b == BATTLE_TILE_MARK_HIDE_RANGE)) {
            map_modify_palette(MAP_COLOR_MODE_BASE_REPEAT, 8, MAP_CLUT_ROW_MARKED_TILES, 0, 1, 10, 10, 10);
        }
        break;
    case BATTLE_TILE_MARK_HIDE_TARGETS:
        for (i = 0; i < g_map_mesh_parts[0].textured_triangle_count; i++) {
            if (g_map_textured_triangle_positions[i].terrain_tile != MAP_POLYGON_TILE_NONE) {
                g_target_color_tile_y = (g_map_textured_triangle_positions[i].terrain_tile >> 1) & 0x7f;
                g_target_color_tile_x = g_map_textured_triangle_positions[i].terrain_tile >> 8;
                tile = g_target_color_tile_y * g_map_tile_width + g_target_color_tile_x;
                if (g_map_textured_triangle_positions[i].terrain_tile & 1) {
                    tile += 0x100;
                }
                marks = g_map_tile_data_ptr[tile].ceiling_depth_and_marks;
                if (marks.targeted) {
                    if (!(marks.ability_range)) {
                        g_battle_gfx_current_render_buffer->gt3[i].clut
                            = (g_battle_gfx_current_render_buffer->gt3[i].clut & 0x803f)
                            | MAP_CLUT(MAP_CLUT_ROW_MAP, 0);
                        if (g_map_textured_triangle_positions[i].polygon_flags & MAP_POLYGON_FLAG_LIT) {
                            g_map_textured_triangle_positions[i].polygon_flags |= MAP_POLYGON_FLAG_LIT_ACTIVE;
                        } else {
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
                    } else {
                        g_battle_gfx_current_render_buffer->gt3[i].r0 = 0x60;
                        g_battle_gfx_current_render_buffer->gt3[i].r1 = 0x60;
                        g_battle_gfx_current_render_buffer->gt3[i].r2 = 0x60;
                        g_battle_gfx_current_render_buffer->gt3[i].g0 = 0x18;
                        g_battle_gfx_current_render_buffer->gt3[i].g1 = 0x18;
                        g_battle_gfx_current_render_buffer->gt3[i].g2 = 0x18;
                        g_battle_gfx_current_render_buffer->gt3[i].b0 = 0x20;
                        g_battle_gfx_current_render_buffer->gt3[i].b1 = 0x20;
                        g_battle_gfx_current_render_buffer->gt3[i].b2 = 0x20;
                    }
                }
            }
        }
        for (i = 0; i < g_map_mesh_parts[0].textured_quad_count; i++) {
            if (g_map_textured_quad_positions[i].terrain_tile != MAP_POLYGON_TILE_NONE) {
                g_target_color_tile_y = (g_map_textured_quad_positions[i].terrain_tile >> 1) & 0x7f;
                g_target_color_tile_x = g_map_textured_quad_positions[i].terrain_tile >> 8;
                tile = g_target_color_tile_y * g_map_tile_width + g_target_color_tile_x;
                if (g_map_textured_quad_positions[i].terrain_tile & 1) {
                    tile += 0x100;
                }
                marks = g_map_tile_data_ptr[tile].ceiling_depth_and_marks;
                if (marks.targeted) {
                    if (!(marks.ability_range)) {
                        g_battle_gfx_current_render_buffer->gt4[i].clut
                            = (g_battle_gfx_current_render_buffer->gt4[i].clut & 0x803f)
                            | MAP_CLUT(MAP_CLUT_ROW_MAP, 0);
                        if (g_map_textured_quad_positions[i].polygon_flags & MAP_POLYGON_FLAG_LIT) {
                            g_map_textured_quad_positions[i].polygon_flags |= MAP_POLYGON_FLAG_LIT_ACTIVE;
                        } else {
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
                    } else {
                        g_battle_gfx_current_render_buffer->gt4[i].r0 = 0x60;
                        g_battle_gfx_current_render_buffer->gt4[i].r1 = 0x60;
                        g_battle_gfx_current_render_buffer->gt4[i].r2 = 0x60;
                        g_battle_gfx_current_render_buffer->gt4[i].r3 = 0x60;
                        g_battle_gfx_current_render_buffer->gt4[i].g0 = 0x18;
                        g_battle_gfx_current_render_buffer->gt4[i].g1 = 0x18;
                        g_battle_gfx_current_render_buffer->gt4[i].g2 = 0x18;
                        g_battle_gfx_current_render_buffer->gt4[i].g3 = 0x18;
                        g_battle_gfx_current_render_buffer->gt4[i].b0 = 0x20;
                        g_battle_gfx_current_render_buffer->gt4[i].b1 = 0x20;
                        g_battle_gfx_current_render_buffer->gt4[i].b2 = 0x20;
                        g_battle_gfx_current_render_buffer->gt4[i].b3 = 0x20;
                    }
                }
            }
        }
        g_target_tile_mark_modes[2] = BATTLE_TILE_MARK_HIDE_TARGETS;
        g_target_tile_color_buffer = g_battle_gfx_current_render_buffer;
        break;
    }
}

/* Initializes both battle render buffers: every primitive in each pool gets its
 * GPU command code, the full-screen Gouraud overlay gets its corners and two
 * semi-transparent draw modes, and the ordering table pointers are bound.
 *
 * The function-scope counters are intentionally reused by each pool loop:
 * their lifetime reproduces the target register allocation. */
void battle_gfx_init_render_buffers(void) {
    POLY_F3 prototype;
    s32 primitive_index;
    s32 buffer_index;

    g_battle_gfx_current_render_buffer = &g_battle_render_buffers[0];
    g_battle_render_buffers[0].otag = g_main_gfx_battle_otags[0];
    g_battle_render_buffers[1].otag = g_main_gfx_battle_otags[1];
    {
        for (buffer_index = 0; buffer_index < 2; buffer_index++) {
            for (primitive_index = 0; primitive_index < 64; primitive_index++) {
                SetPolyFT3(&g_battle_render_buffers[buffer_index].ft3[primitive_index]);
            }
        }
    }
    SetPolyF3(&prototype);
    g_map_current_darkness_color.r = 0;
    g_map_current_darkness_color.g = 0;
    g_map_current_darkness_color.b = 1;
    g_map_current_darkness_color.cd = prototype.code;
    SetPolyG4(&g_battle_render_buffers[0].overlay);
    SetPolyG4(&g_battle_render_buffers[1].overlay);
    setXY4(&g_battle_render_buffers[0].overlay, 0, 0, 0x180, 0, 0, 0xF0, 0x180, 0xF0);
    setXY4(&g_battle_render_buffers[1].overlay, 0, 0, 0x180, 0, 0, 0xF0, 0x180, 0xF0);
    SetDrawMode(&g_battle_render_buffers[0].overlay_modes[1], 0, 1, 0, 0);
    SetDrawMode(&g_battle_render_buffers[1].overlay_modes[1], 0, 1, 0, 0);
    SetDrawMode(&g_battle_render_buffers[0].overlay_modes[0], 0, 1, 0, 0);
    SetDrawMode(&g_battle_render_buffers[1].overlay_modes[0], 0, 1, 0, 0);
    {
        for (primitive_index = 0; primitive_index < 360; primitive_index++) {
            for (buffer_index = 0; buffer_index < 2; buffer_index++) {
                SetPolyGT3(&g_battle_render_buffers[buffer_index].gt3[primitive_index]);
            }
        }
    }
    {
        for (primitive_index = 0; primitive_index < 710; primitive_index++) {
            for (buffer_index = 0; buffer_index < 2; buffer_index++) {
                SetPolyGT4(&g_battle_render_buffers[buffer_index].gt4[primitive_index]);
            }
        }
    }
    {
        for (primitive_index = 0; primitive_index < 64; primitive_index++) {
            for (buffer_index = 0; buffer_index < 2; buffer_index++) {
                SetPolyF3(&g_battle_render_buffers[buffer_index].f3[primitive_index]);
            }
        }
    }
    {
        for (primitive_index = 0; primitive_index < 256; primitive_index++) {
            for (buffer_index = 0; buffer_index < 2; buffer_index++) {
                SetPolyF4(&g_battle_render_buffers[buffer_index].f4[primitive_index]);
            }
        }
    }
    g_sound_primary_weather_sfx_id = 0;
    g_sound_weather_sfx_enabled = 0;
    g_effect_map_vertices = 0;
}
