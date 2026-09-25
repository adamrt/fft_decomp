#include "fft/event_bunit.h"
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
    s32 was_before_initial_delay;
    s32 index;
    s32 repeat_enabled;
    s32 secondary_repeat_enabled;
    u32 current_input;
    u32 filtered_input;
    u32* counter;
    u32* counter_base;
    u32 counter_value;
    u32 long_counter_value;
    u32 long_press_mask;
    u32 speed, initial_delay, secondary_repeat_period, repeat_period;
    speed = bunit_menu_get_event_speed();
    initial_delay = g_main_input_repeat_initial_delay / speed;
    repeat_period = g_main_input_repeat_period / speed;
    secondary_repeat_period = g_main_input_secondary_repeat_period / speed + 2;
    current_input = PadRead(0);
    if (current_input == (PSX_PAD_L1 | PSX_PAD_R1 | PSX_PAD_SELECT | PSX_PAD_START)) {
        main_system_reset_game();
    }
    {
        u16 previous_latch = g_bunit_input_latched_button;
        u32 latched_input = current_input & previous_latch;
        if (latched_input != 0) {
            current_input = latched_input;
        } else if (current_input == 0) {
            g_bunit_input_latched_button = 0;
            g_bunit_input_activation_timer += speed;
        } else {
            if (g_bunit_input_activation_timer < 4) {
                u32 direction_input = current_input & (PSX_PAD_UP | PSX_PAD_RIGHT | PSX_PAD_DOWN | PSX_PAD_LEFT);

                if (direction_input != 0) {
                    current_input = direction_input;
                    g_bunit_input_activation_timer = 10;
                }
            }
            if (g_bunit_input_activation_timer >= 4) {
                g_bunit_input_activation_timer = 0;
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
                            g_bunit_input_latched_button = bit;
                        }
                    } while (index < 16);
                }
            } else {
                current_input = 0;
                g_bunit_input_latched_button = 0;
            }
        }
    }
    filtered_input = current_input;
    if (g_bunit_input_activation_timer >= 10) {
        g_bunit_input_activation_timer = 10;
    }
    g_bunit_menu_input_mask = filtered_input;
    for (index = 0; index < 16; index++) {
        s32 mask = 1 << index;

        if (mask & g_bunit_menu_input_mask) {
            if (mask & g_bunit_input_previous) {
                g_bunit_menu_input_mask &= ~mask;
            } else {
                g_bunit_input_previous |= mask;
            }
        } else {
            g_bunit_input_previous &= ~mask;
        }
    }
    /* Label form of the twins' repeat test
     * `(n >= delay && n % period != 0) || (n >= 2 && n < delay)`: written that
     * way here the loop is too short for loop.c's threshold and it hoists
     * the `index == 4` constant, which the target reloads each pass. */
    counter_base = (u32*)g_bunit_input_repeat_counters;
    index = 0;
    counter = counter_base;
    do {
        if (index == 4) {
            counter = counter_base + 12;
            index = 12;
        }
        repeat_enabled = 1 << index;
        if ((current_input & repeat_enabled) != 0) {
            (*counter)++;
        } else {
            *counter = 0;
        }
        counter_value = *counter;
        was_before_initial_delay = counter_value < initial_delay;
        repeat_enabled = (s32)counter_value < 2;
        if (was_before_initial_delay == 0) {
            if ((counter_value % repeat_period) == 0) {
                /* Hides counter_value so the `< 2` test is recomputed here. */
                __asm__("" : "=r"(counter_value) : "0"(counter_value));
                repeat_enabled = (s32)counter_value < 2;
                goto test_primary_repeat;
            }
            secondary_repeat_enabled = 1 << index;
            goto clear_primary_repeat;
        }
    test_primary_repeat:
        if (repeat_enabled == 0) {
            secondary_repeat_enabled = 1 << index;
            if (was_before_initial_delay != 0) {
            clear_primary_repeat:
                current_input &= ~secondary_repeat_enabled;
                goto test_initial_delay;
            }
            goto set_long_press;
        }
    test_initial_delay:
        if (*counter >= initial_delay) {
        set_long_press:
            long_press_mask = 1 << (index + 16);
            /* Keeps the counter reload after the long-press shift. */
            __asm__("" : "=r"(long_press_mask) : "0"(long_press_mask));
            current_input |= long_press_mask;
            long_counter_value = *counter;
            if (long_counter_value >= initial_delay) {
                if ((long_counter_value % secondary_repeat_period) == 0) {
                    goto test_secondary_repeat;
                }
                secondary_repeat_enabled = 1 << index;
                goto clear_secondary_repeat;
            }
            repeat_enabled = (s32)long_counter_value < 2;
            goto test_secondary_counter;
        }
    test_secondary_repeat:
        long_counter_value = *counter;
        repeat_enabled = (s32)long_counter_value < 2;
    test_secondary_counter:
        if (repeat_enabled == 0) {
            s32 below_double_delay = long_counter_value < (initial_delay << 1);
            if (below_double_delay != 0) {
                secondary_repeat_enabled = 1 << index;
            clear_secondary_repeat:
                filtered_input &= ~secondary_repeat_enabled;
            }
        }
        index++;
        counter++;
    } while (index < 16);
    current_input &= 0xf00ff00f;
    filtered_input &= 0xf00ff00f;
    current_input |= g_bunit_menu_input_mask & 0x0ff0;
    filtered_input |= g_bunit_menu_input_mask & 0x0ff0;
    g_bunit_menu_input_active_mask = current_input;
    g_bunit_menu_input_repeat_mask = filtered_input;
    g_bunit_input_primary_repeat = current_input;
    g_bunit_input_secondary_repeat = filtered_input;
}
