#include "fft/event_card.h"
#include "psx/types.h"

u8* card_cmd_draw_textured_quad_handler(u8* command) {
    RECT rect;
    const u8* color;
    s32 x;
    s32 y;

    x = command[3];
    y = command[4];
    rect.x = x;
    rect.y = y;
    rect.w = command[5];
    rect.h = command[6];
    color = g_card_text_color;
    if (command[0] == 4) {
        color = 0;
    }
    card_gfx_enqueue_textured_quad(&rect, command[7], command[8], color, g_card_gfx_draw_semitrans,
        g_card_gfx_texture_page, g_card_gfx_draw_clut, g_card_text_otag_index);
    return command + command[1];
}
