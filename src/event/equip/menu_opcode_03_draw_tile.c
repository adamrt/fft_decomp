/* The menu command that draws a flat-colored tile, following the list row while one is rendered. */

#include "fft/gfx_primitive.h"
#include "fft/menu_gfx.h"
#include "fft/menu_script.h"
#include "psx/libgpu.h"
#include "psx/types.h"

/* Stream command: draw a flat tile of size (cmd->tile.width, cmd->tile.height) at
 * (cmd->tile.x, cmd->tile.y) in cmd->tile.color. While a list row is
 * rendered, y is offset by the row's position and the scroll offset. Returns
 * the next command. */
menu_instruction_t* menu_opcode_03_draw_tile(menu_instruction_t* cmd) {
    RECT rect;
    s32 y;
    s32 base;

    if (g_menu_list_row_mode == 0) {
        y = cmd->tile.y;
    } else {
        base = g_menu_list_row_height * g_menu_list_row_index + cmd->tile.y;
        y = base + g_menu_list_scroll_offset;
        if (g_menu_list_scroll_offset < 0) {
            y = y - g_menu_list_row_height;
        }
    }
    rect.x = cmd->tile.x;
    rect.y = y;
    rect.w = cmd->tile.width;
    rect.h = cmd->tile.height;
    gfx_enqueue_tile(&rect, &cmd->tile.color, g_menu_gfx_semitransparency, g_menu_gfx_otag_index);
    return menu_next_instruction(cmd);
}
