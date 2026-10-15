#include "fft/event_equip.h"
#include "fft/menu.h"
#include "fft/menu_script.h"
#include "psx/types.h"

/* Stream command: draw a flat tile of size (cmd->tile.width, cmd->tile.height) at
 * (cmd->tile.x, cmd->tile.y) in cmd->tile.color. While a list row is
 * rendered, y is offset by the row's position and the scroll offset. Returns
 * the next command. */
menu_instruction_t* equip_cmd_draw_translucent_tile_handler(menu_instruction_t* cmd) {
    RECT rect;
    s32 y;
    s32 base;

    if (g_menu_list_row_mode == 0) {
        y = cmd->tile.y;
    } else {
        base = g_equip_menu_list_row_height * g_menu_list_row_index + cmd->tile.y;
        y = base + g_menu_list_scroll_offset;
        if (g_menu_list_scroll_offset < 0) {
            y = y - g_equip_menu_list_row_height;
        }
    }
    rect.x = cmd->tile.x;
    rect.y = y;
    rect.w = cmd->tile.width;
    rect.h = cmd->tile.height;
    gfx_enqueue_tile(&rect, &cmd->tile.color, g_menu_gfx_semitransparency, g_menu_gfx_otag_index);
    return menu_next_instruction(cmd);
}
