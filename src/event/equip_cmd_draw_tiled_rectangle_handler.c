#include "fft/event_equip.h"
#include "psx/types.h"

/* The definition narrows clut to u16; this file passes it as a full word, so
 * every call below goes through this word-clut prototype to keep the target's
 * argument setup (a u16 formal truncates at the call). */
typedef void (*equip_gfx_enqueue_textured_quad_word_clut_t)(const RECT*, s32, s32, const u8*, s32, u16, s32, s32);

/* Draw a rectangle tiled with 16-pixel textured quads.
 *
 * Kind 2 takes the rectangle from the global tiled rect; otherwise command
 * bytes 5 and 6 pack the full-tile counts and remainders in their nibbles.
 *
 * The target keeps sp+0x28..0x60 as reload spill slots for plain locals,
 * assigned 8 bytes apart in declaration order, so the declaration order below
 * is significant; pad0-pad2 fill the unreferenced slots at sp+0x68..0x78
 * through zero-instruction "m" operands. The "r" hints raise reference counts
 * so `command`, `full_columns` and `row` get s8, s5 and s3. Each call site uses
 * its own semitrans/tpage/otag temporaries. Setting `column = 0` after the call
 * emits `move s1,zero` after the jal. `y += 16` before `row++` fills the
 * full_rows reload delay slot. In the last loop, assigning `tile_w` and
 * `inner_clut` twice keeps `li v0,0x10` and the sp+0x58 clut reload in the loop.
 */
u8* equip_cmd_draw_tiled_rectangle_handler(u8* command) {
    RECT rect;
    s32 base_x;
    s32 base_y;
    s32 full_rows;
    s32 partial_width;
    s32 partial_height;
    s32 otag_index;
    u16 clut;
    s32 source_u;
    s32 pad0;
    s32 pad1;
    s32 pad2;
    s32 full_columns;
    s32 row;
    s32 column;
    s32 x;
    s32 y;
    s32 bottom_y;
    s32 tile_y;
    s32 last_y;
    s32 kind;
    s32 loop_clut;
    s32 inner_clut;
    s32 otag;
    s32 semitrans;
    u16 tpage;
    s32 semitrans2, semitrans3, semitrans4;
    u16 tpage2, tpage3, tpage4;
    s32 otag2, otag3, otag4;
    s32 tile_w;

    otag_index = g_equip_gfx_sprite_ot_index;
    clut = g_equip_text_metric_2;
    kind = command[2];

    if (kind == 2) {
        s32 width;
        s32 height;

        base_x = g_equip_gfx_tiled_rect.x;
        width = g_equip_gfx_tiled_rect.w;
        base_y = g_equip_gfx_tiled_rect.y;
        {
            s32 columns = width / 16;
            /* Divides into v0 and copies to s5, as the target does, instead of
             * shifting straight into s5. */
            __asm__("" : "=r"(columns) : "0"(columns));
            full_columns = columns;
        }
        partial_width = (s16)(width - full_columns * 16);
        height = g_equip_gfx_tiled_rect.h;
        full_rows = height / 16;
        partial_height = (s16)(height % 16);
    } else {
        u8 packed_width;
        u8 packed_height;

        base_x = command[3];
        packed_width = command[5];
        base_y = command[4];
        full_columns = packed_width >> 4;
        partial_width = packed_width & 15;
        packed_height = command[6];
        full_rows = packed_height >> 4;
        partial_height = packed_height & 15;
    }
    /* Allocation-priority hints (see above); they emit nothing. */
    __asm__ __volatile__("" : : "r"(full_columns));
    __asm__ __volatile__("" : : "r"(full_columns));
    __asm__ __volatile__("" : : "r"(command));
    __asm__ __volatile__("" : : "r"(command));
    row = 0;
    if (full_rows > 0) {
        loop_clut = clut;
        source_u = base_x + partial_width;
        y = base_y;
        do {
            semitrans = g_equip_gfx_semitransparency;
            tpage = g_equip_text_digit_texture_page;
            otag = otag_index;
            rect.x = base_x;
            rect.y = y;
            rect.w = partial_width;
            rect.h = 16;
            ((equip_gfx_enqueue_textured_quad_word_clut_t)equip_gfx_enqueue_textured_quad)(
                &rect, 8, 8, 0, semitrans, tpage, loop_clut, otag);
            column = 0;
            if (full_columns > 0) {
                tile_y = y;
                x = source_u;
                do {
                    semitrans2 = g_equip_gfx_semitransparency;
                    tpage2 = g_equip_text_digit_texture_page;
                    otag2 = otag_index;
                    rect.x = x;
                    rect.y = tile_y;
                    rect.w = 16;
                    rect.h = 16;
                    ((equip_gfx_enqueue_textured_quad_word_clut_t)equip_gfx_enqueue_textured_quad)(
                        &rect, 8, 8, 0, semitrans2, tpage2, loop_clut, otag2);
                    x += 16;
                    column++;
                } while (column < full_columns);
            }
            y += 16;
            row++;
        } while (row < full_rows);
    }

    /* Gives `row` s3 rather than s4 (see above). */
    __asm__ __volatile__("" : : "r"(row));
    __asm__ __volatile__("" : : "r"(row));
    if (partial_height != 0) {
        semitrans3 = g_equip_gfx_semitransparency;
        tpage3 = g_equip_text_digit_texture_page;
        otag3 = otag_index;
        bottom_y = base_y + row * 16;
        rect.x = base_x;
        rect.y = bottom_y;
        rect.w = partial_width;
        rect.h = partial_height;
        equip_gfx_enqueue_textured_quad(&rect, 8, 8, 0, semitrans3, tpage3, clut, otag3);
        column = 0;
        if (full_columns > 0) {
            last_y = bottom_y;
            x = base_x + partial_width;
            do {
                semitrans4 = g_equip_gfx_semitransparency;
                tpage4 = g_equip_text_digit_texture_page;
                otag4 = otag_index;
                rect.x = x;
                rect.y = last_y;
                tile_w = 16;
                rect.w = tile_w;
                tile_w = column;
                rect.h = partial_height;
                inner_clut = clut;
                ((equip_gfx_enqueue_textured_quad_word_clut_t)equip_gfx_enqueue_textured_quad)(
                    &rect, 8, 8, 0, semitrans4, tpage4, inner_clut, otag4);
                inner_clut = column;
                x += 16;
                column++;
            } while (column < full_columns);
        }
    }

    /* The three unreferenced spill slots at sp+0x68..0x78 (see above). */
    __asm__ __volatile__("" : : "m"(pad0));
    __asm__ __volatile__("" : : "m"(pad1));
    __asm__ __volatile__("" : : "m"(pad2));
    return command + command[1];
}
