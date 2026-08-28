#include "fft/equip.h"
#include "psx/types.h"

u8* equip_cmd_draw_translucent_tile_handler(u8* cmd) {
    s16 rect[4];
    s32 y;
    s32 base;

    if (g_equip_menu_list_row_mode == 0) {
        y = cmd[3];
    } else {
        base = g_equip_menu_list_row_height * g_equip_menu_list_row_index + cmd[3];
        y = base + g_equip_menu_list_scroll_offset;
        if (g_equip_menu_list_scroll_offset < 0) {
            y = y - g_equip_menu_list_row_height;
        }
    }
    rect[0] = cmd[2];
    rect[1] = y;
    rect[2] = cmd[4];
    rect[3] = cmd[5];
    equip_gfx_enqueue_translucent_tile(
        (const RECT*)rect, cmd + 6, (u8)g_equip_gfx_semitransparency, g_equip_gfx_sprite_ot_index);
    return cmd + cmd[1];
}
