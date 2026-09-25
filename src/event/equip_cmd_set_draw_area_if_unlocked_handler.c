#include "fft/event_equip.h"
#include "psx/types.h"

/* Stream command: enqueue a draw-area rect (x, y, w, h from the command)
   unless the zoom transition already emitted one. */
u8* equip_cmd_set_draw_area_if_unlocked_handler(u8* cmd) {
    RECT rect;

    if (g_equip_gfx_zoom_draw_area_active == 0) {
        rect.x = cmd[2];
        rect.y = cmd[3];
        rect.w = cmd[4];
        rect.h = cmd[5];
        equip_gfx_enqueue_draw_area(&rect, g_equip_gfx_sprite_ot_index - 1);
    }
    return cmd + cmd[1];
}
