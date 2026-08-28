#include "fft/equip.h"
#include "psx/types.h"

u8* equip_cmd_draw_row_callback_quad_handler(u8* cmd) {
    equip_row_callback_t fn;
    u8* entry;
    s32 row;
    u16 tpage;
    u16 clut;

    fn = g_equip_menu_list_row_callbacks[cmd[2]];
    if (g_equip_menu_list_row_mode == 0) {
        row = cmd[3];
    } else {
        row = g_equip_menu_scroll_base_index + g_equip_menu_list_row_index;
        if (g_equip_menu_list_scroll_offset < 0) {
            row--;
        }
    }

    entry = (u8*)fn(row);
    if (entry != 0) {
        g_equip_cmd_row_sprite_body[0] = cmd[4];
        g_equip_cmd_row_sprite_body[1] = cmd[5];
        g_equip_cmd_row_sprite_body[2] = entry[4];
        g_equip_cmd_row_sprite_body[3] = entry[6];
        g_equip_cmd_row_sprite_body[4] = entry[0];
        g_equip_cmd_row_sprite_body[5] = entry[2];
        clut = *(u16*)(entry + 8);
        tpage = *(u16*)(entry + 0xa);
        g_equip_gfx_clut_id = clut;
        g_equip_gfx_texture_page = tpage;
        equip_cmd_draw_textured_quad_handler(g_equip_cmd_row_sprite_body - 3);
    }

    return cmd + cmd[1];
}
