/* The menu command that draws a row callback's icon for a list row. */

#include "fft/menu.h"
#include "fft/menu_input.h"
#include "fft/menu_list.h"
#include "fft/menu_script.h"
#include "psx/types.h"

/* Draw the selected callback's icon for the literal or current list row,
 * using its texture, dimensions and palette. */
menu_instruction_t* menu_cmd_draw_row_callback_quad_handler(menu_instruction_t* cmd) {
    menu_icon_source_t* entry;
    s32 row;
    menu_icon_source_t* (*callback)(s32);

    callback = g_menu_row_callbacks[cmd->callback_sprite.callback_index].icon;
    if (g_menu_list_row_mode == 0) {
        row = cmd->callback_sprite.row;
    } else {
        row = g_menu_scroll_base_index + g_menu_list_row_index;
        if (g_menu_list_scroll_offset < 0) {
            row -= 1;
        }
    }

    entry = callback(row);
    if (entry != 0) {
        g_menu_cmd_row_quad.sprite.x = cmd->callback_sprite.x;
        g_menu_cmd_row_quad.sprite.y = cmd->callback_sprite.y;
        g_menu_cmd_row_quad.sprite.w = entry->w;
        g_menu_cmd_row_quad.sprite.h = entry->h;
        g_menu_cmd_row_quad.sprite.u = entry->u;
        g_menu_cmd_row_quad.sprite.v = entry->v;
        g_menu_gfx_clut_id = entry->clut;
        g_menu_gfx_texture_page = entry->tpage;
        menu_cmd_draw_textured_quad_handler((menu_instruction_t*)&g_menu_cmd_row_quad);
    }
    return menu_next_instruction(cmd);
}
