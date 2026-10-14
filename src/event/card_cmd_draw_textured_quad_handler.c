#include "fft/event_card.h"
#include "fft/menu.h"
#include "psx/types.h"

/* Stream command: draw a textured quad of size (command[5], command[6]) at
 * (command[3], command[4]) with texture origin (command[7], command[8]), in the
 * current sprite color or, for MENU_SCRIPT_OP_UNCOLORED_QUAD, unshaded. Returns the next command. */
u8* card_cmd_draw_textured_quad_handler(u8* command) {
    RECT rect;
    const rgb24_t* color;
    s32 x;
    s32 y;

    x = command[3];
    y = command[4];
    rect.x = x;
    rect.y = y;
    rect.w = command[5];
    rect.h = command[6];
    color = &g_menu_text_color;
    if (command[0] == MENU_SCRIPT_OP_UNCOLORED_QUAD) {
        color = 0;
    }
    card_gfx_enqueue_textured_quad(&rect, command[7], command[8], color, g_card_gfx_draw_semitrans,
        g_card_gfx_texture_page, g_card_gfx_draw_clut, g_card_text_otag_index);
    return command + command[1];
}
