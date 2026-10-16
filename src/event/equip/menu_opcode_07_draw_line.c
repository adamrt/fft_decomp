/* The menu command that draws a flat-colored line. */

#include "fft/event_equip.h"
#include "fft/gfx.h"
#include "fft/gfx_primitive.h"
#include "fft/menu_gfx.h"
#include "fft/menu_script.h"

/* Stream command: draw a LINE_F2 from (x0, y0) to
   (x1, y1) with the RGB that follows the endpoints in the command. */
menu_instruction_t* equip_menu_opcode_07_draw_line(menu_instruction_t* stream) {
    point16_t endpoints[2];

    endpoints[0].x = stream->line.x0;
    endpoints[0].y = stream->line.y0;
    endpoints[1].x = stream->line.x1;
    endpoints[1].y = stream->line.y1;
    gfx_enqueue_line_f2(endpoints, &stream->line.color, g_menu_gfx_semitransparency, g_menu_gfx_otag_index);
    return menu_next_instruction(stream);
}
