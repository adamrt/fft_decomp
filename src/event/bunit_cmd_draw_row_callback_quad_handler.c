#include "fft/bunit.h"
#include "psx/types.h"

/* Sprite record returned by a row callback; only the low byte of each
 * u16 coordinate is copied into the textured-quad command. */
typedef struct {
    u8 u;      /* 0x00 */
    u8 u_hi;   /* 0x01 */
    u8 v;      /* 0x02 */
    u8 v_hi;   /* 0x03 */
    u8 w;      /* 0x04 */
    u8 w_hi;   /* 0x05 */
    u8 h;      /* 0x06 */
    u8 h_hi;   /* 0x07 */
    u16 clut;  /* 0x08 */
    u16 tpage; /* 0x0A */
} bunit_cmd_row_sprite_t;

u8* bunit_cmd_draw_row_callback_quad_handler(u8* cmd) {
    bunit_cmd_row_sprite_t* entry;
    s32 row;
    bunit_cmd_row_sprite_t* (*callback)(s32);

    callback = (bunit_cmd_row_sprite_t * (*)(s32)) g_bunit_menu_row_callbacks[cmd[2]];
    if (g_bunit_menu_scroll_list_active == 0) {
        row = cmd[3];
    } else {
        row = g_bunit_menu_scroll_base_index + g_bunit_menu_cursor_row;
        if (g_bunit_menu_scroll_pixel_offset < 0) {
            row -= 1;
        }
    }

    entry = callback(row);
    if (entry != 0) {
        g_bunit_cmd_row_quad[3] = cmd[4];
        g_bunit_cmd_row_quad[4] = cmd[5];
        g_bunit_cmd_row_quad[5] = entry->w;
        g_bunit_cmd_row_quad[6] = entry->h;
        g_bunit_cmd_row_quad[7] = entry->u;
        g_bunit_cmd_row_quad[8] = entry->v;
        g_bunit_gfx_clut_id = entry->clut;
        g_bunit_gfx_texture_page = entry->tpage;
        bunit_cmd_draw_textured_quad_handler(g_bunit_cmd_row_quad);
    }
    return cmd + cmd[1];
}
