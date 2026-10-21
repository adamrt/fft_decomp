/* BUNIT's pad latch and repeat masks feed its per-frame help and fade input lock. */

#include "fft/input.h"
#include "fft/gfx_primitive.h"
#include "fft/help.h"
#include "fft/menu.h"
#include "fft/menu_input.h"
#include "fft/sound.h"
#include "fft/system.h"
#include "fft/thread.h"
#include "fft/unit_list.h"
#include "psx/libetc.h"
#include "psx/pad.h"
#include "psx/types.h"

/* controller input */
static u16 g_unit_list_input_latched_button;
extern u16 g_unit_list_input_previous;

/* help text */
extern u8 g_unit_list_help_text_state;
/*
 * Publish newly pressed buttons and two repeat rates for BUNIT menus.
 *
 * CARD's latch/activation/repeat logic with a slower secondary repeat
 * (period + 2) and a soft reset on L1+R1+Start+Select. Both masks are
 * also mirrored to g_unit_list_input_primary_repeat/g_unit_list_input_secondary_repeat.
 */
static void input_update_controller(void) {
    s16 bit;
    s32 index;
    u32 current_input;
    u32 filtered_input;
    s32* counter_base;
    u32 speed, initial_delay, secondary_repeat_period, repeat_period;
    speed = menu_get_event_speed();
    initial_delay = g_input_repeat_initial_delay / speed;
    repeat_period = g_input_repeat_period / speed;
    secondary_repeat_period = g_input_secondary_repeat_period / speed + 2;
    current_input = PadRead(0);
    if (current_input == (PSX_PAD_L1 | PSX_PAD_R1 | PSX_PAD_SELECT | PSX_PAD_START)) {
        system_game_reset();
    }
    if (current_input & g_unit_list_input_latched_button) {
        current_input &= g_unit_list_input_latched_button;
    } else if (current_input == 0) {
        g_unit_list_input_latched_button = 0;
        g_input_activation_timer += speed;
    } else {
        if (g_input_activation_timer < 4 && (current_input & PSX_PAD_DPAD_MASK)) {
            current_input &= PSX_PAD_DPAD_MASK;
            g_input_activation_timer = 10;
        }
        if (g_input_activation_timer >= 4) {
            g_input_activation_timer = 0;
            for (index = 0; index < 16; index++) {
                bit = 1 << index;
                if (current_input & bit) {
                    current_input &= bit;
                    g_unit_list_input_latched_button = bit;
                }
            }
        } else {
            current_input = 0;
            g_unit_list_input_latched_button = 0;
        }
    }
    filtered_input = current_input;
    if (g_input_activation_timer >= 10) {
        g_input_activation_timer = 10;
    }
    g_menu_input_newly_pressed = filtered_input;
    for (index = 0; index < 16; index++) {
        s32 mask = 1 << index;

        if (mask & g_menu_input_newly_pressed) {
            if (mask & g_unit_list_input_previous) {
                g_menu_input_newly_pressed &= ~mask;
            } else {
                g_unit_list_input_previous |= mask;
            }
        } else {
            g_unit_list_input_previous &= ~mask;
        }
    }
    counter_base = g_input_repeat_counters;
    for (index = 0; index < 16; index++) {
        if (index == 4) {
            index = 12;
        }
        if (current_input & (1 << index)) {
            counter_base[index]++;
        } else {
            counter_base[index] = 0;
        }
        if ((counter_base[index] >= initial_delay && counter_base[index] % repeat_period != 0)
            || (counter_base[index] >= 2 && counter_base[index] < initial_delay)) {
            current_input &= ~(1 << index);
        }
        if (counter_base[index] >= initial_delay) {
            current_input |= 1U << (index + 16);
        }
        if ((counter_base[index] >= initial_delay && counter_base[index] % secondary_repeat_period != 0)
            || (counter_base[index] >= 2 && counter_base[index] < initial_delay << 1)) {
            filtered_input &= ~(1 << index);
        }
    }
    /* Only the shoulder buttons and the d-pad repeat (with their repeat flags in bits 16
     * and up); the face buttons, Select and Start count only when newly pressed. */
    current_input &= 0xf00ff00f;
    filtered_input &= 0xf00ff00f;
    current_input |= g_menu_input_newly_pressed & 0x0ff0;
    filtered_input |= g_menu_input_newly_pressed & 0x0ff0;
    g_menu_input_primary_repeat = current_input;
    g_menu_input_secondary_repeat = filtered_input;
    g_unit_list_input_primary_repeat = current_input;
    g_unit_list_input_secondary_repeat = filtered_input;
}

/*
 * Per-frame input update of the BUNIT screen.
 *
 * Reads the pad, then clears the 32-bit input values while the text thread runs or a
 * help screen is open, on the frame the text ends and during a screen fade.
 * The help text state goes 1 on the text thread's first frame (which queues
 * the window-open sound), 2 after it and 0 otherwise.
 */
void unit_list_update_input(void) {
    s32 running;

    input_update_controller();
    running = thread_is_running(THREAD_SCREEN_SLOT_TEXT);

    if (running != 0 || g_unit_list_help_screen_id != MENU_GRID_HELP_NONE) {
        menu_input_clear_state();
        if (running != 0) {
            g_unit_list_help_text_state = (g_unit_list_help_text_state != 0) ? 2 : 1;
        } else {
            g_unit_list_help_text_state = 0;
        }
    } else if (g_unit_list_help_text_state != 0 || gfx_get_fade_state() != 0) {
        g_unit_list_help_text_state = 0;
        menu_input_clear_state();
    }

    if (running == 0) {
        g_menu_input_lock_mode = 0;
    }
    if (g_unit_list_help_text_state == 1) {
        g_sound_queued_effect_id = SOUND_SFX_WINDOW_OPEN;
    }
}
