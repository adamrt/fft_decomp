#include "fft/world.h"
#include "psx/gpu.h"
#include "psx/types.h"

extern void world_gfx_copy_sprite_position_uv_and_size();

/*
 * Position the icon-list cursor sprites and animate the highlighted icon.
 *
 * The current thread's words 0x50/0x54 hold the highlight timer and phase:
 * the phase restarts (2) while input is blocked and clears once the timer
 * reaches 27 frames. The cursor sprites follow the selected row.
 */
void world_menu_update_icon_cursor_sprites(
    world_menu_icon_thread_param_t* param, world_menu_icon_sprites_t* record, s32 frame, s32 cursor) {
    native_thread_t* thread = &g_world_threads[g_world_thread_current_id];
    s32 x;
    s32 y;

    if (cursor >= 0) {
        if (g_world_formation_display_mode == 1) {
            thread->task_words[1] = 2;
            thread->task_words[0] = 0;
        }
        if (world_thread_is_previous_running() == 0 && g_world_thread_task_active == 0) {
            if (thread->task_words[1] != 0 && thread->task_words[0] >= 0x1B) {
                thread->task_words[1] = 0;
                thread->task_words[0] = 0;
            }
        } else if (thread->task_words[1] == 0) {
            thread->task_words[1] = 2;
            thread->task_words[0] = 0;
        }
        if (thread->task_words[1] != 0) {
            x = world_lookup_thread_parameter_threshold_value(1);
        } else {
            x = world_lookup_thread_parameter_threshold_value(0);
        }
        thread->task_words[0] += g_world_event_speed;
        x -= 12;
        y = cursor * 16 + 10;
        record->sprites[1].x0 = param->icon_x + x;
        record->sprites[1].y0 = param->flags + y;
        record->sprites[2].x0 = param->icon_x + x + 2;
        record->sprites[2].y0 = param->flags + y + 2;
    }
    if (g_world_event_speed == 2) {
        frame *= 2;
    }
    world_gfx_copy_sprite_position_uv_and_size(param, &record->sprites[0], frame);
    world_menu_zoom_cursor_frame(param, &record->sprites[0], frame);
}
