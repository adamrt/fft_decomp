#include "fft/event_bunit.h"
#include "psx/libc.h"
#include "psx/types.h"

/* Draw record: print a callback-supplied number right-aligned in a field of
 * `cmd[4]` eight-pixel cells, then advance past the record. */
u8* bunit_cmd_draw_right_aligned_number_handler(u8* cmd) {
    s8 digits[16];
    RECT glyph_rect;
    s32 length;
    s32 count;
    s16 blanks;
    s16 i;
    s32 j;
    s32 x;
    s32 y;

    length = sprintf((char*)digits, g_bunit_text_decimal_format, g_bunit_menu_row_callbacks[cmd[3]](cmd[9]));
    x = cmd[7];
    y = cmd[8];
    blanks = cmd[4] - length;
    count = cmd[4] - blanks;
    g_bunit_gfx_blank_cell_rect.y = y + 4;
    for (i = blanks - 1; i != -1; i--) {
        g_bunit_gfx_blank_cell_rect.x = x;
        bunit_gfx_enqueue_oriented_textured_quad(&g_bunit_gfx_blank_cell_rect, 0, 0, 0, g_bunit_gfx_otag_index);
        x += 8;
    }
    glyph_rect.x = x;
    glyph_rect.y = y;
    glyph_rect.w = 8;
    glyph_rect.h = 0x10;
    for (j = 0; j < count; j++) {
        bunit_gfx_enqueue_textured_quad(&glyph_rect, (digits[j] - '0') * 8 + 0xA8, 0x30, 0, 0,
            g_bunit_menu_icon_texture_page, 0x3ED3, g_bunit_gfx_otag_index);
        glyph_rect.x += 8;
    }
    /* Four extra uses raise count's allocation priority above cmd's, giving it s2 as in the target. */
    __asm__("" : : "r"(count), "r"(count), "r"(count), "r"(count));
    return cmd + cmd[1];
}
