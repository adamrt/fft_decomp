/* Animate and draw CARD's save-list cursor from its sprite sequence. */

#include "fft/gfx.h"
#include "fft/gfx_primitive.h"
#include "fft/menu.h"
#include "fft/menu_cursor.h"
#include "fft/menu_gfx.h"
#include "fft/menu_script.h"
#include "fft/menu_window.h"
#include "fft/save.h"
#include "fft/text.h"
#include "psx/libc.h"
#include "psx/libgpu.h"
#include "psx/types.h"

extern u16 g_save_gfx_texture_page;
extern u16 g_save_gfx_draw_clut;

static s16 g_save_menu_window_right_x = 0;

/* Stream command: draw a textured quad of size (command[5], command[6]) at
 * (command[3], command[4]) with texture origin (command[7], command[8]), in the
 * current sprite color or, for MENU_SCRIPT_OP_UNCOLORED_QUAD, unshaded. Returns the next command. */
u8* menu_draw_textured_quad(u8* command) {
    RECT rect;
    const rgb24_t* color;
    s32 x = command[3];
    s32 y = command[4];

    rect.x = x;
    rect.y = y;
    rect.w = command[5];
    rect.h = command[6];
    color = &g_menu_text_color;
    if (command[0] == MENU_SCRIPT_OP_UNCOLORED_QUAD) {
        color = 0;
    }
    menu_gfx_enqueue_textured_quad(&rect, command[7], command[8], color, g_menu_gfx_semitransparency,
        g_save_gfx_texture_page, g_save_gfx_draw_clut, g_menu_gfx_otag_index);
    return command + command[1];
}

/*
 * Draw a menu window's 5x9/5x7 border pieces around its rectangle and tile
 * the interior through menu_opcode_11_draw_tiled_rectangle.
 *
 * Source 2 takes the signed rectangle from the shared window record, other sources use the command's
 * byte operands, and source 1 selects the alternate border texture
 * coordinates. Returns the command that follows.
 */
