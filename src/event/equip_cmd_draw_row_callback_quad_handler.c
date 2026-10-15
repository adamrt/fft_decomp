#include "fft/event_equip.h"
#include "fft/menu.h"
#include "fft/menu_script.h"
#include "psx/types.h"

/* Draw the selected callback's icon for the literal or current list row,
 * using its texture, dimensions and palette. */
menu_instruction_t* equip_cmd_draw_row_callback_quad_handler(menu_instruction_t* cmd) {
    menu_icon_source_t* (*fn)(s32 row);
    menu_icon_source_t* entry;
    s32 row;
    u16 tpage;
    u16 clut;

    fn = g_menu_row_callbacks[cmd->callback_sprite.callback_index].icon;
    if (g_menu_list_row_mode == 0) {
        row = cmd->callback_sprite.row;
    } else {
        row = g_menu_scroll_base_index + g_menu_list_row_index;
        if (g_menu_list_scroll_offset < 0) {
            row--;
        }
    }

    entry = fn(row);
    if (entry != 0) {
        g_equip_cmd_row_sprite_command.sprite.x = cmd->callback_sprite.x;
        g_equip_cmd_row_sprite_command.sprite.y = cmd->callback_sprite.y;
        g_equip_cmd_row_sprite_command.sprite.w = entry->w;
        g_equip_cmd_row_sprite_command.sprite.h = entry->h;
        g_equip_cmd_row_sprite_command.sprite.u = entry->u;
        g_equip_cmd_row_sprite_command.sprite.v = entry->v;
        clut = entry->clut;
        tpage = entry->tpage;
        g_menu_gfx_clut_id = clut;
        g_menu_gfx_texture_page = tpage;
        menu_cmd_draw_textured_quad_handler((menu_instruction_t*)&g_equip_cmd_row_sprite_command);
    }

    return menu_next_instruction(cmd);
}
