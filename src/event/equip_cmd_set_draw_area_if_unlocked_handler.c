#include "fft/equip.h"
#include "psx/types.h"

/* Stream command: enqueue a draw-area rect (x, y, w, h from the command)
   unless the zoom transition already emitted one. */
u8* equip_cmd_set_draw_area_if_unlocked_handler(u8* cmd) {
    s16 rect[4];

    if (g_equip_gfx_zoom_draw_area_active == 0) {
        rect[0] = cmd[2];
        rect[1] = cmd[3];
        rect[2] = cmd[4];
        rect[3] = cmd[5];
        equip_gfx_enqueue_draw_area((RECT*)rect, g_equip_gfx_sprite_ot_index - 1);
    }
    return cmd + cmd[1];
}
