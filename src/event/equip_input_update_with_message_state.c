#include "fft/event_equip.h"
#include "fft/menu.h"
#include "fft/thread.h"
#include "psx/pad.h"
#include "psx/types.h"

/*
 * Per-frame input update of the EQUIP screen.
 *
 * Reads the pad, then clears all three 32-bit input values during frames 1..3 of the
 * screen transition and while the text thread runs. The text state goes 1 on
 * the thread's first frame (which queues the window-open sound), 2 after it
 * and 0 once it has ended, when the message and sound locks are released.
 * Any repeat input starts a five-frame timer during which L2 is masked out.
 */
void equip_input_update_with_message_state(void) {
    s32 running;
    s32 frame;
    u8 state;
    s32 delay;

    running = 0;
    menu_input_update_controller();
    frame = equip_gfx_get_transition_frame();
    if (frame >= 1 && frame <= 3) {
        menu_input_clear_state();
    } else {
        running = thread_is_running(THREAD_SCREEN_SLOT_TEXT);
        if (running != 0) {
            menu_input_clear_state();
            if (g_equip_text_thread_running_state != 0) {
                state = 2;
            } else {
                state = 1;
            }
            g_equip_text_thread_running_state = state;
        } else {
            g_equip_text_thread_running_state = 0;
        }
    }

    delay = g_equip_input_lock_timer;
    if (delay != 0) {
        g_equip_input_lock_timer = delay - 1;
        g_menu_input_primary_repeat &= ~PSX_PAD_L2;
        g_menu_input_secondary_repeat &= ~PSX_PAD_L2;
        g_menu_input_newly_pressed &= ~PSX_PAD_L2;
    }
    if (g_menu_input_primary_repeat != 0) {
        g_equip_input_lock_timer = 5;
    }
    if (running == 0) {
        g_equip_text_message_thread_active = 0;
        g_menu_input_lock_mode = 0;
        g_equip_sound_suppress_queued = 0;
    }
    if (g_equip_text_thread_running_state == 1) {
        g_sound_queued_effect_id = MAIN_SFX_WINDOW_OPEN;
    }
}
