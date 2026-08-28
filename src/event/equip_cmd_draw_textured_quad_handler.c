#include "fft/equip.h"
#include "psx/types.h"

u8* equip_cmd_draw_textured_quad_handler(u8* cmd) {
    s16 rect[4];
    s32 y;
    u8* color;

    if (g_equip_menu_list_row_mode == 0) {
        y = cmd[4];
    } else {
        y = g_equip_menu_list_row_height * g_equip_menu_list_row_index + cmd[4] - g_equip_menu_list_scroll_offset;
        if (g_equip_menu_list_scroll_offset < 0) {
            y -= g_equip_menu_list_row_height;
        }
    }

    rect[0] = cmd[3];
    rect[1] = y;
    rect[2] = cmd[5];
    rect[3] = cmd[6];

    color = g_equip_gfx_sprite_color;
    if (cmd[0] == 4) {
        color = 0;
    }

    /* The local rect is an s16[4] and the two page/clut arguments are passed as
     * full words here; calling through this file's original prototype keeps the
     * target's argument setup, which the definition's narrow u16 formals would
     * truncate at the call. */
    ((void (*)(s16*, s32, s32, u8*, s32, s32, s32, s32))equip_gfx_enqueue_textured_quad)(rect, cmd[7], cmd[8], color,
        g_equip_gfx_semitransparency, g_equip_gfx_texture_page, g_equip_gfx_clut_id, g_equip_gfx_sprite_ot_index);
    return cmd + cmd[1];
}
