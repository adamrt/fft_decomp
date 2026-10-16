/* The JOBSTTS screen's per-frame input update, withholding input during transitions and help text. */

#include "fft/main_sound.h"
#include "fft/menu_input.h"
#include "fft/menu_window.h"
#include "fft/sound.h"
#include "fft/text.h"
#include "fft/thread.h"
#include "psx/pad.h"
#include "psx/types.h"

/*
 * Per-frame input update of the JOBSTTS screen.
 *
 * Reads the pad, then clears all three 32-bit input values during frames 1..7 of the
 * screen transition, while the text thread runs and on the frame it ends.
 * The text state goes 1 on the thread's first frame (which queues the
 * window-open sound), 2 after it and 0 once it has ended. Any repeat input
 * starts a five-frame timer during which L2 is masked out.
 */
void jobstts_menu_input_update_with_message_state(void) {
    s32 running = 0;
    s32 frame;

    menu_input_update_controller();
    frame = menu_get_window_scale_step();
    if (frame < 1 || frame > 7) {
        running = thread_is_running(THREAD_SCREEN_SLOT_TEXT);
        if (running != 0) {
            g_menu_input_newly_pressed = 0;
            g_menu_input_primary_repeat = 0;
            g_menu_input_secondary_repeat = 0;
            g_text_thread_running_state = (g_text_thread_running_state != 0) ? 2 : 1;
        } else if (g_text_thread_running_state != 0) {
            g_text_thread_running_state = 0;
            g_menu_input_newly_pressed = 0;
            g_menu_input_primary_repeat = 0;
            g_menu_input_secondary_repeat = 0;
        }
    } else {
        g_menu_input_newly_pressed = 0;
        g_menu_input_primary_repeat = 0;
        g_menu_input_secondary_repeat = 0;
    }
    if (g_input_lock_timer != 0) {
        g_input_lock_timer--;
        g_menu_input_primary_repeat &= ~PSX_PAD_L2;
        g_menu_input_secondary_repeat &= ~PSX_PAD_L2;
        g_menu_input_newly_pressed &= ~PSX_PAD_L2;
    }
    if (g_menu_input_primary_repeat != 0) {
        g_input_lock_timer = 5;
    }
    if (running == 0) {
        g_menu_input_lock_mode = 0;
    }
    if (g_text_thread_running_state == 1) {
        g_sound_queued_effect_id = MAIN_SFX_WINDOW_OPEN;
    }
}
