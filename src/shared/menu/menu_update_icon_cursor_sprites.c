#include "fft/battle_event.h"
#include "fft/battle_menu.h"
#include "fft/menu.h"
#include "fft/script.h"
#include "fft/thread.h"
#include "psx/libgpu.h"
#include "psx/types.h"

/* Position the icon-list cursor sprites and animate the highlighted icon.
 *
 * The current thread's 32-bit values at 0x50/0x54 hold the highlight timer and phase:
 * the phase restarts at 2 while input is blocked and clears once the timer
 * reaches 27 frames. The cursor sprites follow the selected row. */
void menu_update_icon_cursor_sprites(menu_entry_t* param, menu_icon_sprites_t* record, s32 frame, s32 cursor) {
    thread_t* thread = &g_threads[g_thread_current_id];
    s32 x;
    s32 y;

    if (cursor >= 0) {
        if (g_menu_formation_display_mode == 1) {
            thread->task_words[1] = 2;
            thread->task_words[0] = 0;
        }
        if (battle_thread_is_previous_running() == 0 && g_menu_input_lock_mode == 0) {
            if (thread->task_words[1] != 0 && thread->task_words[0] >= 0x1B) {
                thread->task_words[1] = 0;
                thread->task_words[0] = 0;
            }
        } else if (thread->task_words[1] == 0) {
            thread->task_words[1] = 2;
            thread->task_words[0] = 0;
        }
        if (thread->task_words[1] != 0) {
            x = menu_get_cursor_bob_offset(1);
        } else {
            x = menu_get_cursor_bob_offset(0);
        }
        thread->task_words[0] += g_script_event_speed;
        x -= 12;
        y = cursor * 16 + 10;
        record->sprites[1].x0 = param->window.x + x;
        record->sprites[1].y0 = param->window.y + y;
        record->sprites[2].x0 = param->window.x + x + 2;
        record->sprites[2].y0 = param->window.y + y + 2;
    }
    if (g_script_event_speed == 2) {
        frame *= 2;
    }
    menu_copy_sprite_position_uv_and_size(param, &record->sprites[0], frame);
    menu_zoom_cursor_frame(param, &record->sprites[0], frame);
}