menu_window_command_t* menu_draw_window_frame(menu_window_command_t* command) {
    RECT rect;
    s32 left;
    s32 top;
    s32 full_columns;
    s32 full_rows;
    s32 partial_width;
    s32 partial_height;
    s32 priority = g_menu_gfx_otag_index;
    s16 u_left;
    s16 u_middle;
    s16 u_right;
    s16 v_bottom;
    u16 clut = g_menu_palette_clut;
    s16 v_top;
    s32 column;
    s32 row;

    if (command->rectangle_source == 2) {
        left = g_menu_window_rect_source->rect.x - 5;
        top = g_menu_window_rect_source->rect.y - 9;
        full_columns = g_menu_window_rect_source->rect.w / 16;
        partial_width = g_menu_window_rect_source->rect.w % 16;
        full_rows = g_menu_window_rect_source->rect.h / 16;
        partial_height = g_menu_window_rect_source->rect.h % 16;
    } else {
        u8 width;
        u8 height;
        left = command->x - 5;
        top = command->y - 9;
        width = command->width;
        full_columns = width >> 4;
        partial_width = width & 15;
        height = command->height;
        full_rows = height >> 4;
        partial_height = height & 15;
    }
    g_cmd_window_interior_command.rectangle_source = command->rectangle_source;
    g_save_menu_window_right_x = command->width + left + 5;
    v_top = 2;
    if (command->rectangle_source == 1) {
        u_left = 0xDA;
        v_top = 3;
        u_middle = 0xDF;
        u_right = 0xF7;
        v_bottom = 0x18;
    } else {
        u_left = 2;
        u_middle = 8;
        u_right = 0x1F;
        v_bottom = 0x1C;
    }

    /* Top edge: left corner, the partial piece, the full 16-pixel pieces, right corner. */
    rect.x = left;
    rect.y = top;
    rect.w = 5;
    rect.h = 9;
    menu_gfx_enqueue_textured_quad(
        &rect, u_left, v_top, 0, g_menu_gfx_semitransparency, g_menu_atlas_texture_page, clut, priority);
    rect.x = left + 5;
    rect.y = top;
    rect.w = partial_width;
    rect.h = 9;
    menu_gfx_enqueue_textured_quad(
        &rect, u_middle, v_top, 0, g_menu_gfx_semitransparency, g_menu_atlas_texture_page, clut, priority);
    for (column = 0; column < full_columns; column++) {
        rect.x = left + (5 + partial_width) + column * 16;
        rect.y = top;
        rect.w = 16;
        rect.h = 9;
        menu_gfx_enqueue_textured_quad(
            &rect, u_middle, v_top, 0, g_menu_gfx_semitransparency, g_menu_atlas_texture_page, clut, priority);
    }
    rect.x = left + (5 + partial_width) + column * 16;
    rect.y = top;
    rect.w = 5;
    rect.h = 9;
    menu_gfx_enqueue_textured_quad(
        &rect, u_right - 5, v_top, 0, g_menu_gfx_semitransparency, g_menu_atlas_texture_page, clut, priority);

    /* Side edges, always from the default border texture: the partial pieces, then one
     * full piece per side for each 16-pixel row. */
    rect.x = left;
    rect.y = top + 9;
    rect.w = 5;
    rect.h = partial_height;
    menu_gfx_enqueue_textured_quad(
        &rect, 2, 8, 0, g_menu_gfx_semitransparency, g_menu_atlas_texture_page, clut, priority);
    rect.x = left + (5 + partial_width) + full_columns * 16;
    rect.y = top + 9;
    rect.w = 5;
    rect.h = partial_height;
    menu_gfx_enqueue_textured_quad(
        &rect, 0x1A, 8, 0, g_menu_gfx_semitransparency, g_menu_atlas_texture_page, clut, priority);
    for (row = 0; row < full_rows; row++) {
        rect.x = left;
        rect.y = top + (9 + partial_height) + row * 16;
        rect.w = 5;
        rect.h = 16;
        menu_gfx_enqueue_textured_quad(
            &rect, 2, 8, 0, g_menu_gfx_semitransparency, g_menu_atlas_texture_page, clut, priority);
        rect.x = left + (5 + partial_width) + full_columns * 16;
        rect.y = top + (9 + partial_height) + row * 16;
        rect.w = 5;
        rect.h = 16;
        menu_gfx_enqueue_textured_quad(
            &rect, 0x1A, 8, 0, g_menu_gfx_semitransparency, g_menu_atlas_texture_page, clut, priority);
    }

    if (command->rectangle_source == 2) {
        g_gfx_tiled_rect.x = g_menu_window_rect_source->rect.x;
        g_gfx_tiled_rect.y = g_menu_window_rect_source->rect.y;
        g_gfx_tiled_rect.w = g_menu_window_rect_source->rect.w;
        g_gfx_tiled_rect.h = g_menu_window_rect_source->rect.h;
    } else {
        g_cmd_window_interior_command.x = command->x;
        g_cmd_window_interior_command.y = command->y;
        g_cmd_window_interior_command.width = command->width;
        g_cmd_window_interior_command.height = command->height;
    }
    menu_opcode_11_draw_tiled_rectangle((menu_instruction_t*)&g_cmd_window_interior_command);

    /* Bottom edge, laid out like the top. */
    top = top + 9 + partial_height + full_rows * 16;
    rect.x = left;
    rect.y = top;
    rect.w = 5;
    rect.h = 7;
    menu_gfx_enqueue_textured_quad(
        &rect, u_left, v_bottom - 7, 0, g_menu_gfx_semitransparency, g_menu_atlas_texture_page, clut, priority);
    rect.x = left + 5;
    rect.y = top;
    rect.w = partial_width;
    rect.h = 7;
    menu_gfx_enqueue_textured_quad(
        &rect, u_middle, v_bottom - 7, 0, g_menu_gfx_semitransparency, g_menu_atlas_texture_page, clut, priority);
    for (column = 0; column < full_columns; column++) {
        rect.x = left + (5 + partial_width) + column * 16;
        rect.y = top;
        rect.w = 16;
        rect.h = 7;
        menu_gfx_enqueue_textured_quad(
            &rect, u_middle, v_bottom - 7, 0, g_menu_gfx_semitransparency, g_menu_atlas_texture_page, clut, priority);
    }
    rect.x = left + (5 + partial_width) + column * 16;
    rect.y = top;
    rect.w = 5;
    rect.h = 7;
    menu_gfx_enqueue_textured_quad(
        &rect, u_right - 5, v_bottom - 7, 0, g_menu_gfx_semitransparency, g_menu_atlas_texture_page, clut, priority);
    command = (menu_window_command_t*)((u8*)command + command->length);
    return command;
}

