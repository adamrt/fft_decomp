#include "fft/data.h"
#include "fft/thread.h"
#include "fft/world.h"
#include "psx/gpu.h"
#include "psx/types.h"

/*
 * Position the two cursor quads to the right of a menu entry's text.
 *
 * Same highlight timer/phase handling as world_menu_update_icon_cursor_sprites
 * (thread words 0x50/0x54); the quads bob with the blink table value and sit
 * six pixels further left per cursor row. The first quad is the 16x16 arrow,
 * the second its two-pixel shadow offset.
 */
void world_menu_update_entry_cursor_quads(
    world_menu_entry_t* entry, POLY_FT4* quad, POLY_FT4* shadow, s32 unused, s32 cursor) {
    world_thread_t* thread = &g_world_threads[g_world_thread_current_id];
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
            y = world_lookup_thread_parameter_threshold_value(1);
        } else {
            y = world_lookup_thread_parameter_threshold_value(0);
        }
        thread->task_words[0] += g_world_event_speed;
        x = -(cursor + 1) * 6;
        y -= 0x17;
        quad->x0 = entry->window_x + (entry->inner_width + x) - 0x13;
        quad->y0 = entry->window_y - y + 4;
        quad->x1 = entry->window_x + (entry->inner_width + x) - 3;
        quad->y1 = entry->window_y - y + 4;
        quad->x2 = entry->window_x + (entry->inner_width + x) - 0x13;
        quad->y2 = entry->window_y - y + 0x14;
        quad->x3 = entry->window_x + (entry->inner_width + x) - 3;
        quad->y3 = entry->window_y - y + 0x14;
        shadow->x0 = entry->window_x + (entry->inner_width + x) - 0x11;
        shadow->y0 = entry->window_y - y + 6;
        shadow->x1 = entry->window_x + (entry->inner_width + x) - 1;
        shadow->y1 = entry->window_y - y + 6;
        shadow->x2 = entry->window_x + (entry->inner_width + x) - 0x11;
        shadow->y2 = entry->window_y - y + 0x16;
        shadow->x3 = entry->window_x + (entry->inner_width + x) - 1;
        shadow->y3 = entry->window_y - y + 0x16;
    }
}
