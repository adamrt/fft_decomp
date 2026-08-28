#include "fft/script_variables.h"
#include "fft/wldcore.h"
#include "fft/world.h"
#include "psx/types.h"

void wldcore_window_draw_centered_text(s32 window_index, s32 text_id) {
    wldcore_point32_t point;
    s16 text_width;
    s16 text_height;
    s32 i;

    world_text_measure_pixels(&text_width, &text_height, world_text_find_entry(text_id));
    i = 0;
    point.x = (((g_wldcore_window_render_record_widths[window_index].value - text_width) / 2) & 0xfffe) + 8;
    point.y = 10;
    for (; i < g_wldcore_window_render_record_widths[window_index].value
            * g_wldcore_window_render_record_heights[window_index].value / 8;
        i++) {
        g_wldcore_window_image_buffer[i] = 0;
    }
    if (text_id != 0) {
        wldcore_menu_display_text_entry(window_index, text_id, point, g_wldcore_window_image_buffer);
    }
    wldcore_window_load_image_record_to_vram(window_index, g_wldcore_window_image_buffer);
    world_script_set_variable(EVENT_SCRIPT_VAR_PENDING_SOUND_EFFECT, 0);
    world_script_set_variable(EVENT_SCRIPT_VAR_PENDING_WEATHER_SOUND, 0);
    world_script_set_variable(EVENT_SCRIPT_VAR_PENDING_MUSIC_TRACK, 0);
}
