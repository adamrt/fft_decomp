#include "fft/battle.h"
#include "psx/types.h"

/*
 * The geometry block starts with four polygon counts, then stores positions
 * for all four polygon classes, normals for textured polygons, per-polygon
 * render data, and finally terrain-tile locations for textured polygons.
 * Each position/normal vector occupies eight renderer bytes: XYZ followed by
 * a spare halfword.  The first position vector's spare halfword holds the
 * polygon's packed terrain-tile location.
 * Target 0x800f4dd4 is the geometry pass for primary and animated meshes;
 * battle_map_init_mesh_render_records performs the separate packet
 * pass. Count order is textured triangles/quads, then untextured
 * triangles/quads. Mesh metadata +0x88..+0x96 retains starting indices and
 * incoming counts before the persistent totals advance.
 */

/* Field address plus byte offset reproduces the target's per-field lui/addu
 * stores; record indexing does not. */
#define READ_GEOMETRY_HALFWORD()       (*source++)
#define COPY_GEOMETRY_HALFWORD(symbol) (*(u16*)((u8*)&(symbol) + destination_offset) = READ_GEOMETRY_HALFWORD())

void battle_map_append_mesh_geometry(u16* geometry_data, battle_map_mesh_part_metadata_t* metadata) {
    u16* source = geometry_data;
    s32 index;
    s32 textured_triangle_count;
    s32 textured_quad_count;
    s32 untextured_triangle_count;
    register s32 untextured_quad_count asm("$12");
    register s32 polygon_end asm("$3");
    register s32 polygon_end_copy asm("$4");
    s32 destination_byte_end;
    s32 destination_offset;
    s32 has_polygons;
    u16 textured_triangle_start;
    register u16 textured_quad_start asm("$3");
    u16 untextured_triangle_start;
    u16 untextured_quad_start;
    s32 current_textured_triangles;
    s32 current_textured_quads;
    s32 current_untextured_triangles;
    s32 current_untextured_quads;
    s32 new_untextured_triangle_count;
    register s32 new_untextured_quad_count asm("$3");
    s32 renderer_state;

    /* Pins: with all five removed the function keeps its size but the loop
     * counters/bounds shift registers ($t0<->$t1, $t3<->$t4) and the first
     * bound computation is hoisted above the metadata stores. The two
     * memory fences below are load-bearing on their own: they keep the
     * g_battle_map_resource_load_state store and the final count reload in the
     * target's order. */
    index = g_battle_map_textured_triangle_count;
    renderer_state = (s32)g_battle_data;
    g_battle_map_resource_load_state = 0x79;
    g_battle_map_mesh_data_buffer = renderer_state;

    textured_triangle_start = g_battle_map_textured_triangle_count;
    untextured_triangle_start = g_battle_map_untextured_triangle_count;
    untextured_quad_start = g_battle_map_untextured_quad_count;

    textured_triangle_count = READ_GEOMETRY_HALFWORD();
    textured_quad_count = READ_GEOMETRY_HALFWORD();
    untextured_triangle_count = READ_GEOMETRY_HALFWORD();
    untextured_quad_count = *source;

    /* Keeps the textured-quad count load below the entry stores (see above). */
    __asm__ volatile("" : : : "memory");
    textured_quad_start = g_battle_map_textured_quad_count;
    source++;
    metadata->textured_triangle_start = textured_triangle_start;
    metadata->untextured_triangle_start = untextured_triangle_start;
    metadata->untextured_quad_start = untextured_quad_start;
    metadata->textured_quad_start = textured_quad_start;
    metadata->textured_triangle_count = textured_triangle_count;
    metadata->textured_quad_count = textured_quad_count;
    metadata->untextured_triangle_count = untextured_triangle_count;
    metadata->untextured_quad_count = untextured_quad_count;

    polygon_end = textured_triangle_count + index;
    has_polygons = index < polygon_end;
    if (has_polygons) {
        polygon_end_copy = polygon_end;
        destination_offset = index * 0x18;
        do {
            COPY_GEOMETRY_HALFWORD(g_battle_map_textured_triangle_positions[0].x0);
            COPY_GEOMETRY_HALFWORD(g_battle_map_textured_triangle_positions[0].y0);
            COPY_GEOMETRY_HALFWORD(g_battle_map_textured_triangle_positions[0].z0);
            COPY_GEOMETRY_HALFWORD(g_battle_map_textured_triangle_positions[0].x1);
            COPY_GEOMETRY_HALFWORD(g_battle_map_textured_triangle_positions[0].y1);
            COPY_GEOMETRY_HALFWORD(g_battle_map_textured_triangle_positions[0].z1);
            COPY_GEOMETRY_HALFWORD(g_battle_map_textured_triangle_positions[0].x2);
            COPY_GEOMETRY_HALFWORD(g_battle_map_textured_triangle_positions[0].y2);
            COPY_GEOMETRY_HALFWORD(g_battle_map_textured_triangle_positions[0].z2);
            index++;
            destination_offset += 0x18;
        } while (index < polygon_end_copy);
    }

    index = g_battle_map_textured_quad_count;
    destination_byte_end = textured_quad_count + index;
    destination_offset = index << 5;
    has_polygons = index < destination_byte_end;
    if (has_polygons) {
        destination_byte_end <<= 5;
        do {
            COPY_GEOMETRY_HALFWORD(g_battle_map_textured_quad_positions[0].x0);
            COPY_GEOMETRY_HALFWORD(g_battle_map_textured_quad_positions[0].y0);
            COPY_GEOMETRY_HALFWORD(g_battle_map_textured_quad_positions[0].z0);
            COPY_GEOMETRY_HALFWORD(g_battle_map_textured_quad_positions[0].x1);
            COPY_GEOMETRY_HALFWORD(g_battle_map_textured_quad_positions[0].y1);
            COPY_GEOMETRY_HALFWORD(g_battle_map_textured_quad_positions[0].z1);
            COPY_GEOMETRY_HALFWORD(g_battle_map_textured_quad_positions[0].x2);
            COPY_GEOMETRY_HALFWORD(g_battle_map_textured_quad_positions[0].y2);
            COPY_GEOMETRY_HALFWORD(g_battle_map_textured_quad_positions[0].z2);
            COPY_GEOMETRY_HALFWORD(g_battle_map_textured_quad_positions[0].x3);
            COPY_GEOMETRY_HALFWORD(g_battle_map_textured_quad_positions[0].y3);
            COPY_GEOMETRY_HALFWORD(g_battle_map_textured_quad_positions[0].z3);
            destination_offset += 0x20;
        } while (destination_offset < destination_byte_end);
    }

    index = g_battle_map_untextured_triangle_count;
    polygon_end = untextured_triangle_count + index;
    has_polygons = index < polygon_end;
    if (has_polygons) {
        polygon_end_copy = polygon_end;
        destination_offset = index * 0x18;
        do {
            COPY_GEOMETRY_HALFWORD(g_battle_map_untextured_triangle_positions[0].x0);
            COPY_GEOMETRY_HALFWORD(g_battle_map_untextured_triangle_positions[0].y0);
            COPY_GEOMETRY_HALFWORD(g_battle_map_untextured_triangle_positions[0].z0);
            COPY_GEOMETRY_HALFWORD(g_battle_map_untextured_triangle_positions[0].x1);
            COPY_GEOMETRY_HALFWORD(g_battle_map_untextured_triangle_positions[0].y1);
            COPY_GEOMETRY_HALFWORD(g_battle_map_untextured_triangle_positions[0].z1);
            COPY_GEOMETRY_HALFWORD(g_battle_map_untextured_triangle_positions[0].x2);
            COPY_GEOMETRY_HALFWORD(g_battle_map_untextured_triangle_positions[0].y2);
            COPY_GEOMETRY_HALFWORD(g_battle_map_untextured_triangle_positions[0].z2);
            index++;
            destination_offset += 0x18;
        } while (index < polygon_end_copy);
    }

    index = g_battle_map_untextured_quad_count;
    destination_byte_end = untextured_quad_count + index;
    destination_offset = index << 5;
    has_polygons = index < destination_byte_end;
    if (has_polygons) {
        destination_byte_end <<= 5;
        do {
            COPY_GEOMETRY_HALFWORD(g_battle_map_untextured_quad_positions[0].x0);
            COPY_GEOMETRY_HALFWORD(g_battle_map_untextured_quad_positions[0].y0);
            COPY_GEOMETRY_HALFWORD(g_battle_map_untextured_quad_positions[0].z0);
            COPY_GEOMETRY_HALFWORD(g_battle_map_untextured_quad_positions[0].x1);
            COPY_GEOMETRY_HALFWORD(g_battle_map_untextured_quad_positions[0].y1);
            COPY_GEOMETRY_HALFWORD(g_battle_map_untextured_quad_positions[0].z1);
            COPY_GEOMETRY_HALFWORD(g_battle_map_untextured_quad_positions[0].x2);
            COPY_GEOMETRY_HALFWORD(g_battle_map_untextured_quad_positions[0].y2);
            COPY_GEOMETRY_HALFWORD(g_battle_map_untextured_quad_positions[0].z2);
            COPY_GEOMETRY_HALFWORD(g_battle_map_untextured_quad_positions[0].x3);
            COPY_GEOMETRY_HALFWORD(g_battle_map_untextured_quad_positions[0].y3);
            COPY_GEOMETRY_HALFWORD(g_battle_map_untextured_quad_positions[0].z3);
            destination_offset += 0x20;
        } while (destination_offset < destination_byte_end);
    }

    index = g_battle_map_textured_triangle_count;
    polygon_end = textured_triangle_count + index;
    has_polygons = index < polygon_end;
    if (has_polygons) {
        polygon_end_copy = polygon_end;
        destination_offset = index * 0x18;
        do {
            COPY_GEOMETRY_HALFWORD(g_battle_map_textured_triangle_normals[0].normals[0].vx);
            COPY_GEOMETRY_HALFWORD(g_battle_map_textured_triangle_normals[0].normals[0].vy);
            COPY_GEOMETRY_HALFWORD(g_battle_map_textured_triangle_normals[0].normals[0].vz);
            COPY_GEOMETRY_HALFWORD(g_battle_map_textured_triangle_normals[0].normals[1].vx);
            COPY_GEOMETRY_HALFWORD(g_battle_map_textured_triangle_normals[0].normals[1].vy);
            COPY_GEOMETRY_HALFWORD(g_battle_map_textured_triangle_normals[0].normals[1].vz);
            COPY_GEOMETRY_HALFWORD(g_battle_map_textured_triangle_normals[0].normals[2].vx);
            COPY_GEOMETRY_HALFWORD(g_battle_map_textured_triangle_normals[0].normals[2].vy);
            COPY_GEOMETRY_HALFWORD(g_battle_map_textured_triangle_normals[0].normals[2].vz);
            index++;
            destination_offset += 0x18;
        } while (index < polygon_end_copy);
    }

    index = g_battle_map_textured_quad_count;
    destination_byte_end = textured_quad_count + index;
    destination_offset = index << 5;
    has_polygons = index < destination_byte_end;
    if (has_polygons) {
        destination_byte_end <<= 5;
        do {
            COPY_GEOMETRY_HALFWORD(g_battle_map_textured_quad_normals[0].normals[0].vx);
            COPY_GEOMETRY_HALFWORD(g_battle_map_textured_quad_normals[0].normals[0].vy);
            COPY_GEOMETRY_HALFWORD(g_battle_map_textured_quad_normals[0].normals[0].vz);
            COPY_GEOMETRY_HALFWORD(g_battle_map_textured_quad_normals[0].normals[1].vx);
            COPY_GEOMETRY_HALFWORD(g_battle_map_textured_quad_normals[0].normals[1].vy);
            COPY_GEOMETRY_HALFWORD(g_battle_map_textured_quad_normals[0].normals[1].vz);
            COPY_GEOMETRY_HALFWORD(g_battle_map_textured_quad_normals[0].normals[2].vx);
            COPY_GEOMETRY_HALFWORD(g_battle_map_textured_quad_normals[0].normals[2].vy);
            COPY_GEOMETRY_HALFWORD(g_battle_map_textured_quad_normals[0].normals[2].vz);
            COPY_GEOMETRY_HALFWORD(g_battle_map_textured_quad_normals[0].normals[3].vx);
            COPY_GEOMETRY_HALFWORD(g_battle_map_textured_quad_normals[0].normals[3].vy);
            COPY_GEOMETRY_HALFWORD(g_battle_map_textured_quad_normals[0].normals[3].vz);
            destination_offset += 0x20;
        } while (destination_offset < destination_byte_end);
    }

    /* Skip texture records (10/12 bytes) and untextured records (4 bytes). */
    source += textured_triangle_count * 5 + textured_quad_count * 6 + untextured_triangle_count * 2
        + untextured_quad_count * 2;

    index = g_battle_map_textured_triangle_count;
    polygon_end = textured_triangle_count + index;
    has_polygons = index < polygon_end;
    if (has_polygons) {
        polygon_end_copy = polygon_end;
        destination_offset = index * 0x18;
        do {
            COPY_GEOMETRY_HALFWORD(g_battle_map_textured_triangle_positions[0].terrain_tile.packed);
            index++;
            destination_offset += 0x18;
        } while (index < polygon_end_copy);
    }

    index = g_battle_map_textured_quad_count;
    destination_byte_end = textured_quad_count + index;
    destination_offset = index << 5;
    has_polygons = index < destination_byte_end;
    if (has_polygons) {
        destination_byte_end <<= 5;
        do {
            COPY_GEOMETRY_HALFWORD(g_battle_map_textured_quad_positions[0].terrain_tile.packed);
            destination_offset += 0x20;
        } while (destination_offset < destination_byte_end);
    }

    current_textured_triangles = g_battle_map_textured_triangle_count;
    current_textured_quads = g_battle_map_textured_quad_count;
    g_battle_map_textured_triangle_count = textured_triangle_count + current_textured_triangles;
    current_untextured_triangles = g_battle_map_untextured_triangle_count;
    g_battle_map_textured_quad_count = textured_quad_count + current_textured_quads;
    /* Keeps the untextured-quad count reload after the store above. */
    __asm__ volatile("" : : : "memory");
    current_untextured_quads = g_battle_map_untextured_quad_count;
    new_untextured_triangle_count = untextured_triangle_count + current_untextured_triangles;
    new_untextured_quad_count = untextured_quad_count + current_untextured_quads;
    g_battle_map_untextured_triangle_count = new_untextured_triangle_count;
    g_battle_map_untextured_quad_count = new_untextured_quad_count;
}
