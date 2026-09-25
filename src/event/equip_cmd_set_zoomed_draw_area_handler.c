#include "fft/event_equip.h"
#include "psx/types.h"

void equip_cmd_set_zoomed_draw_area_handler(world_menu_window_command_t* command) {
    RECT area;
    s32 frame;
    s32 scale;

    frame = g_equip_gfx_transition_frame;
    if (frame < 4) {
        if (frame < 3) {
            g_equip_cmd_stream_input = 0;
        }
        scale = g_equip_gfx_window_zoom_percent[frame];
        area.w = command->width * scale / 100;
        area.h = command->height * scale / 100;
        area.x = command->x + (command->width >> 1) - (area.w >> 1);
        area.y = command->y + (command->height >> 1) - (area.h >> 1);
        equip_gfx_enqueue_draw_area(&area, g_equip_gfx_sprite_ot_index - 1);
        g_equip_gfx_zoom_draw_area_active = 1;
    } else {
        g_equip_gfx_zoom_draw_area_active = 0;
    }
    equip_cmd_draw_window_frame_handler(command);
}
