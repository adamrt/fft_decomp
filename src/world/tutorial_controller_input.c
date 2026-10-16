/* Tutorial controller input and input with message state. */

#include "fft/help.h"
#include "fft/input.h"
#include "fft/menu.h"
#include "fft/menu_input.h"
#include "fft/menu_window.h"
#include "fft/screen.h"
#include "fft/sound.h"
#include "fft/thread.h"
#include "fft/tutorial.h"
#include "psx/pad.h"
#include "psx/types.h"

static u32 g_tutorial_repeat_counters[16] = { 0 };
static u16 g_tutorial_previous_input = 0;

/*
 * Publish newly pressed buttons and two repeat rates, reading tutorial input.
 *
 * Twin of screen_input_update_pad (card_save.c) with its own edge latch,
 * single-button latch, activation timer, previous-state and counter arrays;
 * the secondary repeat period is two frames longer.
 *
 * Triangle reports one press and nothing while held. Otherwise one button is
 * reported at a time: of several, the lowest button bit is latched and
 * reported alone until it is released. After a release, a new press counts only once
 * the activation timer reaches 4 (VSyncs), except a D-pad press, which counts
 * at once.
 */
static void tutorial_update_input(void) {
    s16 bit;
    s32 index;
    u32 current_input;
    u32 filtered_input;
    u32* counter_base;
    u32 speed;
    u32 initial_delay;
    u32 secondary_repeat_period;
    u32 repeat_period;

    speed = menu_get_event_speed();
    initial_delay = g_input_repeat_initial_delay / speed;
    repeat_period = g_input_repeat_period / speed;
    secondary_repeat_period = g_input_secondary_repeat_period / speed + 2;
    current_input = screen_input_read(0);
    if (current_input & PSX_PAD_TRIANGLE) {
        if (g_screen_input_triangle_latched != 0) {
            menu_input_clear_state();
        } else {
            g_screen_input_triangle_latched = 1;
            g_menu_input_newly_pressed = PSX_PAD_TRIANGLE;
            g_menu_input_secondary_repeat = PSX_PAD_TRIANGLE;
            g_menu_input_primary_repeat = PSX_PAD_TRIANGLE;
        }
    } else {
        g_screen_input_triangle_latched = 0;
        {
            u16 previous_latch = g_tutorial_latched_button;
            u32 latched_input = current_input & previous_latch;
            if (latched_input != 0) {
                current_input = latched_input;
            } else if (current_input == 0) {
                g_tutorial_latched_button = 0;
                g_tutorial_activation_timer += speed;
            } else {
                if (g_tutorial_activation_timer < 4) {
                    u32 priority_input = current_input & PSX_PAD_DPAD_MASK;

                    if (priority_input != 0) {
                        current_input = priority_input;
                        g_tutorial_activation_timer = 10;
                    }
                }
                if (g_tutorial_activation_timer >= 4) {
                    g_tutorial_activation_timer = 0;
                    index = 0;
                    {
                        s32 bit_base = 1;
                        do {
                            u32 matched_input;
                            bit = bit_base << index;
                            matched_input = current_input & bit;
                            index++;
                            if (matched_input != 0) {
                                current_input = matched_input;
                                g_tutorial_latched_button = bit;
                            }
                        } while (index < 16);
                    }
                } else {
                    current_input = 0;
                    g_tutorial_latched_button = 0;
                }
            }
        }
        filtered_input = current_input;
        if (g_tutorial_activation_timer >= 10) {
            g_tutorial_activation_timer = 10;
        }
        g_menu_input_newly_pressed = filtered_input;
        for (index = 0; index < 16; index++) {
            s32 mask = 1U << index;

            if (mask & g_menu_input_newly_pressed) {
                if (mask & g_tutorial_previous_input) {
                    g_menu_input_newly_pressed &= ~mask;
                } else {
                    g_tutorial_previous_input |= mask;
                }
            } else {
                g_tutorial_previous_input &= ~mask;
            }
        }
        /* Auto-repeat for the shoulder buttons (bits 0-3) and the D-pad (bits 12-15); the
         * face buttons, Select and Start (bits 4-11) skip the counters and report only new
         * presses. Bits 16-31 flag a button held past the initial delay. */
        counter_base = g_tutorial_repeat_counters;
        for (index = 0; index < 16; index++) {
            if (index == 4) {
                index = 12;
            }
            if (current_input & (1U << index)) {
                counter_base[index]++;
            } else {
                counter_base[index] = 0;
            }
            if ((counter_base[index] >= initial_delay && counter_base[index] % repeat_period != 0)
                || ((s32)counter_base[index] >= 2 && counter_base[index] < initial_delay)) {
                current_input &= ~(1U << index);
            }
            if (counter_base[index] >= initial_delay) {
                current_input |= 1U << (index + 16);
            }
            if ((counter_base[index] >= initial_delay && counter_base[index] % secondary_repeat_period != 0)
                || ((s32)counter_base[index] >= 2 && counter_base[index] < initial_delay << 1)) {
                filtered_input &= ~(1U << index);
            }
        }
        {
            u32 newly_pressed = g_menu_input_newly_pressed & 0x0ff0;
            current_input &= 0xf00ff00f;
            filtered_input &= 0xf00ff00f;
            current_input |= newly_pressed;
            filtered_input |= newly_pressed;
            g_menu_input_primary_repeat = current_input;
            g_menu_input_secondary_repeat = filtered_input;
        }
    }
    g_screen_input_primary_repeat_latched = g_menu_input_primary_repeat;
    g_screen_input_secondary_repeat_latched = g_menu_input_secondary_repeat;
}

/*
 * Per-frame input update of the formation, shop and name-entry screens.
 *
 * Reads the pad (through the tutorial input reader), then clears the 32-bit
 * input masks during steps 1..3 of the window scale animation, while the text
 * thread runs or a help grid is open, on the frame the text ends and during a
 * screen fade. The text phase goes 1 on the first frame of a text task (which
 * queues the window-open sound), 2 after it and 0 otherwise. When the text
 * thread is not running, the input lock and the held-loop sound release are
 * cleared.
 */
void menu_input_update_with_message_state(void) {
    s32 running = 0;
    s32 step;
    u8 value;

    tutorial_update_input();
    step = menu_get_window_scale_step();
    if (step >= 1 && step <= 3) {
        menu_input_clear_state();
    } else {
        running = thread_is_running(THREAD_SCREEN_SLOT_TEXT);
        if (running != 0 || g_screen_grid_id != MENU_GRID_HELP_NONE) {
            menu_input_clear_state();
            if (running != 0 && g_menu_input_lock_mode != 0) {
                value = 1;
                if (g_screen_text_task_phase != 0) {
                    value = 2;
                }
                g_screen_text_task_phase = value;
            } else {
                g_screen_text_task_phase = 0;
            }
        } else if (g_screen_text_task_phase != 0 || screen_fade_get_state() != SCREEN_FADE_STATE_NONE) {
            g_screen_text_task_phase = 0;
            menu_input_clear_state();
        }
    }
    if (running == 0) {
        g_menu_input_lock_mode = 0;
        g_screen_sound_release_held_loop = 0;
    }
    if (g_screen_text_task_phase == 1) {
        g_sound_queued_effect_id = SOUND_SFX_WINDOW_OPEN;
    }
}
