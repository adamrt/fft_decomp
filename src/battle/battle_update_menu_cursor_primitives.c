#include "fft/battle.h"
#include "fft/event.h"
#include "fft/thread.h"
#include "fft/world.h"
#include "psx/gpu.h"
#include "psx/types.h"

extern void battle_menu_copy_cursor_frame_data();

/* Position the icon-list cursor sprites and animate the highlighted icon.
 *
 * The current thread's words 0x50/0x54 hold the highlight timer and phase:
 * the phase restarts at 2 while input is blocked and clears once the timer
 * reaches 27 frames. The cursor sprites follow the selected row. */
void battle_update_menu_cursor_primitives(
    world_menu_icon_thread_param_t* param, world_menu_icon_sprites_t* record, s32 frame, s32 cursor) {
    native_thread_t* thread = &g_battle_threads[g_battle_current_thread_id];
    s32 x;
    s32 y;

    if (cursor >= 0) {
        if (g_battle_formation_display_mode == 1) {
            thread->task_words[1] = 2;
            thread->task_words[0] = 0;
        }
        if (battle_thread_is_previous_running() == 0 && g_event_mode == 0) {
            if (thread->task_words[1] != 0 && thread->task_words[0] >= 0x1B) {
                thread->task_words[1] = 0;
                thread->task_words[0] = 0;
            }
        } else if (thread->task_words[1] == 0) {
            thread->task_words[1] = 2;
            thread->task_words[0] = 0;
        }
        if (thread->task_words[1] != 0) {
            x = battle_menu_get_cursor_bob_offset(1);
        } else {
            x = battle_menu_get_cursor_bob_offset(0);
        }
        thread->task_words[0] += g_battle_event_speed;
        x -= 12;
        y = cursor * 16 + 10;
        record->sprites[1].x0 = param->icon_x + x;
        record->sprites[1].y0 = param->flags + y;
        record->sprites[2].x0 = param->icon_x + x + 2;
        record->sprites[2].y0 = param->flags + y + 2;
    }
    if (g_battle_event_speed == 2) {
        frame *= 2;
    }
    battle_menu_copy_cursor_frame_data(param, &record->sprites[0], frame);
    battle_menu_zoom_cursor_frame(param, &record->sprites[0], frame);
}
