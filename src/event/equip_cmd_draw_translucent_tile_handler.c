#include "fft/event_equip.h"
#include "psx/types.h"

u8* equip_cmd_draw_translucent_tile_handler(u8* cmd) {
    RECT rect;
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
    rect.x = cmd[2];
    rect.y = y;
    rect.w = cmd[4];
    rect.h = cmd[5];
    equip_gfx_enqueue_translucent_tile(&rect, cmd + 6, (u8)g_equip_gfx_semitransparency, g_equip_gfx_sprite_ot_index);
    return cmd + cmd[1];
}
