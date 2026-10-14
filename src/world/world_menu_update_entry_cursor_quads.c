#include "fft/script.h"
#include "fft/thread.h"
#include "fft/world.h"
#include "psx/libgpu.h"
#include "psx/types.h"

/*
 * Position the two cursor quads to the right of a menu entry's text.
 *
 * Same highlight timer/phase handling as menu_update_icon_cursor_sprites
 * (32-bit thread fields at 0x50/0x54); the quads bob with the blink table value and sit
 * six pixels further left per cursor row. The first quad is the 16x16 arrow,
 * the second its two-pixel shadow offset.
 */
void world_menu_update_entry_cursor_quads(
    menu_entry_t* entry, POLY_FT4* quad, POLY_FT4* shadow, s32 unused, s32 cursor) {
    thread_t* thread = &g_threads[g_thread_current_id];
    s32 x;
    s32 y;

    if (cursor >= 0) {
        if (g_world_formation_display_mode == 1) {
            thread->task_words[1] = 2;
            thread->task_words[0] = 0;
        }
        if (world_thread_is_previous_running() == 0 && g_menu_input_lock_mode == 0) {
            if (thread->task_words[1] != 0 && thread->task_words[0] >= 0x1B) {
                thread->task_words[1] = 0;
                thread->task_words[0] = 0;
            }
        } else if (thread->task_words[1] == 0) {
            thread->task_words[1] = 2;
            thread->task_words[0] = 0;
        }
        if (thread->task_words[1] != 0) {
            y = world_menu_get_cursor_bob_offset(1);
        } else {
            y = world_menu_get_cursor_bob_offset(0);
        }
        thread->task_words[0] += g_script_event_speed;
        x = -(cursor + 1) * 6;
        y -= 0x17;
        setXY4(quad, entry->window.x + (entry->inner_width + x) - 0x13, entry->window.y - y + 4,
            entry->window.x + (entry->inner_width + x) - 3, entry->window.y - y + 4,
            entry->window.x + (entry->inner_width + x) - 0x13, entry->window.y - y + 0x14,
            entry->window.x + (entry->inner_width + x) - 3, entry->window.y - y + 0x14);
        setXY4(shadow, entry->window.x + (entry->inner_width + x) - 0x11, entry->window.y - y + 6,
            entry->window.x + (entry->inner_width + x) - 1, entry->window.y - y + 6,
            entry->window.x + (entry->inner_width + x) - 0x11, entry->window.y - y + 0x16,
            entry->window.x + (entry->inner_width + x) - 1, entry->window.y - y + 0x16);
    }
}
