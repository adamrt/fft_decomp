#include "fft/event_bunit.h"
#include "psx/types.h"

void bunit_cmd_set_zoomed_draw_area_handler(world_menu_window_command_t* command) {
    RECT area;
    s32 frame;
    s32 scale;

    frame = g_bunit_gfx_transition_frame;
    if (frame < 4) {
        if (frame < 3) {
            g_bunit_cmd_stream_input = 0;
        }
        scale = g_bunit_gfx_window_zoom_percent[frame];
        area.w = command->width * scale / 100;
        area.h = command->height * scale / 100;
        area.x = command->x + (command->width >> 1) - (area.w >> 1);
        area.y = command->y + (command->height >> 1) - (area.h >> 1);
        bunit_gfx_enqueue_draw_area(&area, g_bunit_gfx_otag_index - 1);
        g_bunit_gfx_otag_index_locked = 1;
    } else {
        g_bunit_gfx_otag_index_locked = 0;
    }
    bunit_cmd_draw_window_frame_handler(command);
}
