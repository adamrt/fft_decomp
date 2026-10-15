#include "fft/event_bunit.h"
#include "fft/menu.h"
#include "psx/pad.h"
#include "psx/types.h"

/*
 * Publish newly pressed buttons and two repeat rates for BUNIT menus.
 *
 * CARD's latch/activation/repeat logic with a slower secondary repeat
 * (period + 2) and a soft reset on L1+R1+Start+Select. Both masks are
 * also mirrored to g_bunit_input_primary_repeat/g_bunit_input_secondary_repeat.
 */
void bunit_input_update_controller(void) {
    s16 bit;
    s32 index;
    u32 current_input;
    u32 filtered_input;
    s32* counter_base;
    u32 speed, initial_delay, secondary_repeat_period, repeat_period;
    speed = menu_get_event_speed();
    initial_delay = g_main_input_repeat_initial_delay / speed;
    repeat_period = g_main_input_repeat_period / speed;
    secondary_repeat_period = g_main_input_secondary_repeat_period / speed + 2;
    current_input = PadRead(0);
    if (current_input == (PSX_PAD_L1 | PSX_PAD_R1 | PSX_PAD_SELECT | PSX_PAD_START)) {
        main_system_reset_game();
    }
    if (current_input & g_bunit_input_latched_button) {
        current_input &= g_bunit_input_latched_button;
    } else if (current_input == 0) {
        g_bunit_input_latched_button = 0;
        g_bunit_input_activation_timer += speed;
    } else {
        if (g_bunit_input_activation_timer < 4) {
            if (current_input & (PSX_PAD_UP | PSX_PAD_RIGHT | PSX_PAD_DOWN | PSX_PAD_LEFT)) {
                current_input &= PSX_PAD_UP | PSX_PAD_RIGHT | PSX_PAD_DOWN | PSX_PAD_LEFT;
                g_bunit_input_activation_timer = 10;
            }
        }
        if (g_bunit_input_activation_timer >= 4) {
            g_bunit_input_activation_timer = 0;
            for (index = 0; index < 16; index++) {
                bit = 1 << index;
                if (current_input & bit) {
                    current_input &= bit;
                    g_bunit_input_latched_button = bit;
                }
            }
        } else {
            current_input = 0;
            g_bunit_input_latched_button = 0;
        }
    }
    filtered_input = current_input;
    if (g_bunit_input_activation_timer >= 10) {
        g_bunit_input_activation_timer = 10;
    }
    g_menu_input_newly_pressed = filtered_input;
    for (index = 0; index < 16; index++) {
        s32 mask = 1 << index;

        if (mask & g_menu_input_newly_pressed) {
            if (mask & g_bunit_input_previous) {
                g_menu_input_newly_pressed &= ~mask;
            } else {
                g_bunit_input_previous |= mask;
            }
        } else {
            g_bunit_input_previous &= ~mask;
        }
    }
    counter_base = g_bunit_input_repeat_counters;
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
    g_bunit_input_primary_repeat = current_input;
    g_bunit_input_secondary_repeat = filtered_input;
}
