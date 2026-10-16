#include "fft/event_equip.h"
#include "fft/gfx.h"
#include "fft/menu.h"
#include "fft/menu_script.h"
#include "psx/libc.h"
#include "psx/libgpu.h"
#include "psx/types.h"

/* Draw record: print a callback-supplied number right-aligned in a field of
 * eight-pixel cells, then advance past the record. `count` holds
 * the value, then the printed length, then the number of digit cells. */
menu_instruction_t* menu_cmd_draw_right_aligned_number_handler_variant(menu_instruction_t* command) {
    s8 buf[0x10];
    RECT rect;
    s16 i;
    s16 x;
    s32 count;
    u8 y;
    s16 blanks;
    s32 j;

    count = g_menu_row_callbacks[command->number.getter].value(command->number.value);
    count = sprintf(buf, g_equip_text_decimal_format, count);
    blanks = command->number.width - count;
    x = command->number.x;
    y = command->number.y;
    count = command->number.width - blanks;
    g_gfx_blank_cell_rect.position.y = y + 4;
    for (i = blanks - 1; i != -1; i--) {
        g_gfx_blank_cell_rect.position.x = x;
        menu_gfx_enqueue_oriented_textured_quad(
            &g_gfx_blank_cell_rect, 0, TEXTURE_UV_FLIP_NONE, 0, g_menu_gfx_otag_index);
        x += 8;
    }
    rect.x = x;
    rect.y = y;
    rect.w = 8;
    rect.h = 0x10;
    for (j = 0; j < count; j++) {
        menu_gfx_enqueue_textured_quad(&rect, (buf[j] * 8) - 0xD8, 0x30, 0, 0, g_menu_cursor_texture_page,
            g_menu_text_clut_1_mode0, g_menu_gfx_otag_index);
        rect.x += 8;
    }
    return menu_next_instruction(command);
}
