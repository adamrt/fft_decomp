/* Input. A small object between boundaries the binary proves. */

#include "fft/world.h"
#include "psx/pad.h"

s32 world_input_read_controller(s32 unused) {
    return PadRead(0);
}

/*
 * Publish newly pressed buttons and two repeat rates for WORLD menus.
 *
 * The 0x10 input uses a separate edge latch. Other inputs share CARD's
 * activation delay, single-button latch, and repeat-counter logic. Counter
 * slots 4..11 are skipped; those buttons use only the newly pressed mask.
 */
void world_input_update_controller(void) {
    s16 bit;
    s32 index;
    u32 current_input;
    u32 filtered_input;
    s32* counter_base;
    u32 speed, initial_delay, secondary_repeat_period, repeat_period;
    speed = menu_get_event_speed();
    initial_delay = g_main_input_repeat_initial_delay / speed;
    repeat_period = g_main_input_repeat_period / speed;
    secondary_repeat_period = g_main_input_secondary_repeat_period / speed;
    current_input = world_input_read_controller(0);
    if (current_input & PSX_PAD_TRIANGLE) {
        if (g_world_input_triangle_latch != 0) {
            menu_input_clear_state();
        } else {
            g_world_input_triangle_latch = 1;
            g_menu_input_newly_pressed = PSX_PAD_TRIANGLE;
            g_menu_input_secondary_repeat = PSX_PAD_TRIANGLE;
            g_menu_input_primary_repeat = PSX_PAD_TRIANGLE;
        }
    } else {
        g_world_input_triangle_latch = 0;
        if (current_input & g_world_input_latched_button) {
            current_input &= g_world_input_latched_button;
        } else if (current_input == 0) {
            g_world_input_latched_button = 0;
            g_input_activation_timer += speed;
        } else {
            if (g_input_activation_timer < 4) {
                if (current_input & PSX_PAD_DPAD_MASK) {
                    current_input &= PSX_PAD_DPAD_MASK;
                    g_input_activation_timer = 10;
                }
            }
            if (g_input_activation_timer >= 4) {
                g_input_activation_timer = 0;
                for (index = 0; index < 16; index++) {
                    bit = 1U << index;
                    if (current_input & bit) {
                        current_input &= bit;
                        g_world_input_latched_button = bit;
                    }
                }
            } else {
                current_input = 0;
                g_world_input_latched_button = 0;
            }
        }
        filtered_input = current_input;
        if (g_input_activation_timer >= 10) {
            g_input_activation_timer = 10;
        }
        g_menu_input_newly_pressed = filtered_input;
        for (index = 0; index < 16; index++) {
            s32 mask = 1U << index;

            if (mask & g_menu_input_newly_pressed) {
                if (mask & g_world_input_previous) {
                    g_menu_input_newly_pressed &= ~mask;
                } else {
                    g_world_input_previous |= mask;
                }
            } else {
                g_world_input_previous &= ~mask;
            }
        }
        counter_base = g_input_repeat_counters;
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
                || (counter_base[index] >= 2 && counter_base[index] < initial_delay)) {
                current_input &= ~(1U << index);
            }
            if (counter_base[index] >= initial_delay) {
                current_input |= 1U << (index + 16);
            }
            if ((counter_base[index] >= initial_delay && counter_base[index] % secondary_repeat_period != 0)
                || (counter_base[index] >= 2 && counter_base[index] < initial_delay << 1)) {
                filtered_input &= ~(1U << index);
            }
        }
        current_input &= 0xf00ff00f;
        filtered_input &= 0xf00ff00f;
        current_input |= g_menu_input_newly_pressed & 0x0ff0;
        filtered_input |= g_menu_input_newly_pressed & 0x0ff0;
        g_menu_input_primary_repeat = current_input;
        g_menu_input_secondary_repeat = filtered_input;
    }
    g_world_input_primary_repeat_latched = g_menu_input_primary_repeat;
    g_world_input_secondary_repeat_latched = g_menu_input_secondary_repeat;
}
