#include "fft/wldcore.h"

typedef struct wldcore_text_origin {
    s16 x;
    s16 y;
    s32 _unused_04;
    s32 value;
} wldcore_text_origin_t;

/* `point` is passed by value: GCC spills the two register words to their
 * home slots before narrowing them into the origin record. */
void wldcore_menu_display_text_entry(s32 index, s32 text_id, wldcore_point32_t point, void* buffer) {
    wldcore_text_origin_t origin;
    s32 x = point.x;
    s32 y = point.y;

    origin.x = x;
    origin.y = y;
    origin.value = g_wldcore_window_render_records[index].width;
    world_menu_display_text_entry(text_id, buffer, &origin);
}
