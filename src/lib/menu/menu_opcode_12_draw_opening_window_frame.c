/* The menu command that draws a window frame zooming open behind a cropped draw area. */

#include "fft/gfx_primitive.h"
#include "fft/menu_gfx.h"
#include "fft/menu_script.h"
#include "fft/menu_window.h"
#include "psx/libgpu.h"
#include "psx/types.h"

/* Stream command for a window frame that zooms open: during the first four
 * transition frames it first queues a draw area cropped about the window's
 * center to the zoom table's percentage of its size (and, for the first
 * three, withholds the stream's input), then draws the frame through
 * menu_opcode_00_draw_window_frame. */
menu_instruction_t* menu_opcode_12_draw_opening_window_frame(menu_instruction_t* command) {
    RECT area;
    s32 frame = g_gfx_transition_frame;
    s32 scale;

    if (frame < 4) {
        if (frame < 3) {
            g_menu_script_input = 0;
        }
        scale = g_gfx_window_zoom_percent[frame];
        area.w = command->window.width * scale / 100;
        area.h = command->window.height * scale / 100;
        area.x = command->window.x + (command->window.width >> 1) - (area.w >> 1);
        area.y = command->window.y + (command->window.height >> 1) - (area.h >> 1);
        menu_gfx_enqueue_draw_area(&area, g_menu_gfx_otag_index - 1);
        g_gfx_otag_index_locked = 1;
    } else {
        g_gfx_otag_index_locked = 0;
    }
    return menu_opcode_00_draw_window_frame(command);
}
