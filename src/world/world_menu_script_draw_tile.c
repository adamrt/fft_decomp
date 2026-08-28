#include "fft/world.h"
#include "psx/gpu.h"
#include "psx/types.h"

/* Draws the rectangle described by a 6-byte command header (x, y, w, h)
 * followed by its payload, and returns the next command. */
u8* world_menu_script_draw_tile(u8* cmd) {
    RECT rect;
    s32 y;
    s16 step;

    if (g_world_menu_use_scroll_position == 0) {
        y = cmd[3];
    } else {
        step = g_world_menu_row_height;
        y = step * g_world_menu_scroll_row_offset + cmd[3] + g_world_menu_scroll_pixel_offset;
        if (g_world_menu_scroll_pixel_offset < 0) {
            y -= step;
        }
    }
    rect.x = cmd[2];
    rect.y = y;
    rect.w = cmd[4];
    rect.h = cmd[5];
    /* g_world_menu_semi_trans is an s16 (its writer stores a halfword); this site reads its
     * low byte, which a (u8) cast would render as lh + andi. */
    world_gfx_append_tile_to_otag(&rect, cmd + 6, *(u8*)&g_world_menu_semi_trans, g_world_menu_draw_priority);
    return cmd + cmd[1];
}
