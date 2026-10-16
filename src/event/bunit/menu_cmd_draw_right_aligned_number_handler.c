/* The menu command that draws a row callback's value right-aligned in eight-pixel cells. */

#include "fft/event_bunit.h"
#include "fft/gfx.h"
#include "fft/menu.h"
#include "fft/menu_input.h"
#include "fft/menu_script.h"
#include "psx/libc.h"
#include "psx/libgpu.h"
#include "psx/types.h"

/* Draw the row callback's value right-aligned in a field of `width` 8x16 cells: blank
 * cells on the left, then the digit glyphs (u 0xa8 + 8 per digit, v 0x30 on the cursor
 * texture page). A value with more digits than `width` would start the blank-cell
 * countdown below -1 and run it through the whole 16-bit range. */
menu_instruction_t* bunit_menu_cmd_draw_right_aligned_number_handler(menu_instruction_t* cmd) {
    s8 digits[16];
    RECT glyph_rect;
    s32 count;
    s16 blanks;
    s16 i;
    s32 j;
    s32 x;
    s32 y;

    count = g_menu_row_callbacks[cmd->number.getter].value(cmd->number.value);
    count = sprintf(digits, g_bunit_text_decimal_format, count);
    x = cmd->number.x;
    y = cmd->number.y;
    blanks = cmd->number.width - count;
    count = cmd->number.width - blanks;
    g_gfx_blank_cell_rect.position.y = y + 4;
    for (i = blanks - 1; i != -1; i--) {
        g_gfx_blank_cell_rect.position.x = x;
        menu_gfx_enqueue_oriented_textured_quad(
            &g_gfx_blank_cell_rect, 0, TEXTURE_UV_FLIP_NONE, 0, g_menu_gfx_otag_index);
        x += 8;
    }
    glyph_rect.x = x;
    glyph_rect.y = y;
    glyph_rect.w = 8;
    glyph_rect.h = 0x10;
    for (j = 0; j < count; j++) {
        menu_gfx_enqueue_textured_quad(&glyph_rect, (digits[j] - '0') * 8 + 0xA8, 0x30, 0, 0,
            g_menu_cursor_texture_page, 0x3ED3, g_menu_gfx_otag_index);
        glyph_rect.x += 8;
    }
    return menu_next_instruction(cmd);
}
