#include "fft/event_jobstts.h"
#include "psx/pad.h"

void jobstts_input_update_with_message_state(void) {
    s32 running;
    s32 frame;

    running = 0;
    jobstts_update_controller_input();
    frame = jobstts_gfx_get_transition_frame();
    if ((u32)(frame - 1) >= 7) {
        running = battle_thread_is_running(1);
        if (running != 0) {
            g_jobstts_input_newly_pressed = 0;
            g_jobstts_input_primary_repeat = 0;
            g_jobstts_input_secondary_repeat = 0;
            g_jobstts_text_thread_running_state = (g_jobstts_text_thread_running_state != 0) ? 2 : 1;
        } else if (g_jobstts_text_thread_running_state != 0) {
            g_jobstts_text_thread_running_state = 0;
            g_jobstts_input_newly_pressed = 0;
            g_jobstts_input_primary_repeat = 0;
            g_jobstts_input_secondary_repeat = 0;
        }
    } else {
        g_jobstts_input_newly_pressed = 0;
        g_jobstts_input_primary_repeat = 0;
        g_jobstts_input_secondary_repeat = 0;
    }
    if (g_jobstts_input_lock_timer != 0) {
        g_jobstts_input_lock_timer--;
        g_jobstts_input_primary_repeat &= ~PSX_PAD_L2;
        g_jobstts_input_secondary_repeat &= ~PSX_PAD_L2;
        g_jobstts_input_newly_pressed &= ~PSX_PAD_L2;
    }
    if (g_jobstts_input_primary_repeat != 0) {
        g_jobstts_input_lock_timer = 5;
    }
    if (running == 0) {
        g_event_mode = 0;
    }
    if (g_jobstts_text_thread_running_state == 1) {
        g_jobstts_sound_queued_effect_id = MAIN_SFX_WINDOW_OPEN;
    }
}
