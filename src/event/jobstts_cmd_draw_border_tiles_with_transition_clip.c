#include "fft/event_jobstts.h"
#include "fft/menu.h"

/* Draw the border tiles through a vertically expanding transition clip. */
menu_instruction_t* jobstts_cmd_draw_border_tiles_with_transition_clip(menu_instruction_t* command) {
    menu_instruction_t* next;
    RECT rect;

    if (g_jobstts_gfx_transition_frame < 8) {
        s32 scale;

        if (g_jobstts_gfx_transition_frame < 3) {
            g_jobstts_cmd_stream_input = 0;
        }
        scale = g_jobstts_gfx_transition_height_percent[g_jobstts_gfx_transition_frame];
        rect.w = 0x100;
        rect.h = (command->window.height * scale) / 100;
        rect.x = 0;
        rect.y = (command->window.y + (command->window.height >> 1)) - (rect.h >> 1);
        menu_gfx_enqueue_draw_area(&rect, g_menu_gfx_otag_index);
        g_jobstts_gfx_otag_index_locked = 1;
        g_menu_gfx_otag_index = g_menu_gfx_otag_index + 1;
    } else {
        g_jobstts_gfx_otag_index_locked = 0;
    }
    next = jobstts_cmd_draw_window_frame_handler(command);
    g_menu_gfx_otag_index++;
    return next;
}
