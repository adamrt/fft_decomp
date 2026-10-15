#include "fft/menu.h"
#include "fft/script.h"
#include "fft/wldcore.h"
#include "psx/types.h"

/* Clear a render record's text image, draw text `text_id` centerd
 * horizontally in it (nothing for id 0) and upload it. Also clears the
 * three pending sound script variables. */
void wldcore_window_draw_centered_text(s32 window_index, s32 text_id) {
    point32_t point;
    s16 text_width;
    s16 text_height;
    s32 i;

    text_measure_pixels(&text_width, &text_height, text_find_entry(text_id));
    point.x = (((g_wldcore_window_render_records[window_index].bounds.w - text_width) / 2) & 0xfffe) + 8;
    point.y = 10;
    for (i = 0; i < g_wldcore_window_render_records[window_index].bounds.w
            * g_wldcore_window_render_records[window_index].bounds.h / 8;
        i++) {
        g_wldcore_window_image_buffer[i] = 0;
    }
    if (text_id != 0) {
        window_display_text_entry_variant(window_index, text_id, point, g_wldcore_window_image_buffer);
    }
    wldcore_window_load_image_record_to_vram(window_index, g_wldcore_window_image_buffer);
    script_set_variable(EVENT_SCRIPT_VAR_PENDING_SOUND_EFFECT, 0);
    script_set_variable(EVENT_SCRIPT_VAR_PENDING_WEATHER_SOUND, 0);
    script_set_variable(EVENT_SCRIPT_VAR_PENDING_MUSIC_TRACK, 0);
}