/* Tile the command rectangle, or the shared rectangle when source is 2. */
menu_instruction_t* menu_opcode_11_draw_tiled_rectangle(menu_instruction_t* command) {
    RECT rect;
    s32 base_x;
    s32 base_y;
    s32 full_rows;
    s32 partial_width;
    s32 partial_height;
    s32 otag_index = g_menu_gfx_otag_index;
    u16 clut = g_menu_palette_clut;
    s32 full_columns;
    s32 row;
    s32 column;

    if (command->window.rectangle_source == 2) {
        base_x = g_gfx_tiled_rect.x;
        base_y = g_gfx_tiled_rect.y;
        full_columns = g_gfx_tiled_rect.w / 16;
        partial_width = g_gfx_tiled_rect.w % 16;
        full_rows = g_gfx_tiled_rect.h / 16;
        partial_height = g_gfx_tiled_rect.h % 16;
    } else {
        u8 packed_width;
        u8 packed_height;

        base_x = command->window.x;
        base_y = command->window.y;
        packed_width = command->window.width;
        packed_height = command->window.height;
        full_columns = packed_width >> 4;
        partial_width = packed_width & 15;
        full_rows = packed_height >> 4;
        partial_height = packed_height & 15;
    }

    for (row = 0; row < full_rows; row++) {
        rect.x = base_x;
        rect.y = base_y + row * 16;
        rect.w = partial_width;
        rect.h = 16;
        menu_gfx_enqueue_textured_quad(
            &rect, 8, 8, 0, g_menu_gfx_semitransparency, g_menu_atlas_texture_page, clut, otag_index);
        for (column = 0; column < full_columns; column++) {
            rect.x = base_x + partial_width + column * 16;
            rect.y = base_y + row * 16;
            rect.w = 16;
            rect.h = 16;
            menu_gfx_enqueue_textured_quad(
                &rect, 8, 8, 0, g_menu_gfx_semitransparency, g_menu_atlas_texture_page, clut, otag_index);
        }
    }

    if (partial_height != 0) {
        rect.x = base_x;
        rect.y = base_y + row * 16;
        rect.w = partial_width;
        rect.h = partial_height;
        menu_gfx_enqueue_textured_quad(
            &rect, 8, 8, 0, g_menu_gfx_semitransparency, g_menu_atlas_texture_page, clut, otag_index);
        for (column = 0; column < full_columns; column++) {
            rect.x = base_x + partial_width + column * 16;
            rect.y = base_y + row * 16;
            rect.w = 16;
            rect.h = partial_height;
            menu_gfx_enqueue_textured_quad(
                &rect, 8, 8, 0, g_menu_gfx_semitransparency, g_menu_atlas_texture_page, clut, otag_index);
        }
    }

    command = (menu_instruction_t*)((u8*)command + command->header.length);
    return command;
}

/* Stream command: set the texture page from the command (color mode
 * data[4], x data[3] * 16, y data[2] * 256, blend rate 0) and return the
 * next command. */
const u8* menu_set_texture_page(const u8* data) {
    g_save_gfx_texture_page = GetTPage(data[4], 0, data[3] << 4, data[2] << 8);
    return data + data[1];
}

/* Menu script command: select the ordering-table slot later primitives go to (byte 3) and return
 * the next command (byte 1 holds this command's length). */
const u8* menu_set_draw_priority(const u8* data) {
    g_menu_gfx_otag_index = data[3];
    return data + data[1];
}

/* Draw the command's literal value as `width` zero-padded digit glyphs from (x, y),
 * `spacing` pixels apart, tinted with the menu text color. A style of 2 or more draws
 * nothing. Returns the next command. */
