#include "fft/equip.h"
#include "psx/types.h"

/* EQUIP.OUT 001caf14 - Stream command: draw a LINE_F2 from (x0, y0) to
   (x1, y1) with the RGB that follows the endpoints in the command. */
u8* equip_cmd_draw_line_f2_handler(u8* stream) {
    s16 endpoints[4];

    endpoints[0] = stream[2];
    endpoints[1] = stream[3];
    endpoints[2] = stream[4];
    endpoints[3] = stream[5];
    equip_gfx_enqueue_line_f2(
        (const RECT*)endpoints, stream + 6, (u8)g_equip_gfx_semitransparency, g_equip_gfx_sprite_ot_index);
    return stream + stream[1];
}
