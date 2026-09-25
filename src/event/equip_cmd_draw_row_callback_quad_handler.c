#include "fft/equip.h"
#include "psx/types.h"

/* Low bytes of the four halfword coordinates shared by icon and item row descriptors. */
typedef struct equip_cmd_row_quad_source {
    u8 x_low;
    u8 x_high;
    u8 y_low;
    u8 y_high;
    u8 width_low;
    u8 width_high;
    u8 height_low;
    u8 height_high;
    u16 clut;
    u16 tpage;
} equip_cmd_row_quad_source_t;

u8* equip_cmd_draw_row_callback_quad_handler(u8* cmd) {
    equip_row_callback_t fn;
    equip_cmd_row_quad_source_t* entry;
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

    entry = (equip_cmd_row_quad_source_t*)fn(row);
    if (entry != 0) {
        g_equip_cmd_row_sprite_body[0] = cmd[4];
        g_equip_cmd_row_sprite_body[1] = cmd[5];
        g_equip_cmd_row_sprite_body[2] = entry->width_low;
        g_equip_cmd_row_sprite_body[3] = entry->height_low;
        g_equip_cmd_row_sprite_body[4] = entry->x_low;
        g_equip_cmd_row_sprite_body[5] = entry->y_low;
        clut = entry->clut;
        tpage = entry->tpage;
        g_equip_gfx_clut_id = clut;
        g_equip_gfx_texture_page = tpage;
        equip_cmd_draw_textured_quad_handler(g_equip_cmd_row_sprite_body - 3);
    }

    return cmd + cmd[1];
}
