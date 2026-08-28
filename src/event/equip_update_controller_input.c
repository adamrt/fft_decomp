#include "fft/equip.h"
#include "fft/main_runtime.h"
#include "psx/pad.h"
#include "psx/types.h"

/*
 * Publish newly pressed buttons and two repeat rates for EQUIP menus.
 *
 * Same latch/activation/repeat logic as world_input_update_controller and
 * card_input_update_controller, with the frame-speed divisor fixed at 2.
 * The divisor stays a variable: the target divides by a register holding 2
 * rather than shifting.
 */
void equip_update_controller_input(void) {
    s16 bit;
    s32 index;
    /* Pinned: unpinned, GCC gives filtered_input and the hoisted 1 << index
     * constant each other's register ($t1/$t2). */
    register u32 current_input asm("$8");
    register u32 filtered_input asm("$9");
    u32* counter;
    u32* counter_base;
    u32 speed, initial_delay, secondary_repeat_period, repeat_period;
    speed = 2;
    initial_delay = g_main_input_repeat_initial_delay / speed;
    repeat_period = g_main_input_repeat_period / speed;
    secondary_repeat_period = g_main_input_secondary_repeat_period / speed;
    current_input = PadRead(0);
    {
        u16 previous_latch = g_equip_input_latched_button;
        u32 latched_input = current_input & previous_latch;
        if (latched_input != 0) {
            current_input = latched_input;
        } else if (current_input == 0) {
            g_equip_input_latched_button = 0;
            g_equip_input_activation_timer += speed;
        } else {
            if (g_equip_input_activation_timer < 4) {
                u32 priority_input = current_input & (PSX_PAD_UP | PSX_PAD_RIGHT | PSX_PAD_DOWN | PSX_PAD_LEFT);

                if (priority_input != 0) {
                    current_input = priority_input;
                    g_equip_input_activation_timer = 10;
                }
            }
            if (g_equip_input_activation_timer >= 4) {
                g_equip_input_activation_timer = 0;
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
                            g_equip_input_latched_button = bit;
                        }
                    } while (index < 16);
                }
            } else {
                current_input = 0;
                g_equip_input_latched_button = 0;
            }
        }
    }
    filtered_input = current_input;
    if (g_equip_input_activation_timer >= 10) {
        g_equip_input_activation_timer = 10;
    }
    g_equip_input_newly_pressed = filtered_input;
    for (index = 0; index < 16; index++) {
        s32 mask = 1 << index;

        if (mask & g_equip_input_newly_pressed) {
            if (mask & g_equip_input_previous) {
                g_equip_input_newly_pressed &= ~mask;
            } else {
                g_equip_input_previous |= mask;
            }
        } else {
            g_equip_input_previous &= ~mask;
        }
    }
    counter_base = (u32*)g_equip_input_repeat_counters;
    index = 0;
    counter = counter_base;
    do {
        if (index == 4) {
            counter = counter_base + 12;
            index = 12;
        }
        if (current_input & (1 << index)) {
            (*counter)++;
        } else {
            *counter = 0;
        }
        if ((*counter >= initial_delay && *counter % repeat_period != 0)
            || ((s32)*counter >= 2 && *counter < initial_delay)) {
            current_input &= ~(1 << index);
        }
        if (*counter >= initial_delay) {
            current_input |= 1 << (index + 16);
        }
        if ((*counter >= initial_delay && *counter % secondary_repeat_period != 0)
            || ((s32)*counter >= 2 && *counter < initial_delay << 1)) {
            filtered_input &= ~(1 << index);
        }
        index++;
        counter++;
    } while (index < 16);
    {
        u32 newly_pressed = g_equip_input_newly_pressed & 0x0ff0;
        g_equip_input_primary_repeat = (current_input & 0xf00ff00f) | newly_pressed;
        g_equip_input_secondary_repeat = (filtered_input & 0xf00ff00f) | newly_pressed;
    }
}
