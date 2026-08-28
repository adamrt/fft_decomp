#include "fft/battle.h"
#include "fft/battle_gfx.h"
#include "psx/types.h"

/*
 * Runtime polygon records interleave GPU packet fields with renderer state.
 * This pass skips the geometry and normal vectors consumed by
 * battle_map_append_mesh_geometry, then appends UV coordinates, CLUT commands,
 * texture-page commands, and the initial state of untextured records.
 * Target 0x800f5578 reads five-halfword textured triangles and six-halfword
 * textured quads. CLUT rows keep their low six bits and gain 0x7800;
 * texture pages are copied verbatim. Destination strides are 0x28/0x34 for
 * textured triangles/quads and 0x14/0x18 for untextured triangles/quads.
 * Untextured POLY_F3/POLY_F4 packets start with colour 0/0/1.
 */
/* Textured packets are addressed as g_battle_data plus a byte offset (in that
 * addu order); typed record pointers or indices break the pinned schedule. */
#define TEXTURED_TRIANGLE_AT(byte_offset) (*(POLY_GT3*)((u8*)g_battle_data->gt3 + (byte_offset)))
#define GT4_OFFSET                        sizeof(g_battle_data->gt3)
#define TEXTURED_QUAD_AT(byte_offset)     (*(POLY_GT4*)((u8*)g_battle_data + (byte_offset) + GT4_OFFSET))

