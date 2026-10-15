#include "fft/event_equip.h"
#include "fft/menu.h"

/* EQUIP.OUT 001caf14 - Stream command: draw a LINE_F2 from (x0, y0) to
   (x1, y1) with the RGB that follows the endpoints in the command. */
menu_instruction_t* equip_cmd_draw_line_f2_handler(menu_instruction_t* stream) {
    point16_t endpoints[2];

    endpoints[0].x = stream->line.x0;
    endpoints[0].y = stream->line.y0;
    endpoints[1].x = stream->line.x1;
    endpoints[1].y = stream->line.y1;
    gfx_enqueue_line_f2(endpoints, &stream->line.color, g_menu_gfx_semitransparency, g_menu_gfx_otag_index);
    return menu_next_instruction(stream);
}