menu_draw_number_command_t* menu_draw_decimal_number(menu_draw_number_command_t* command) {
    s8 text[16];
    menu_digit_glyph_t glyph;
    RECT unused_minus_rect; /* Preserves the original frame size. */
    s32 y;
    s16 pad;
    s32 pos;
    s32 value;
    s32 digit;
    s32 i;
    u16 clut;
    const rgb24_t* color;

    value = command->value;
    y = command->y;
    clut = g_menu_alternate_number_clut;
    glyph.u = 0x78;
    glyph.v = 0x10;
    glyph.w = 6;
    glyph.h = 0xB;
    pad = command->width - sprintf(text, g_menu_text_decimal_format, value);
    pos = 0;
    color = &g_menu_text_color;
    for (i = 0; i < command->width; i++) {
        glyph.rect.x = command->x + (i * command->spacing);
        glyph.rect.y = y;
        glyph.rect.w = glyph.w;
        glyph.rect.h = glyph.h;
        if (pad != 0) {
            digit = 0;
            pad -= 1;
        } else {
            digit = text[pos] - '0';
            pos += 1;
        }
        if (command->style < 2) {
            menu_gfx_enqueue_textured_quad(&glyph.rect, glyph.u + (glyph.w * digit), glyph.v, color,
                g_menu_gfx_semitransparency, g_menu_atlas_texture_page, clut, g_menu_gfx_otag_index);
        }
    }
    return (menu_draw_number_command_t*)((u8*)command + command->length);
}

/* Advances the animated cursor at `pos` and draws it, sliding horizontally,
 * with its semi-transparent shadow. `mode` bit 0 starts the reverse curve;
 * mode 0 cancels it. The same routine as menu_update_and_draw_animated_cursor, except
 * that it draws at the fixed ordering-table entries 0x3C and 0x3B. */
void menu_update_and_draw_sliding_cursor(point16_t* pos, u16* state, s32 mode) {
    RECT rect;
    s32 active;
    s32 active_bit = *state >> 15;
    s32 timer;
    s32 offset;
    s8 raw_offset;
    s32 i;
    u16 front_clut;
    u16 shadow_clut;

    active = active_bit;
    if (mode == 0) {
        if (active_bit != 0) {
            *state = 0;
            active = 0;
        }
    }
    active_bit = active;
    if (active_bit == 0 && (mode & 1) != 0) {
        *state = 0x8000;
        active = 1;
    }

    timer = *state;
    timer &= 0x7F;
    timer = timer * menu_get_event_speed();
    active_bit = active;
    if (active_bit != 0) {
        if (timer >= 0x25) {
            timer = 0x24;
        }
    } else {
        if (timer >= 0x2F) {
            timer = 0;
        }
    }

    active_bit = active;
    if (active_bit != 0) {
        for (i = 0; g_menu_cursor_reverse_offset_keyframes[i] < timer; i += 2) { }
        raw_offset = g_menu_cursor_reverse_offset_keyframes[i + 1];
    } else {
        for (i = 0; g_menu_cursor_forward_offset_keyframes[i] < timer; i += 2) { }
        raw_offset = g_menu_cursor_forward_offset_keyframes[i + 1];
    }

    offset = raw_offset;
    timer += menu_get_event_speed();
    active_bit = active;
    if (active_bit != 0) {
        if (timer < 0x25) {
            (*state)++;
        }
    } else {
        if (timer < 0x2F) {
            (*state)++;
        } else {
            *state = 0;
        }
    }

    if (mode != 0) {
        front_clut = g_menu_cursor_mode1_foreground_clut;
        shadow_clut = g_menu_cursor_mode1_background_clut;
    } else {
        front_clut = g_menu_cursor_mode0_foreground_clut;
        shadow_clut = g_menu_cursor_mode0_background_clut;
    }

    rect.x = offset + pos->x - 2;
    rect.y = pos->y;
    rect.w = 0x10;
    rect.h = 0x10;
    menu_gfx_enqueue_textured_quad(&rect, 0xA8, 0, 0, 0, g_menu_cursor_texture_page, front_clut, 0x3C);
    rect.x += 2;
    rect.y += 2;
    menu_gfx_enqueue_textured_quad(&rect, 0xB8, 0, 0, 1, 0x5F, shadow_clut, 0x3B);
}