void battle_map_init_mesh_render_records(u16* geometry_data) {
    u16* source = geometry_data;
    s32 polygon_index;
    s32 textured_triangle_count;
    s32 textured_quad_count;
    s32 untextured_triangle_count;
    s32 untextured_quad_count;
    s32 textured_record_offset;
    s32 triangle_vector_halfwords;
    s32 quad_vector_halfwords;

    polygon_index = g_battle_map_textured_triangle_render_record_count;
    {
        s32 renderer_data;

        renderer_data = g_battle_data;
        g_battle_map_mesh_data_buffer = renderer_data;
    }

    textured_triangle_count = *source++;
    textured_quad_count = *source++;
    untextured_triangle_count = *source++;
    untextured_quad_count = *source++;

    triangle_vector_halfwords = textured_triangle_count * 9;
    quad_vector_halfwords = textured_quad_count * 12;
    source += triangle_vector_halfwords + quad_vector_halfwords + untextured_triangle_count * 9
        + untextured_quad_count * 12 + triangle_vector_halfwords + quad_vector_halfwords;

    if (polygon_index < textured_triangle_count + polygon_index) {
        s32 loop_condition;
        s32 record_offset_units;

        record_offset_units = polygon_index * 4;
        record_offset_units += polygon_index;
        textured_record_offset = record_offset_units * 8;
        /* Pins: the per-field temporaries and both record pointers are
         * hand-allocated. With every pin removed the function is the same
         * size but nearly every temporary lands in a different register
         * (and the target's $s0 save disappears). */
        do {
            register u16 packed asm("$3");
            register s32 u0 asm("$10");
            register s32 v0 asm("$12");
            register s32 u1 asm("$8");
            register s32 v1 asm("$11");
            register s32 v2 asm("$9");
            register u16 clut_data asm("$14");
            register u16 texture_page asm("$13");
            register u16 clut_command asm("$2");
            register POLY_GT3* first_record asm("$2");
            POLY_GT3* final_record;

            packed = *source++;
            first_record = (POLY_GT3*)g_battle_data;
            v0 = packed >> 8;
            u0 = packed & 0xff;
            packed = *source++;
            clut_data = packed;
            packed = *source++;
            v1 = packed >> 8;
            u1 = packed & 0xff;
            packed = *source++;
            texture_page = packed;
            packed = *source;

            first_record = (POLY_GT3*)((u8*)first_record + textured_record_offset);
            first_record->u0 = u0;
            TEXTURED_TRIANGLE_AT(textured_record_offset).u1 = u1;
            TEXTURED_TRIANGLE_AT(textured_record_offset).u2 = packed;
            polygon_index++;
            TEXTURED_TRIANGLE_AT(textured_record_offset).v0 = v0;
            source++;
            TEXTURED_TRIANGLE_AT(textured_record_offset).v1 = v1;
            v2 = packed >> 8;
            TEXTURED_TRIANGLE_AT(textured_record_offset).v2 = v2;
            clut_command = (clut_data & 0x3f) | 0x7800;
            final_record = (POLY_GT3*)g_battle_data;
            final_record = (POLY_GT3*)((u8*)final_record + textured_record_offset);
            textured_record_offset += 0x28;
            loop_condition = textured_triangle_count + g_battle_map_textured_triangle_render_record_count;
            loop_condition = polygon_index < loop_condition;
            final_record->clut = clut_command;
            final_record->tpage = texture_page;
        } while (loop_condition);
    }

    polygon_index = g_battle_map_textured_quad_render_record_count;
    if (polygon_index < textured_quad_count + polygon_index) {
        s32 loop_condition;
        s32 record_offset_units;

        record_offset_units = polygon_index * 2;
        record_offset_units += polygon_index;
        record_offset_units *= 4;
        record_offset_units += polygon_index;
        textured_record_offset = record_offset_units * 4;
        /* Pins: the same hand allocation as the triangle loop, plus u2. */
        do {
            u16 packed;
            register s32 u0 asm("$10");
            register s32 v0 asm("$12");
            register s32 u1 asm("$8");
            register s32 v1 asm("$11");
            register s32 u2 asm("$4");
            register s32 v2 asm("$9");
            register u16 clut_data asm("$14");
            register u16 texture_page asm("$13");
            register u16 clut_command asm("$2");
            register u8* first_record asm("$2");
            u8* final_record;

            packed = *source++;
            first_record = (u8*)g_battle_data;
            v0 = packed >> 8;
            u0 = packed & 0xff;
            packed = *source++;
            clut_data = packed;
            packed = *source++;
            v1 = packed >> 8;
            u1 = packed & 0xff;
            packed = *source++;
            texture_page = packed;
            packed = *source++;
            v2 = packed >> 8;
            u2 = packed & 0xff;
            packed = *source;

            first_record += textured_record_offset;
            ((POLY_GT4*)(first_record + GT4_OFFSET))->u0 = u0;
            TEXTURED_QUAD_AT(textured_record_offset).u1 = u1;
            TEXTURED_QUAD_AT(textured_record_offset).u2 = u2;
            TEXTURED_QUAD_AT(textured_record_offset).u3 = packed;
            TEXTURED_QUAD_AT(textured_record_offset).v0 = v0;
            polygon_index++;
            TEXTURED_QUAD_AT(textured_record_offset).v1 = v1;
            source++;
            TEXTURED_QUAD_AT(textured_record_offset).v2 = v2;
            packed >>= 8;
            TEXTURED_QUAD_AT(textured_record_offset).v3 = packed;
            clut_command = (clut_data & 0x3f) | 0x7800;
            final_record = (u8*)g_battle_data;
            final_record += textured_record_offset;
            textured_record_offset += 0x34;
            loop_condition = textured_quad_count + g_battle_map_textured_quad_render_record_count;
            loop_condition = polygon_index < loop_condition;
            ((POLY_GT4*)(final_record + GT4_OFFSET))->clut = clut_command;
            ((POLY_GT4*)(final_record + GT4_OFFSET))->tpage = texture_page;
        } while (loop_condition);
    }

    polygon_index = g_battle_map_untextured_triangle_render_record_count;
    if (polygon_index < untextured_triangle_count + polygon_index) {
        s32 active;

        active = 1;
        do {
            g_battle_data->f3[polygon_index].r0 = 0;
            g_battle_data->f3[polygon_index].g0 = 0;
            g_battle_data->f3[polygon_index].b0 = active;
            polygon_index++;
        } while (polygon_index < untextured_triangle_count + g_battle_map_untextured_triangle_render_record_count);
    }

    polygon_index = g_battle_map_untextured_quad_render_record_count;
    if (polygon_index < untextured_quad_count + polygon_index) {
        s32 active;

        active = 1;
        do {
            g_battle_data->f4[polygon_index].r0 = 0;
            g_battle_data->f4[polygon_index].g0 = 0;
            g_battle_data->f4[polygon_index].b0 = active;
            polygon_index++;
        } while (polygon_index < untextured_quad_count + g_battle_map_untextured_quad_render_record_count);
    }
}
