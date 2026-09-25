#include "fft/world.h"
#include "psx/gpu.h"
#include "psx/types.h"

/* Draws the sprite described by a 9-byte command header (x, y, w, h, u, v)
 * and returns the next command. */
u8* world_menu_script_draw_sprite(u8* cmd) {
    RECT rect;
    s32 y;
    s16 step;
    CVECTOR* color;

    if (g_world_menu_use_scroll_position == 0) {
        y = cmd[4];
    } else {
        step = g_world_menu_row_height;
        y = step * g_world_menu_scroll_row_offset + cmd[4] - g_world_menu_scroll_pixel_offset;
        if (g_world_menu_scroll_pixel_offset < 0) {
            y -= step;
        }
    }
    rect.x = cmd[3];
    rect.y = y;
    rect.w = cmd[5];
    rect.h = cmd[6];
    if (cmd[0] == 4) {
        color = 0;
    } else {
        color = &g_world_menu_sprite_color;
    }
    world_gfx_enqueue_textured_quad(&rect, cmd[7], cmd[8], color, g_world_menu_semi_trans, g_world_menu_texture_page,
        g_world_menu_clut, g_world_menu_draw_priority);
    return cmd + cmd[1];
}
