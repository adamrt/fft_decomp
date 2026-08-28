#include "fft/battle.h"
#include "fft/equip.h"
#include "fft/main_sound.h"
#include "psx/pad.h"
#include "psx/types.h"

void equip_input_update_with_message_state(void) {
    s32 running;
    s32 frame;
    u8 state;
    s32 delay;

    running = 0;
    equip_update_controller_input();
    frame = equip_gfx_get_transition_frame();
    if (frame >= 1 && frame <= 3) {
        equip_input_clear_state();
    } else {
        running = battle_thread_is_running(1);
        if (running != 0) {
            equip_input_clear_state();
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
        g_equip_input_primary_repeat &= ~PSX_PAD_L2;
        g_equip_input_secondary_repeat &= ~PSX_PAD_L2;
        g_equip_input_newly_pressed &= ~PSX_PAD_L2;
    }
    if (g_equip_input_primary_repeat != 0) {
        g_equip_input_lock_timer = 5;
    }
    if (running == 0) {
        g_equip_text_message_thread_active = 0;
        g_event_mode = 0;
        g_equip_sound_suppress_queued = 0;
    }
    if (g_equip_text_thread_running_state == 1) {
        g_equip_sound_queued_effect_id = MAIN_SFX_WINDOW_OPEN;
    }
}
