#include "fft/event_card.h"
#include "fft/menu.h"
#include "psx/pad.h"
#include "psx/types.h"

/*
 * Read pad 0 and publish the newly pressed buttons and two repeat rates for
 * the CARD menus.
 *
 * One button at a time is latched: once the idle counter (advanced by the
 * event speed each idle frame) reaches 4 the lowest pressed bit wins, and a
 * d-pad press is accepted at once. A held button
 * repeats once its counter reaches the initial delay, every repeat period in
 * the primary 32-bit value (which also raises bit 16 + n while repeating) and every
 * secondary period, after twice the delay, in the secondary 32-bit value. All three
 * periods are divided by the event speed. Counter slots 4..11 are skipped;
 * those buttons use only the newly pressed mask.
 */
void card_input_update_controller(void) {
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
    current_input = PadRead(0);
    if (current_input & g_card_input_latched_button) {
        current_input &= g_card_input_latched_button;
    } else if (current_input == 0) {
        g_card_input_latched_button = 0;
        g_input_activation_timer += speed;
    } else {
        if (g_input_activation_timer < 4) {
            if (current_input & (PSX_PAD_UP | PSX_PAD_RIGHT | PSX_PAD_DOWN | PSX_PAD_LEFT)) {
                current_input &= PSX_PAD_UP | PSX_PAD_RIGHT | PSX_PAD_DOWN | PSX_PAD_LEFT;
                g_input_activation_timer = 10;
            }
        }
        if (g_input_activation_timer >= 4) {
            g_input_activation_timer = 0;
            for (index = 0; index < 16; index++) {
                bit = 1 << index;
                if (current_input & bit) {
                    current_input &= bit;
                    g_card_input_latched_button = bit;
                }
            }
        } else {
            current_input = 0;
            g_card_input_latched_button = 0;
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
            if (mask & g_card_input_previous) {
                g_menu_input_newly_pressed &= ~mask;
            } else {
                g_card_input_previous |= mask;
            }
        } else {
            g_card_input_previous &= ~mask;
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
            current_input |= 1 << (index + 16);
        }
        if ((counter_base[index] >= initial_delay && counter_base[index] % secondary_repeat_period != 0)
            || (counter_base[index] >= 2 && counter_base[index] < initial_delay << 1)) {
            filtered_input &= ~(1 << index);
        }
    }
    current_input &= 0xf00ff00f;
    filtered_input &= 0xf00ff00f;
    current_input |= g_menu_input_newly_pressed & 0x0ff0;
    filtered_input |= g_menu_input_newly_pressed & 0x0ff0;
    g_menu_input_primary_repeat = current_input;
    g_menu_input_secondary_repeat = filtered_input;
}
